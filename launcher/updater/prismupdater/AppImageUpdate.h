// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "ReleaseUpdateManifest.h"
#include <QJsonArray>
#include <QDir>
#include <QFileInfo>
#include <QLockFile>
#include <QProcessEnvironment>
#include <QScopeGuard>
#include <QSysInfo>
#include <QTemporaryFile>
#include <QUuid>
#include <functional>
#include <memory>
#ifdef Q_OS_LINUX
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace AppImageUpdate {
inline QJsonObject artifact(const QJsonObject& manifest, const QString& architecture, bool needsNeo)
{
    if (!ReleaseUpdate::validManifest(manifest)) return {};
    QJsonObject selected;
    for (const auto value : manifest.value("artifacts").toArray()) {
        const auto item = value.toObject();
        const auto name = item.value("asset").toString();
        if (item.value("platform").toString() != "linux" || item.value("format").toString() != "appimage" ||
            item.value("architecture").toString() != architecture || (needsNeo && !item.value("neoUi").toBool()) ||
            !name.startsWith("LunaLauncher-Linux-") || !name.endsWith(".AppImage") ||
            name.contains('/') || name.contains('\\') || name.contains(':') || name.contains("..") ||
            item.value("size").toInteger() <= 0 ||
            !QRegularExpression("^[a-fA-F0-9]{64}$").match(item.value("sha256").toString()).hasMatch()) continue;
        if (!selected.isEmpty()) return {};
        selected = item;
    }
    return selected;
}

inline bool validImage(const QString& path, const QString& architecture)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;
    const auto header = file.read(20);
    const auto machine = architecture == "x64" ? 62 : architecture == "arm64" ? 183 : -1;
    return machine > 0 && header.size() == 20 && header.first(4) == QByteArray("\177ELF", 4) &&
        header[4] == 2 && header[5] == 1 && header.mid(8, 3) == QByteArray("AI\002", 3) &&
        uchar(header[18]) == machine && header[19] == 0;
}

// An inherited AppDir would make the new runtime load libraries/plugins from the old mount.
inline QProcessEnvironment cleanEnvironment()
{
    auto env = QProcessEnvironment::systemEnvironment();
    const auto oldDir = env.value("APPDIR");
    for (const auto& key : { "APPIMAGE", "APPDIR", "ARGV0", "OWD", "LD_LIBRARY_PATH", "LD_PRELOAD",
                            "QT_PLUGIN_PATH", "QT_QPA_PLATFORM_PLUGIN_PATH", "QML2_IMPORT_PATH", "QML_IMPORT_PATH",
                            "WEBKIT_EXEC_PATH", "WEBKIT_INJECTED_BUNDLE_PATH", "GSETTINGS_SCHEMA_DIR", "GDK_PIXBUF_MODULE_FILE",
                            "GI_TYPELIB_PATH", "GTK_PATH", "GTK_DATA_PREFIX", "GTK_EXE_PREFIX", "GTK_IM_MODULE_FILE", "GIO_MODULE_DIR" }) env.remove(key);
    if (!oldDir.isEmpty()) {
        for (const auto& key : { "PATH", "XDG_DATA_DIRS" }) {
            QStringList paths;
            for (const auto& path : env.value(key).split(':'))
                if (path != oldDir && !path.startsWith(oldDir + '/')) paths.append(path);
            env.insert(key, paths.join(':'));
        }
    }
    return env;
}

// Only a process launched by this AppDir's AppRun may replace its outer image.
inline QString runningImage(const QString& applicationDirectory)
{
#ifdef Q_OS_LINUX
    const auto env = QProcessEnvironment::systemEnvironment();
    const auto appDir = QFileInfo(env.value("APPDIR")).canonicalFilePath();
    const auto binaryDir = QFileInfo(applicationDirectory).canonicalFilePath();
    const auto image = env.value("APPIMAGE");
    const QFileInfo info(image);
    if (!appDir.isEmpty() && binaryDir == appDir + "/usr/bin" && info.isAbsolute() && info.isFile() &&
        !info.isSymLink() && validImage(image, QSysInfo::buildCpuArchitecture() == "x86_64" ? "x64" : "arm64")) return info.absoluteFilePath();
#endif
    return {};
}

