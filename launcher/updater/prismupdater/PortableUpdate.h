// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "LinuxPortable.h"
#include <QDirIterator>
#include <QTemporaryDir>
#include <QSet>
#include <archive.h>
#include <archive_entry.h>
#include <set>

namespace PortableUpdate {
using Progress = std::function<void(const QString&, qint64, qint64)>;
struct Result { bool success = false; QString error; QString backup; QStringList conflicts; };
inline QStringList records(const QJsonObject& data)
{
    QStringList paths;
    for (const auto value : data.value("files").toArray()) {
        const auto record = value.toObject();
        const auto path = record.value("path").toString();
        if (!LinuxPortable::safePath(path) || LinuxPortable::userPath(path) || paths.contains(path) || path == ".luna-portable.json") return {};
        paths.append(path);
    }
    if (!paths.contains("AppRun") || !paths.contains("usr/bin/lunalauncher") || !paths.contains("usr/bin/lunalauncher_updater")) return {};
    return paths;
}
inline bool exists(const QString& path) { const QFileInfo f(path); return f.exists() || f.isSymLink(); }
inline bool insideLink(const QString& root, const QString& path)
{
    const auto destination = QFileInfo(QDir(root).filePath(path)).canonicalFilePath();
    return destination.startsWith(QDir(root).absolutePath() + '/') && QFileInfo(destination).exists();
}
inline bool validate(const QString& root, const QJsonObject& data, QString& error)
{
    const auto paths = records(data);
    if (paths.isEmpty()) { error = "Invalid portable program file manifest."; return false; }
    QSet<QString> expected(paths.begin(), paths.end()); expected.insert(".luna-portable.json");
    QDirIterator it(root, QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const auto path = it.next(); const QFileInfo info(path);
        if (info.isDir() && !info.isSymLink()) continue;
        if (!expected.contains(QDir(root).relativeFilePath(path))) { error = "Unexpected file in portable package: " + path; return false; }
    }
    for (const auto value : data.value("files").toArray()) {
        const auto record = value.toObject();
        const auto relative = record.value("path").toString();
        const auto path = QDir(root).filePath(relative);
        const QFileInfo info(path);
        if (record.contains("link")) {
            if (!info.isSymLink() || !insideLink(root, relative) ||
                info.symLinkTarget() != QDir::cleanPath(info.absolutePath() + '/' + record.value("link").toString())) {
                error = "Invalid portable symbolic link: " + relative; return false;
            }
        } else if (info.isSymLink() || !info.isFile() || !ReleaseUpdate::verifyPackage(path, record, true)) {
            error = "Portable file checksum or size mismatch: " + relative; return false;
        }
    }
    return true;
}
#ifdef Q_OS_LINUX
inline bool extract(const QString& source, const QString& destination, qint64 limit, QString& error, const Progress& progress)
{
    auto* reader = archive_read_new();
    const auto cleanup = qScopeGuard([&] { archive_read_free(reader); });
    archive_read_support_filter_gzip(reader); archive_read_support_format_tar(reader);
    if (archive_read_open_filename(reader, QFile::encodeName(source).constData(), 1024 * 1024) != ARCHIVE_OK) {
        error = QString::fromUtf8(archive_error_string(reader)); return false;
    }
    QSet<QString> names; QList<QPair<QString, QString>> links;
    qint64 done = 0; archive_entry* entry = nullptr; int code;
    while ((code = archive_read_next_header(reader, &entry)) == ARCHIVE_OK) {
        auto name = QString::fromUtf8(archive_entry_pathname_utf8(entry));
        const auto type = archive_entry_filetype(entry);
        if (type == AE_IFDIR && name.endsWith('/')) name.chop(1);
        if (!LinuxPortable::safePath(name) || LinuxPortable::userPath(name) || names.contains(name) || names.size() >= 20000 || archive_entry_hardlink(entry)) {
            error = "Unsafe or duplicate portable archive path: " + name; return false;
        }
        names.insert(name);
        const auto path = QDir(destination).filePath(name);
        if (type == AE_IFDIR) {
            if (!QDir().mkpath(path)) { error = "Cannot create package directory: " + name; return false; }
        } else if (type == AE_IFLNK) {
            const auto link = QString::fromUtf8(archive_entry_symlink_utf8(entry));
            const auto resolved = QDir::cleanPath(QFileInfo(path).absolutePath() + '/' + link);
            if (link.isEmpty() || QDir::isAbsolutePath(link) || !resolved.startsWith(destination + '/')) {
                error = "Escaping portable symbolic link: " + name; return false;
            }
            links.append({name, link});
        } else if (type == AE_IFREG) {
            const auto size = archive_entry_size(entry);
            if (size < 0 || size > limit - done || !QDir().mkpath(QFileInfo(path).absolutePath())) {
                error = "Portable archive exceeds its unpacked size or cannot create a directory."; return false;
            }
            QFile file(path);
            if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly)) { error = file.errorString(); return false; }
            char block[1024 * 1024]; qint64 written = 0; la_ssize_t count;
            while ((count = archive_read_data(reader, block, sizeof(block))) > 0) {
                if (count > size - written || file.write(block, count) != count) { error = "Failed to unpack portable file: " + name; return false; }
                written += count; done += count;
                if (progress) progress("Unpacking " + name, done, limit);
            }
            if (count < 0 || written != size || !file.flush() || ::fchmod(file.handle(), archive_entry_perm(entry) & 0777) || ::fsync(file.handle())) {
                error = "Could not save complete portable file: " + name; return false;
            }
        } else { error = "Unsupported special file in portable archive: " + name; return false; }
    }
    if (code != ARCHIVE_EOF) { error = "Truncated or invalid portable archive."; return false; }
    // Create links last so archive entries can never write through a symlink.
    for (const auto& link : links) {
        const auto path = QDir(destination).filePath(link.first);
        auto parent = QFileInfo(path).absolutePath();
        while (parent != destination && parent.startsWith(destination + '/')) {
            if (QFileInfo(parent).isSymLink()) { error = "Symbolic link used as an archive parent."; return false; }
            parent = QFileInfo(parent).absolutePath();
        }
        if (exists(path) || !QDir().mkpath(QFileInfo(path).absolutePath()) ||
            ::symlink(QFile::encodeName(link.second).constData(), QFile::encodeName(path).constData())) {
            error = "Could not create portable symbolic link: " + link.first; return false;
        }
    }
    for (const auto& link : links)
        if (!insideLink(destination, link.first)) { error = "Broken or escaping portable symbolic link: " + link.first; return false; }
    return true;
}
inline bool copyProgram(const QString& source, const QString& destination, QString& error, const Progress& progress = {})
{
    const auto data = LinuxPortable::metadata(source);
    auto paths = records(data);
    if (paths.isEmpty()) { error = "The installed portable program manifest is missing or invalid."; return false; }
    paths.append(".luna-portable.json");
    qint64 done = 0;
    for (const auto& relative : paths) {
        const auto from = QDir(source).filePath(relative), to = QDir(destination).filePath(relative);
        if (!QDir().mkpath(QFileInfo(to).absolutePath())) { error = "Cannot prepare isolated updater directory."; return false; }
        const QFileInfo info(from);
        if (info.isSymLink()) {
            const auto link = QDir(info.absolutePath()).relativeFilePath(info.symLinkTarget());
            if (!info.symLinkTarget().startsWith(source + '/') || ::symlink(QFile::encodeName(link).constData(), QFile::encodeName(to).constData())) {
                error = "Cannot copy updater runtime link: " + relative; return false;
            }
        } else if (!info.isFile() || !QFile::copy(from, to)) { error = "Cannot copy updater runtime file: " + relative; return false; }
        if (progress) progress("Preparing isolated updater: " + relative, ++done, paths.size());
    }
    return validate(destination, data, error);
}
inline Result replace(const QString& target, const QString& archive, const QJsonObject& item, const QString& version,
                      const QString& dataRoot, const std::function<bool(const QString&)>& health, const Progress& progress = {})
{
    Result result;
    const auto original = LinuxPortable::metadata(target);
    const auto oldPaths = records(original);
    const QFileInfo info(target);
    if (!info.isAbsolute() || info.isSymLink() || !info.isDir() || oldPaths.isEmpty() ||
        info.canonicalFilePath() != QDir::cleanPath(target) ||
        QStringList{"/", "/usr", "/usr/local", "/opt", "/bin", "/sbin", "/lib", "/lib64", "/etc", "/home"}.contains(target)) {
        result.error = "The target is not a supported Luna portable directory."; return result;
    }
    QLockFile lock(target + ".update.lock");
    if (!lock.tryLock(0)) { result.error = "The portable directory is locked or its parent is not writable."; return result; }
    if (!ReleaseUpdate::verifyPackage(archive, item) || !LinuxPortable::compatible(item, LinuxPortable::architecture(), LinuxPortable::glibcVersion()) ||
        (original.value("neoUi").toBool() && !item.value("neoUi").toBool())) {
        result.error = "Portable archive checksum, size, architecture or glibc requirement does not match."; return result;
    }
    QTemporaryDir stage(info.absolutePath() + "/.luna-portable-stage-XXXXXX");
    if (!stage.isValid()) { result.error = "Cannot create a staging directory beside the portable installation."; return result; }
    if (!extract(archive, stage.path(), item.value("unpackedSize").toInteger(), result.error, progress)) return result;
    const auto next = LinuxPortable::metadata(stage.path());
    if (next.value("version").toString() != version || next.value("architecture") != item.value("architecture") ||
        next.value("neoUi") != item.value("neoUi") || next.value("minGlibc") != item.value("minGlibc") ||
        !validate(stage.path(), next, result.error)) {
        if (result.error.isEmpty()) result.error = "Portable payload metadata differs from the release manifest.";
        return result;
    }
    if (progress) progress("Verifying staged launcher", 0, 0);
    if (!health(stage.path())) { result.error = "New portable launcher failed the version health check; original kept."; return result; }
    const auto newPaths = records(next);
    const auto relativeData = QDir(target).relativeFilePath(QDir(dataRoot).absolutePath());
    if (LinuxPortable::safePath(relativeData) && relativeData != "usr") {
        for (const auto& path : newPaths)
            if (path == relativeData || path.startsWith(relativeData + '/')) {
                result.error = "The portable payload overlaps the configured user data directory."; return result;
            }
    }
    // Check the original marker again after staging, before moving any live data.
    if (LinuxPortable::metadata(target) != original) { result.error = "Portable installation changed while preparing the update."; return result; }
    result.backup = target + ".backup-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    stage.setAutoRemove(false); // No automatic deletion once user data can enter this directory.
    const auto staging = stage.path();
    if (!QDir().rename(target, result.backup)) {
        QDir(staging).removeRecursively(); result.error = "Cannot back up the portable installation; original kept."; return result;
    }
    QSet<QString> managed(oldPaths.begin(), oldPaths.end()); managed.insert(".luna-portable.json");
    QStringList moved;
    std::function<bool(const QString&)> preserve = [&](const QString& relative) {
        const auto from = QDir(result.backup).filePath(relative), to = QDir(staging).filePath(relative);
        const QFileInfo old(from);
        if (old.isDir() && !old.isSymLink()) {
            bool ownedChild = false;
            for (const auto& path : managed) if (path.startsWith(relative + '/')) { ownedChild = true; break; }
            if (ownedChild || exists(to)) {
                if (exists(to) && (!QFileInfo(to).isDir() || QFileInfo(to).isSymLink())) { result.conflicts.append(relative); return true; }
                for (const auto& entry : QDir(from).entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot))
                    if (!preserve(relative + '/' + entry)) return false;
                return true;
            }
        } else if (managed.contains(relative)) return true;
        if (exists(to)) { result.conflicts.append(relative); return true; }
        if (!QDir().mkpath(QFileInfo(to).absolutePath()) || !QDir().rename(from, to)) return false;
        moved.append(relative);
        if (progress) progress("Preserving user data: " + relative, moved.size(), 0);
        return true;
    };
    bool installed = false;
    const auto rollback = [&] {
        const auto current = installed ? target : staging;
        bool ok = true;
        for (auto it = moved.crbegin(); it != moved.crend(); ++it) {
            const auto originalPath = QDir(result.backup).filePath(*it);
            ok = QDir().mkpath(QFileInfo(originalPath).absolutePath()) &&
                QDir().rename(QDir(current).filePath(*it), originalPath) && ok;
        }
        // Keep all directories when data restoration failed, for manual recovery.
        if (ok && installed) ok = QDir().rename(target, staging);
        if (ok) ok = QDir().rename(result.backup, target);
        if (ok) QDir(staging).removeRecursively();
        return ok;
    };
    for (const auto& entry : QDir(result.backup).entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot)) {
        if (!preserve(entry)) {
            result.error = rollback() ? "Failed preserving portable data; original restored." : "Data recovery required. Keep backup and staging directories: " + result.backup + " ; " + staging;
            return result;
        }
    }
    installed = QDir().rename(staging, target);
    if (!installed || !AppImageUpdate::syncDirectory(info.absolutePath()) || !health(target)) {
        result.error = rollback() ? "Portable update failed; original and user data restored." : "Rollback needs manual recovery: " + result.backup + " ; " + staging;
        return result;
    }
    result.success = true;
    return result;
}
#endif
}