#ifdef Q_OS_LINUX
inline bool syncDirectory(const QString& path)
{
    const auto fd = ::open(QFile::encodeName(path).constData(), O_DIRECTORY | O_RDONLY | O_CLOEXEC);
    if (fd < 0) return false;
    const auto result = ::fsync(fd) == 0;
    ::close(fd);
    return result;
}

struct Result { bool success = false; QString error; QString backup; };
inline Result replace(const QString& target, const QString& source, const QJsonObject& item,
                      const std::function<bool(const QString&)>& health,
                      const std::function<void(qint64, qint64)>& progress = {})
{
    Result result;
    const QFileInfo info(target);
    if (!info.isAbsolute() || !info.isFile() || info.isSymLink()) {
        result.error = "AppImage must be a regular file at an absolute path (symlinks are not updated)."; return result;
    }
    QLockFile lock(target + ".update.lock");
    if (!lock.tryLock(0)) { result.error = "Another update holds this AppImage's lock."; return result; }
    struct stat original {};
    if (::stat(QFile::encodeName(target).constData(), &original) || original.st_nlink != 1 ||
        (original.st_mode & 0222) == 0 || !info.isWritable()) {
        result.error = "AppImage is read-only or has multiple hard links; move it to a writable directory first."; return result;
    }
    auto staged = std::make_unique<QTemporaryFile>(info.absolutePath() + "/.luna-appimage-XXXXXX");
    if (!staged->open()) { result.error = staged->errorString(); return result; }
    QFile input(source);
    if (!input.open(QIODevice::ReadOnly)) { result.error = input.errorString(); return result; }
    qint64 copied = 0;
    while (!input.atEnd()) {
        const auto block = input.read(1024 * 1024);
        if (block.isEmpty() || staged->write(block) != block.size()) { result.error = "Failed staging AppImage."; return result; }
        copied += block.size();
        if (copied > item.value("size").toInteger()) { result.error = "AppImage grew beyond the manifest size while staging."; return result; }
        if (progress) progress(copied, item.value("size").toInteger());
    }
    if (!staged->flush() || ::fchmod(staged->handle(), (original.st_mode & 0777) | S_IXUSR) || ::fsync(staged->handle())) {
        result.error = "Failed to save AppImage or executable permissions."; return result;
    }
    const auto stagedPath = staged->fileName();
    const auto cleanupStaged = qScopeGuard([stagedPath] { QFile::remove(stagedPath); });
    // QTemporaryFile::close() retains its writable OS handle. Destroy it
    // before execve, otherwise Linux refuses the health probe with ETXTBSY.
    staged->setAutoRemove(false);
    staged.reset();
    if (!ReleaseUpdate::verifyPackage(stagedPath, item) || !validImage(stagedPath, item.value("architecture").toString())) {
        result.error = "Staged AppImage does not match the release checksum, size or architecture."; return result;
    }
    if (!health(stagedPath)) { result.error = "New AppImage failed the version health check; original kept."; return result; }
    struct stat current {};
    if (::lstat(QFile::encodeName(target).constData(), &current) || current.st_ino != original.st_ino ||
        current.st_dev != original.st_dev || current.st_size != original.st_size || current.st_mtim.tv_sec != original.st_mtim.tv_sec ||
        current.st_mtim.tv_nsec != original.st_mtim.tv_nsec || !S_ISREG(current.st_mode) || current.st_nlink != 1) {
        result.error = "AppImage changed during updating; original kept."; return result;
    }
    result.backup = target + ".backup-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    const auto originalName = QFile::encodeName(target);
    const auto backupName = QFile::encodeName(result.backup);
    if (::link(originalName.constData(), backupName.constData())) {
        result.error = "Could not create a durable AppImage backup."; return result;
    }
    if (!syncDirectory(info.absolutePath())) {
        QFile::remove(result.backup);
        result.error = "Could not save AppImage backup directory; original kept."; return result;
    }
    // POSIX rename atomically swaps the file; the old mounted image remains readable.
    if (::rename(QFile::encodeName(stagedPath).constData(), originalName.constData())) {
        QFile::remove(result.backup); result.error = "Could not replace AppImage; original kept."; return result;
    }
    if (!syncDirectory(info.absolutePath()) || !health(target)) {
        if (::rename(backupName.constData(), originalName.constData()) || !syncDirectory(info.absolutePath()))
            result.error = "Update failed and rollback failed; restore the retained backup: " + result.backup;
        else result.error = "New AppImage failed verification; original restored.";
        return result;
    }
    result.success = true;
    return result;
}
#endif
}
