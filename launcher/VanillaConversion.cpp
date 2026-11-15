// SPDX-License-Identifier: GPL-3.0-only
#include "VanillaConversion.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QCryptographicHash>
#include <QHash>
#include <QProcess>
#include <algorithm>

#include "FileSystem.h"
#include "VanillaCompatibleProvider.h"
#include "minecraft/GradleSpecifier.h"
#include "minecraft/ImportedClientProfile.h"

namespace {
const QSet<QString> gameDirectories{ "mods", "config", "saves", "resourcepacks", "shaderpacks", "screenshots", "logs", "crash-reports", "datapacks" };
const QSet<QString> launcherFiles{ "assets", "libraries", "versions", "launcher_profiles.json", "launcher_accounts.json", "pcl.ini", "hmclversion.cfg" };

QJsonObject mergeVersionJson(const QJsonObject& base, const QJsonObject& overlay)
{
    auto result = base;
    for (auto it = overlay.begin(); it != overlay.end(); ++it) {
        const auto key = it.key();
        if (key == QStringLiteral("uid") || key == QStringLiteral("version"))
            continue;
        if (it.value().isArray() && result.value(key).isArray() && (key == QStringLiteral("libraries") || key == QStringLiteral("game") || key == QStringLiteral("jvm"))) {
            auto values = result.value(key).toArray();
            for (const auto& value : it.value().toArray()) values.append(value);
            result.insert(key, values);
        } else if (it.value().isObject() && result.value(key).isObject()) {
            result.insert(key, mergeVersionJson(result.value(key).toObject(), it.value().toObject()));
        } else {
            result.insert(key, it.value());
        }
    }
    return result;
}

QString findLocalLibrary(const QString& librariesRoot, const QJsonObject& library)
{
    auto path = library.value(QStringLiteral("downloads")).toObject().value(QStringLiteral("artifact")).toObject().value(QStringLiteral("path")).toString();
    if (path.isEmpty()) {
        const GradleSpecifier spec(library.value(QStringLiteral("name")).toString());
        if (spec.valid())
            path = spec.toPath();
    }
    path = QDir::cleanPath(QDir::fromNativeSeparators(path));
    if (path.isEmpty() || path == "." || path == ".." || path.startsWith("../") || path.contains(':') || QDir::isAbsolutePath(path))
        return {};
    const auto exact = QDir(librariesRoot).filePath(QDir::fromNativeSeparators(path));
    if (QFileInfo(exact).isFile())
        return exact;

    QDir current(librariesRoot);
    const auto parts = QDir::fromNativeSeparators(path).split('/', Qt::SkipEmptyParts);
    for (qsizetype index = 0; index < parts.size(); ++index) {
        const auto entries = current.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
        auto it = std::find_if(entries.cbegin(), entries.cend(), [&parts, index](const QFileInfo& entry) {
            return entry.fileName().compare(parts.at(index), Qt::CaseInsensitive) == 0;
        });
        if (it == entries.cend())
            return {};
        if (index == parts.size() - 1)
            return it->isFile() ? it->absoluteFilePath() : QString();
        if (!it->isDir())
            return {};
        current.setPath(it->absoluteFilePath());
    }
    return {};
}

bool hasDownloadSource(const QJsonObject& library)
{
    const auto downloads = library.value(QStringLiteral("downloads")).toObject();
    if (!downloads.value(QStringLiteral("artifact")).toObject().value(QStringLiteral("url")).toString().isEmpty()
        || !library.value(QStringLiteral("url")).toString().isEmpty())
        return true;
    const auto classifiers = downloads.value(QStringLiteral("classifiers")).toObject();
    for (auto it = classifiers.begin(); it != classifiers.end(); ++it)
        if (!it.value().toObject().value(QStringLiteral("url")).toString().isEmpty())
            return true;
    return false;
}

bool hasClientDownload(const QJsonObject& profile)
{
    return !profile.value(QStringLiteral("downloads")).toObject().value(QStringLiteral("client")).toObject().value(QStringLiteral("url")).toString().isEmpty();
}

bool localizeLibraries(QJsonObject& profile, const QString& sourceLibrariesRoot, const QString& targetRoot, QStringList& copied,
                       QStringList& warnings, QString& error)
{
    auto libraries = profile.value(QStringLiteral("libraries")).toArray();
    QHash<QString, QString> localized;
    for (qsizetype index = 0; index < libraries.size(); ++index) {
        auto library = libraries.at(index).toObject();
        const auto name = library.value(QStringLiteral("name")).toString();
        if (name.isEmpty() || library.contains(QStringLiteral("natives"))) {
            // Native classifiers retain the Maven tree copied above. Library
            // resolution uses that tree before considering a remote download.
            continue;
        }
        const auto source = findLocalLibrary(sourceLibrariesRoot, library);
        if (source.isEmpty()) {
            if (hasDownloadSource(library))
                warnings.append(QObject::tr("Library %1 was not found in the source directory; Prism will download it from the profile URL.").arg(name));
            else
                warnings.append(QObject::tr("Library %1 was not found and has no download URL; the converted instance may not launch until this file is supplied.").arg(name));
            continue;
        }
        auto targetName = localized.value(source);
        if (targetName.isEmpty()) {
            const auto digest = QCryptographicHash::hash(name.toUtf8(), QCryptographicHash::Sha256).toHex().left(12);
            targetName = QStringLiteral("pcl-%1-%2").arg(QString::fromLatin1(digest), QFileInfo(source).fileName());
            const auto destination = QDir(targetRoot).filePath(QStringLiteral("libraries/%1").arg(targetName));
            if (!QDir().mkpath(QFileInfo(destination).absolutePath())) {
                error = QObject::tr("Could not create the local library directory for %1.").arg(name);
                return false;
            }
            if (!QFileInfo::exists(destination) && !QFile::copy(source, destination)) {
                error = QObject::tr("Could not preserve the local library %1.").arg(source);
                return false;
            }
            copied.append(QDir(targetRoot).relativeFilePath(destination));
            localized.insert(source, targetName);
        }
        library.insert(QStringLiteral("MMC-hint"), QStringLiteral("local"));
        library.insert(QStringLiteral("MMC-filename"), targetName);
        libraries.replace(index, library);
    }
    profile.insert(QStringLiteral("libraries"), libraries);
    return true;
}

bool readJsonObject(const QString& path, QJsonObject& object, QString& error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = QObject::tr("Could not open version JSON %1: %2").arg(path, file.errorString());
        return false;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        error = QObject::tr("Could not parse version JSON %1: %2").arg(path, parseError.errorString());
        return false;
    }
    object = document.object();
    return true;
}



bool safeRelative(const QString& relative) { return !relative.isEmpty() && !QDir::isAbsolutePath(relative) && relative != ".." && !relative.startsWith("../"); }

bool cancelled(const VanillaConversionFeedback& feedback, QString& error)
{
    if (!feedback.cancelled || !feedback.cancelled()) return false;
    error = QObject::tr("导入已取消。");
    return true;
}

bool copyTree(const QString& source, const QString& target, const QSet<QString>& excluded, QStringList& copied, QString& error,
              const VanillaConversionFeedback& feedback = {}, const QString& stage = {})
{
    if (!QFileInfo(source).exists()) return true;
    if (!QDir().mkpath(target)) { error = QObject::tr("Could not create %1.").arg(target); return false; }
    // Determine a real denominator, then copy in bounded buffers so a single
    // large resource pack also reports progress and observes cancellation.
    feedback.update(QObject::tr("正在扫描待复制文件"), 0, 0, "files", source);
    QList<QFileInfo> entries;
    qint64 totalBytes = 0, totalFiles = 0;
    QDirIterator iterator(source, QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        if (cancelled(feedback, error)) return false;
        const auto sourcePath = iterator.next();
        const auto relative = QDir(source).relativeFilePath(sourcePath);
        if (!safeRelative(relative)) { error = QObject::tr("Unsafe path in source: %1").arg(relative); return false; }
        const auto first = relative.section('/', 0, 0).toLower();
        if (excluded.contains(first)) continue;
        const auto info = QFileInfo(sourcePath);
        if (info.isSymLink()) { error = QObject::tr("Symbolic links are not supported in imported game content: %1").arg(sourcePath); return false; }
        entries.append(info);
        if (info.isFile()) { totalBytes += info.size(); ++totalFiles; }
        feedback.update(QObject::tr("正在扫描待复制文件"), entries.size(), 0, "files", relative);
    }
    qint64 bytes = 0, files = 0;
    const auto label = stage.isEmpty() ? QObject::tr("正在复制实例文件") : stage;
    const auto update = [&](const QString& relative) {
        feedback.update(label, totalBytes > 0 ? bytes : files, totalBytes > 0 ? totalBytes : totalFiles,
                        totalBytes > 0 ? "bytes" : "files", QObject::tr("%1 / %2 个文件 · %3").arg(files).arg(totalFiles).arg(relative));
    };
    update({});
    for (const auto& info : entries) {
        if (cancelled(feedback, error)) return false;
        const auto sourcePath = info.absoluteFilePath();
        const auto relative = QDir(source).relativeFilePath(sourcePath);
        const auto destination = QDir(target).filePath(relative);
        if (info.isDir()) {
            if (!QDir().mkpath(destination)) { error = QObject::tr("Could not create %1.").arg(destination); return false; }
        } else if (info.isFile()) {
            QFile input(sourcePath);
            // The whole destination tree is already transactional. Avoid a
            // per-file fsync for thousands of small assets/modpack files.
            QFile output(destination);
            if (!QDir().mkpath(QFileInfo(destination).absolutePath()) || !input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly)) {
                error = QObject::tr("Could not copy %1.").arg(sourcePath); return false;
            }
            while (!input.atEnd()) {
                if (cancelled(feedback, error)) return false;
                const auto buffer = input.read(1024 * 1024);
                if (input.error() != QFile::NoError || output.write(buffer) != buffer.size()) {
                    error = QObject::tr("Could not copy %1.").arg(sourcePath); return false;
                }
                bytes += buffer.size();
                update(relative);
            }
            output.close();
            if (output.error() != QFile::NoError) { error = output.errorString(); return false; }
            ++files;
            update(relative);
            copied.append(relative);
        }
    }
    return true;
}

bool writeJson(const QString& path, const QJsonObject& value, QString& error)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) { error = QObject::tr("Could not create %1.").arg(QFileInfo(path).absolutePath()); return false; }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) { error = file.errorString(); return false; }
    const auto data = QJsonDocument(value).toJson(QJsonDocument::Indented);
    if (file.write(data) != data.size() || !file.commit()) { error = file.errorString(); return false; }
    return true;
}

QJsonObject readObject(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()).object() : QJsonObject{};
}

// Cache entries are immutable inputs to an export. Never replace a different
// file in an existing game directory, even when it has the same Maven path.
bool copyArtifact(const QString& source, const QString& target, QString& error)
{
    if (!QFileInfo(source).isFile())
        return true; // A remote dependency can be repaired using its download URL.
    if (QFileInfo::exists(target)) {
        QFile left(source), right(target);
        QCryptographicHash a(QCryptographicHash::Sha1), b(QCryptographicHash::Sha1);
        if (left.open(QIODevice::ReadOnly) && right.open(QIODevice::ReadOnly)
            && a.addData(&left) && b.addData(&right) && a.result() == b.result())
            return true;
        error = QObject::tr("An existing game file differs from the installed dependency: %1").arg(target);
        return false;
    }
    if (QDir().mkpath(QFileInfo(target).absolutePath()) && QFile::copy(source, target))
        return true;
    error = QObject::tr("Could not copy dependency %1 to %2.").arg(source, target);
    return false;
}

bool vanillaLibrary(QJsonObject& library, const QString& prismRoot, const VanillaExportOptions& options,
                    QMap<QString, QString>& artifacts, QString& error)
{
    const GradleSpecifier spec(library.value("name").toString());
    if (!spec.valid()) {
        error = QObject::tr("Invalid library coordinate: %1").arg(library.value("name").toString());
        return false;
    }
    const bool local = library.value("MMC-hint").toString() == QStringLiteral("local");
    const auto filename = library.value("MMC-filename").toString(QFileInfo(spec.toPath()).fileName());
    if (local && (filename.contains('/') || filename.contains('\\') || filename == "..")) {
        error = QObject::tr("Invalid instance-local library filename: %1").arg(filename);
        return false;
    }
    const auto absoluteUrl = library.value("MMC-absoluteUrl").toString(library.value("MMC-absulute_url").toString());
    auto downloads = library.value("downloads").toObject();
    auto artifact = downloads.value("artifact").toObject();
    const bool legacyDownload = artifact.isEmpty() && absoluteUrl.isEmpty() && !local;
    if (!library.contains("natives") || !artifact.isEmpty()) {
        const auto path = artifact.value("path").toString(spec.toPath());
        if (!safeRelative(QDir::cleanPath(path))) {
            error = QObject::tr("Unsafe library path: %1").arg(path);
            return false;
        }
        artifact.insert("path", path);
        if (!absoluteUrl.isEmpty()) artifact.insert("url", absoluteUrl);
        if (artifact.value("url").toString().isEmpty() && !local) {
            auto repository = library.value("url").toString("https://libraries.minecraft.net/");
            if (!repository.endsWith('/')) repository += '/';
            artifact.insert("url", repository + path);
        }
        auto source = local ? QDir(prismRoot).filePath("libraries/" + filename)
                            : QDir(options.librariesRoot).filePath(path);
        if (!local && !QFileInfo(source).isFile()) source = QDir(prismRoot).filePath("libraries/" + path);
        if (local && !QFileInfo(source).isFile()) {
            error = QObject::tr("The instance-local library is missing: %1").arg(source);
            return false;
        }
        if (QFileInfo(source).isFile()) {
            artifacts.insert("libraries/" + path, source);
            if (local || !artifact.contains("sha1")) {
                QFile file(source);
                QCryptographicHash hash(QCryptographicHash::Sha1);
                if (!file.open(QIODevice::ReadOnly) || !hash.addData(&file)) {
                    error = QObject::tr("Could not read library %1.").arg(source);
                    return false;
                }
                artifact.insert("sha1", QString::fromLatin1(hash.result().toHex()));
                artifact.insert("size", QFileInfo(source).size());
                if (local) artifact.insert("url", QString());
            }
        }
        // A legacy Maven declaration may intentionally lack checksum metadata.
        // Keep that declaration when its file is not cached yet; do not invent
        // an incomplete Mojang downloads object.
        if (!legacyDownload || artifact.contains("sha1")) downloads.insert("artifact", artifact);
    }
    const auto classifiers = downloads.value("classifiers").toObject();
    for (auto it = classifiers.begin(); it != classifiers.end(); ++it) {
        auto classifier = spec;
        classifier.setClassifier(it.key());
        const auto path = it.value().toObject().value("path").toString(classifier.toPath());
        if (!safeRelative(QDir::cleanPath(path))) {
            error = QObject::tr("Unsafe native library path: %1").arg(path);
            return false;
        }
        auto source = QDir(prismRoot).filePath("libraries/" + path);
        if (!QFileInfo(source).isFile()) source = QDir(options.librariesRoot).filePath(path);
        artifacts.insert("libraries/" + path, source);
    }
    // Older/custom HMCL packs can bundle natives without a downloads object.
    // They still need to survive export back to an ordinary .minecraft tree.
    const auto natives = library.value("natives").toObject();
    for (auto it = natives.begin(); it != natives.end(); ++it) {
        QStringList variants{it.value().toString()};
        if (variants.first().contains("${arch}"))
            variants = {QString(it.value().toString()).replace("${arch}", "32"), QString(it.value().toString()).replace("${arch}", "64")};
        for (const auto& variant : variants) {
            if (variant.isEmpty() || classifiers.contains(variant)) continue;
            auto nativeSpec = spec;
            nativeSpec.setClassifier(variant);
            const auto path = nativeSpec.toPath();
            if (!safeRelative(QDir::cleanPath(path))) {
                error = QObject::tr("Unsafe native library path: %1").arg(path);
                return false;
            }
            auto source = QDir(prismRoot).filePath("libraries/" + path);
            if (!QFileInfo(source).isFile()) source = QDir(options.librariesRoot).filePath(path);
            if (QFileInfo(source).isFile()) artifacts.insert("libraries/" + path, source);
        }
    }
    if (!downloads.isEmpty()) library.insert("downloads", downloads);
    for (const auto& key : library.keys())
        if (key.startsWith("MMC-")) library.remove(key);
    return true;
}

bool vanillaProfile(const QString& prismRoot, const VanillaExportOptions& options, QJsonObject& version,
                    QJsonObject& base, QMap<QString, QString>& artifacts, QString& error)
{
    auto profiles = options.componentProfiles;
    if (profiles.isEmpty()) {
        const auto pack = readObject(QDir(prismRoot).filePath("mmc-pack.json"));
        for (const auto& value : pack.value("components").toArray()) {
            const auto component = value.toObject();
            if (component.value("disabled").toBool()) continue;
            const auto uid = component.value("uid").toString();
            auto profile = uid == "net.minecraft" ? options.versionJson : QJsonObject{};
            if (profile.isEmpty()) profile = readObject(QDir(prismRoot).filePath("patches/" + uid + ".json"));
            if (profile.isEmpty()) {
                error = QObject::tr("Resolved metadata is missing for component %1. Refusing to export an incomplete vanilla profile.").arg(uid);
                return false;
            }
            profile.insert("uid", uid);
            profiles.append(profile);
        }
        if (profiles.isEmpty()) {
            auto profile = options.versionJson.isEmpty() ? readObject(QDir(prismRoot).filePath("patches/net.minecraft.json")) : options.versionJson;
            profile.insert("uid", "net.minecraft");
            profiles.append(profile);
        }
    }
    QString minecraftVersion;
    QJsonArray featureArguments;
    QSet<QString> resolvedIds;
    for (const auto& value : profiles) resolvedIds.insert(value.toObject().value("uid").toString());
    const auto components = readObject(QDir(prismRoot).filePath("mmc-pack.json")).value("components").toArray();
    for (const auto& value : components) {
        const auto component = value.toObject();
        if (!component.value("disabled").toBool() && !resolvedIds.contains(component.value("uid").toString())) {
            error = QObject::tr("Resolved metadata is missing for component %1.").arg(component.value("uid").toString());
            return false;
        }
    }
    for (const auto& value : profiles) {
        const auto profile = value.toObject();
        for (const auto& dependency : profile.value("requires").toArray()) {
            const auto uid = dependency.toObject().value("uid").toString();
            if (!resolvedIds.contains(uid)) {
                error = QObject::tr("Resolved dependency metadata is missing for %1.").arg(uid);
                return false;
            }
        }
        if (profile.value("uid").toString() == "net.minecraft")
            minecraftVersion = profile.value("version").toString(profile.value("id").toString());
    }
    if (minecraftVersion.isEmpty() || minecraftVersion.contains('/') || minecraftVersion.contains('\\') || minecraftVersion == "..") {
        error = QObject::tr("The resolved Minecraft version is missing or invalid.");
        return false;
    }
    for (const auto& value : profiles) {
        auto profile = value.toObject();
        const auto uid = profile.value("uid").toString();
        if (uid != "net.minecraft") {
            for (const auto& key : { "id", "type", "releaseTime", "time" }) profile.remove(key);
        }
        for (const auto& key : { "jarMods", "mods", "+agents" }) {
            if (!profile.value(key).toArray().isEmpty()) {
                error = QObject::tr("Component %1 uses %2, which cannot be exported losslessly to vanilla format.").arg(uid, key);
                return false;
            }
        }
        if (!profile.value("appletClass").toString().isEmpty()) {
            error = QObject::tr("Legacy applet component %1 requires the MMC runtime.").arg(uid);
            return false;
        }
        auto arguments = profile.value("arguments").toObject();
        if (profile.contains("minecraftArguments")) {
            // MMC's legacy argument string replaces the preceding game args.
            QJsonArray game;
            for (const auto& argument : QProcess::splitCommand(profile.value("minecraftArguments").toString())) game.append(argument);
            auto prior = version.value("arguments").toObject();
            prior.remove("game");
            version.insert("arguments", prior);
            arguments.insert("game", game);
        }
        auto game = arguments.value("game").toArray();
        for (const auto& tweaker : profile.value("+tweakers").toArray()) {
            game.append("--tweakClass"); game.append(tweaker);
        }
        if (!game.isEmpty()) arguments.insert("game", game);
        auto jvm = arguments.value("jvm").toArray();
        for (const auto& argument : profile.value("+jvmArgs").toArray()) jvm.append(argument);
        for (const auto& trait : profile.value("+traits").toArray()) {
            if (trait.toString() == "FirstThreadOnMacOS") {
                jvm.append(QJsonObject{ { "rules", QJsonArray{ QJsonObject{ { "action", "allow" }, { "os", QJsonObject{ { "name", "osx" } } } } } },
                                         { "value", "-XstartOnFirstThread" } });
            } else if (trait.toString() == "XR:Initial") {
                featureArguments.append(QJsonObject{ { "rules", QJsonArray{ QJsonObject{ { "action", "allow" }, { "features", QJsonObject{ { "has_custom_resolution", true } } } } } },
                                                       { "value", QJsonArray{ "--width", "${resolution_width}", "--height", "${resolution_height}" } } });
            } else if (trait.toString().startsWith("feature:is_quick_play_")) {
                const auto kind = trait.toString().mid(QStringLiteral("feature:is_quick_play_").size());
                const QHash<QString, QString> names{ { "singleplayer", "Singleplayer" }, { "multiplayer", "Multiplayer" }, { "realms", "Realms" } };
                if (!names.contains(kind)) {
                    error = QObject::tr("Unsupported quick play feature: %1").arg(kind);
                    return false;
                }
                const auto option = "quickPlay" + names.value(kind);
                featureArguments.append(QJsonObject{ { "rules", QJsonArray{ QJsonObject{ { "action", "allow" }, { "features", QJsonObject{ { "is_quick_play_" + kind, true } } } } } },
                                                       { "value", QJsonArray{ "--" + option, "${" + option + '}' } } });
            } else {
                error = QObject::tr("Component %1 requires an unsupported MMC launch trait: %2").arg(uid, trait.toString());
                return false;
            }
        }
        const bool forgeWrapper = profile.value("mainClass").toString().contains("forgewrapper.installer.Main");
        if (forgeWrapper) {
            jvm.append("-Dforgewrapper.librariesDir=${library_directory}");
            jvm.append(QStringLiteral("-Dforgewrapper.minecraft=${library_directory}/../versions/%1/%1.jar").arg(minecraftVersion));
            for (const auto& installer : profile.value("mavenFiles").toArray()) {
                const auto library = installer.toObject();
                const GradleSpecifier spec(library.value("name").toString());
                if (spec.classifier() == "installer")
                    jvm.append("-Dforgewrapper.installer=${library_directory}/" + spec.toPath());
            }
        }
        for (qsizetype i = 0; i < jvm.size(); ++i) {
            if (!jvm.at(i).isString()) continue;
            auto argument = jvm.at(i).toString();
            argument.replace("${primary_jar}", QStringLiteral("${library_directory}/../versions/%1/%1.jar").arg(minecraftVersion));
            jvm[i] = argument;
        }
        if (!jvm.isEmpty()) arguments.insert("jvm", jvm);
        if (!arguments.isEmpty()) profile.insert("arguments", arguments);
        QJsonArray libraries;
        for (const auto& libraryValue : profile.value("libraries").toArray()) {
            auto library = libraryValue.toObject();
            if (!vanillaLibrary(library, prismRoot, options, artifacts, error)) return false;
            libraries.append(library);
        }
        profile.insert("libraries", libraries);
        for (const auto& libraryValue : profile.value("mavenFiles").toArray()) {
            auto library = libraryValue.toObject();
            if (!vanillaLibrary(library, prismRoot, options, artifacts, error)) return false;
            const auto path = "libraries/" + library.value("downloads").toObject().value("artifact").toObject().value("path").toString();
            if (!QFileInfo(artifacts.value(path)).isFile()) {
                error = QObject::tr("Required loader installer has not been downloaded: %1").arg(library.value("name").toString());
                return false;
            }
        }
        if (profile.contains("mainJar")) {
            auto library = profile.value("mainJar").toObject();
            if (!vanillaLibrary(library, prismRoot, options, artifacts, error)) return false;
            auto client = library.value("downloads").toObject().value("artifact").toObject();
            const auto libraryPath = "libraries/" + client.take("path").toString();
            const auto source = artifacts.take(libraryPath);
            artifacts.insert(QStringLiteral("versions/%1/%1.jar").arg(minecraftVersion), source);
            auto downloads = profile.value("downloads").toObject();
            downloads.insert("client", client);
            profile.insert("downloads", downloads);
        }
        const auto javaMajors = profile.value("compatibleJavaMajors").toArray();
        if (!javaMajors.isEmpty()) profile.insert("javaVersion", QJsonObject{ { "majorVersion", javaMajors.first() }, { "component", profile.value("compatibleJavaName").toString("jre-legacy") } });
        const auto logging = profile.value("logging").toObject();
        if (logging.contains("argument")) profile.insert("logging", QJsonObject{ { "client", logging } });
        for (const auto& key : { "uid", "version", "formatVersion", "name", "order", "requires", "conflicts", "volatile", "mainJar",
                                 "appletClass", "+traits", "+jvmArgs", "+tweakers", "+agents", "mavenFiles", "jarMods", "mods",
                                 "compatibleJavaMajors", "compatibleJavaName", "minecraftArguments", "inheritsFrom", "runtimes" })
            profile.remove(key);
        for (const auto& key : profile.keys()) {
            if (key.startsWith('+') || key.startsWith('-') || key.startsWith("MMC-")) {
                error = QObject::tr("Component %1 contains an unsupported MMC directive: %2").arg(uid, key);
                return false;
            }
        }
        version = mergeVersionJson(version, profile);
        if (uid == "net.minecraft" || uid == "org.lwjgl" || uid == "org.lwjgl3") base = mergeVersionJson(base, profile);
    }
    auto arguments = version.value("arguments").toObject();
    auto game = arguments.value("game").toArray();
    for (const auto& value : featureArguments) game.append(value);
    arguments.insert("game", game);
    auto jvm = arguments.value("jvm").toArray();
    bool hasClasspath = false, hasNatives = false;
    for (const auto& value : jvm) {
        hasClasspath |= value.toString() == "${classpath}";
        hasNatives |= value.toString().startsWith("-Djava.library.path=");
    }
    if (!hasNatives) jvm.prepend("-Djava.library.path=${natives_directory}");
    if (!hasClasspath) { jvm.append("-cp"); jvm.append("${classpath}"); }
    arguments.insert("jvm", jvm);
    version.insert("arguments", arguments);
    auto baseArguments = base.value("arguments").toObject();
    auto baseJvm = baseArguments.value("jvm").toArray();
    if (!baseJvm.contains("${classpath}")) { baseJvm.append("-cp"); baseJvm.append("${classpath}"); }
    bool baseHasNatives = false;
    for (const auto& value : baseJvm) baseHasNatives |= value.toString().startsWith("-Djava.library.path=");
    if (!baseHasNatives) baseJvm.prepend("-Djava.library.path=${natives_directory}");
    baseArguments.insert("jvm", baseJvm);
    auto baseGame = baseArguments.value("game").toArray();
    for (const auto& value : featureArguments) baseGame.append(value);
    baseArguments.insert("game", baseGame);
    base.insert("arguments", baseArguments);
    QJsonArray libraries;
    QHash<QString, qsizetype> libraryIndexes;
    for (const auto& value : version.value("libraries").toArray()) {
        const auto library = value.toObject();
        const GradleSpecifier spec(library.value("name").toString());
        const auto key = spec.artifactPrefix() + ':' + spec.classifier() + ':'
            + QString::fromUtf8(QJsonDocument(library.value("rules").toArray()).toJson(QJsonDocument::Compact)) + ':'
            + QString::fromUtf8(QJsonDocument(library.value("natives").toObject()).toJson(QJsonDocument::Compact));
        if (libraryIndexes.contains(key)) libraries[libraryIndexes.value(key)] = library;
        else { libraryIndexes.insert(key, libraries.size()); libraries.append(library); }
    }
    version.insert("libraries", libraries);
    version.insert("id", options.versionId);
    version.insert("jar", minecraftVersion);
    base.insert("id", minecraftVersion);
    if (version.value("mainClass").toString().isEmpty() || arguments.value("game").toArray().isEmpty()
        || (!hasClientDownload(version) && !QFileInfo(artifacts.value(QStringLiteral("versions/%1/%1.jar").arg(minecraftVersion))).isFile())
        || version.value("assetIndex").toObject().value("id").toString().isEmpty()) {
        error = QObject::tr("The resolved profile is missing a main class, launch arguments, client JAR or asset index. Refusing to write an incomplete vanilla instance.");
        return false;
    }
    return true;
}

bool writeIni(const QString& path, const QString& name, QString& error)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) { error = QObject::tr("Could not create %1.").arg(QFileInfo(path).absolutePath()); return false; }
    QSaveFile file(path); if (!file.open(QIODevice::WriteOnly)) { error = file.errorString(); return false; }
    const auto data = QStringLiteral("[General]\nInstanceType=OneSix\nname=%1\n").arg(name).toUtf8();
    if (file.write(data) != data.size() || !file.commit()) { error = file.errorString(); return false; }
    return true;
}

QString pclOriginalMinecraftVersion(const QString& gameRoot)
{
    for (const auto& path : { QDir(gameRoot).filePath(QStringLiteral("PCL/Setup.ini")), QDir(gameRoot).filePath(QStringLiteral("pcl/Setup.ini")) }) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            continue;
        for (const auto& line : QString::fromUtf8(file.readAll()).split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts)) {
            const auto trimmed = line.trimmed();
            if (!trimmed.startsWith(QStringLiteral("VersionOriginal"), Qt::CaseInsensitive))
                continue;
            const auto separator = trimmed.indexOf(QRegularExpression(QStringLiteral("[:=]")));
            if (separator >= 0)
                return trimmed.mid(separator + 1).trimmed();
        }
    }
    return {};
}

QString hmclPatchMinecraftVersion(const QJsonObject& profile)
{
    QString result;
    for (const auto& value : profile.value(QStringLiteral("patches")).toArray()) {
        const auto patch = value.toObject();
        if (patch.value(QStringLiteral("id")).toString().compare(QStringLiteral("game"), Qt::CaseInsensitive) != 0)
            continue;
        const auto version = patch.value(QStringLiteral("version")).toString().trimmed();
        if (!version.isEmpty())
            result = version;
    }
    return result;
}

QJsonObject loaderPatchFromProfile(const QJsonObject& profile, const QString& loaderType, const QString& loaderUid)
{
    const auto patches = profile.value(QStringLiteral("patches")).toArray();
    QJsonObject fallback;
    for (const auto& value : patches) {
        if (!value.isObject())
            continue;
        const auto patch = value.toObject();
        const auto id = patch.value(QStringLiteral("id")).toString().trimmed().toLower();
        if (id == loaderType || id == loaderUid || id.contains(loaderType))
            return patch;
        if (fallback.isEmpty() && (patch.contains(QStringLiteral("libraries")) || patch.contains(QStringLiteral("arguments")))) {
            const auto merged = QJsonDocument(patch).toJson(QJsonDocument::Compact);
            if ((loaderType == QStringLiteral("forge") && merged.contains("minecraftforge"))
                || (loaderType == QStringLiteral("neoforge") && merged.contains("neoforged"))
                || (loaderType == QStringLiteral("fabric") && merged.contains("fabric-loader"))
                || (loaderType == QStringLiteral("quilt") && merged.contains("quilt-loader")))
                fallback = patch;
        }
    }
    if (!fallback.isEmpty())
        return fallback;

    auto profileWithoutPatches = profile;
    profileWithoutPatches.remove(QStringLiteral("patches"));
    return profileWithoutPatches;
}


}

QJsonObject VanillaConversionReport::toJson() const
{
    QJsonArray blockersJson; for (const auto& value : blockers) blockersJson.append(value);
    QJsonArray warningsJson; for (const auto& value : warnings) warningsJson.append(value);
    QJsonArray copiedJson; for (const auto& value : copied) copiedJson.append(value);
    return { { "succeeded", succeeded }, { "blockers", blockersJson }, { "warnings", warningsJson }, { "copied", copiedJson }, { "error", error }, { "destination", destination }, { "clientArtifact", clientArtifact } };
}

VanillaConversionReport VanillaConverter::importToPrism(const QString& sourceRoot, const QString& prismTarget, const VanillaImportOptions& options)
{
    options.feedback.update(QObject::tr("正在解析实例配置"));
    VanillaConversionReport report;
    const auto layout = detectVanillaInstanceLayout(sourceRoot);
    if (!layout.isValid()) { report.error = QObject::tr("No vanilla version JSON was found in %1.").arg(sourceRoot); return report; }
    if (QFileInfo::exists(prismTarget)) { report.error = QObject::tr("The Prism destination already exists: %1").arg(prismTarget); return report; }
    const auto staging = prismTarget + QStringLiteral(".tmp-convert-") + QString::number(QRandomGenerator::global()->generate());
    QDir().mkpath(staging);
    QString error;
    const auto targetGameRoot = QDir(staging).filePath(QStringLiteral("minecraft"));
    if (options.copyGameContent && !copyTree(layout.gameRoot, targetGameRoot, launcherFiles, report.copied, error, options.feedback, QObject::tr("正在复制实例文件"))) { report.error = error; FS::deletePath(staging); return report; }
    // Keep the source's resolved libraries beside the Prism instance. This is
    // needed for Forge installer profiles whose libraries are only available
    // in the external launcher cache and have no public download URL.
    if (options.copyGameContent && !copyTree(layout.librariesRoot, QDir(staging).filePath(QStringLiteral("libraries")), {}, report.copied, error, options.feedback, QObject::tr("正在复制运行库"))) {
        report.error = error;
        FS::deletePath(staging);
        return report;
    }
    // In an isolated HMCL/PCL layout `sourceRoot` already points at
    // `<minecraft-root>/versions/<version>`.  The version loader expects the
    // outer minecraft root, otherwise it appends `versions/<version>` twice.
    auto versionLookupRoot = layout.sourceRoot;
    if (QFileInfo(layout.sourceRoot).dir().dirName() == QStringLiteral("versions"))
        versionLookupRoot = QDir::cleanPath(QDir(layout.sourceRoot).filePath(QStringLiteral("../..")));
    auto resolvedVersion = VanillaCompatibleProvider::resolvedProfile(loadVanillaVersionJson(versionLookupRoot, layout.versionId, &error));
    if (resolvedVersion.isEmpty() || !error.isEmpty()) { report.error = error.isEmpty() ? QObject::tr("Could not resolve the vanilla version JSON.") : error; FS::deletePath(staging); return report; }
    ImportedClientProfile::inferJavaRequirement(resolvedVersion, layout.librariesRoot, layout.clientJar);
    QJsonObject rawProfile;
    if (!readJsonObject(layout.versionJson, rawProfile, error)) { report.error = error; FS::deletePath(staging); return report; }
    // Keep the exact source metadata next to the converted instance. This is
    // deliberately outside the active MMC component graph: it lets users
    // recover PCL/HMCL-specific fields and manual runtime changes without
    // making Prism parse launcher-only files.
    if (!writeJson(QDir(staging).filePath(QStringLiteral("luna-imported/source-profile.json")), rawProfile, error)
        || !writeJson(QDir(staging).filePath(QStringLiteral("luna-imported/resolved-profile.json")), resolvedVersion, error)) {
        report.error = error;
        FS::deletePath(staging);
        return report;
    }
    const auto sourcePclRoot = QDir(layout.gameRoot);
    for (const auto& marker : { QStringLiteral("PCL"), QStringLiteral("pcl"), QStringLiteral("HMCL"), QStringLiteral("hmcl") }) {
        const auto markerPath = sourcePclRoot.filePath(marker);
        if (QFileInfo(markerPath).isDir()) {
            if (!copyTree(markerPath, QDir(staging).filePath(QStringLiteral("luna-imported/%1").arg(marker)), {}, report.copied, error, options.feedback, QObject::tr("正在保留启动器配置"))) {
                report.error = error;
                FS::deletePath(staging);
                return report;
            }
            report.warnings.append(QObject::tr("The original %1 launcher metadata was preserved under luna-imported.").arg(marker));
        }
    }
    for (const auto& marker : { QStringLiteral("PCL.ini"), QStringLiteral("pcl.ini"), QStringLiteral("hmclversion.cfg") }) {
        const auto markerPath = sourcePclRoot.filePath(marker);
        if (QFileInfo(markerPath).isFile()) {
            const auto destination = QDir(staging).filePath(QStringLiteral("luna-imported/%1").arg(marker));
            if (!QDir().mkpath(QFileInfo(destination).absolutePath()) || !QFile::copy(markerPath, destination)) {
                QFile::remove(destination);
                if (!QFile::copy(markerPath, destination)) {
                    report.error = QObject::tr("Could not preserve launcher metadata %1.").arg(markerPath);
                    FS::deletePath(staging);
                    return report;
                }
            }
            report.copied.append(QDir(staging).relativeFilePath(destination));
            report.warnings.append(QObject::tr("The original %1 launcher metadata was preserved under luna-imported.").arg(marker));
        }
    }
    if (QFileInfo(layout.assetsRoot).isDir() && !copyTree(layout.assetsRoot, QDir(staging).filePath("assets"), {}, report.copied, error, options.feedback, QObject::tr("正在复制资源文件"))) {
        report.error = error; FS::deletePath(staging); return report;
    }
    options.feedback.update(QObject::tr("正在校验客户端并生成运行配置"));
    const auto loaderType = VanillaCompatibleProvider::detectedLoaderType(resolvedVersion);
    auto loaderUid = VanillaCompatibleProvider::loaderUidForType(loaderType);
    auto minecraftVersion = VanillaCompatibleProvider::minecraftVersionForLayout(resolvedVersion,layout);
    const auto hmclPatchVersion = hmclPatchMinecraftVersion(rawProfile);
    if (!hmclPatchVersion.isEmpty() && hmclPatchVersion != minecraftVersion) {
        minecraftVersion = hmclPatchVersion;
        report.warnings.append(QObject::tr("HMCL game patch version %1 was used as the Minecraft base version.").arg(minecraftVersion));
    }
    const auto pclOriginalVersion = pclOriginalMinecraftVersion(layout.gameRoot);
    if (!pclOriginalVersion.isEmpty() && pclOriginalVersion != minecraftVersion) {
        minecraftVersion = pclOriginalVersion;
        report.warnings.append(QObject::tr("PCL VersionOriginal=%1 was used as the Minecraft base version.").arg(minecraftVersion));
    }
    const auto loaderVersion = VanillaCompatibleProvider::detectedLoaderVersion(resolvedVersion);
    const bool splitStandardLoader = !loaderUid.isEmpty() && !loaderVersion.isEmpty();
    const bool hasLocalClient = QFileInfo(layout.clientJar).isFile();
    // A complete imported runtime remains authoritative, including custom
    // core JARs. Loader components describe identity; they must not replace
    // the combined libraries/main class/Java settings with index defaults.
    const bool standardMetadataOnly = !hasLocalClient && splitStandardLoader
        && VanillaCompatibleProvider::isStandardLoaderProfile(rawProfile, loaderType, loaderPatchFromProfile(rawProfile, loaderType, loaderUid));
    auto baseVersion = resolvedVersion;
    QJsonObject loaderProfile{{"name",loaderType}};
    const auto componentVersion = minecraftVersion;
    if (!standardMetadataOnly) {
        baseVersion.remove(QStringLiteral("inheritsFrom"));
        baseVersion.insert(QStringLiteral("id"), componentVersion);
        baseVersion.insert(QStringLiteral("uid"), QStringLiteral("net.minecraft"));
        baseVersion.insert(QStringLiteral("version"), componentVersion);
    }
    if (!standardMetadataOnly && QFileInfo(layout.clientJar).isFile()) {
        report.clientArtifact = ImportedClientProfile::inspectClient(layout.clientJar, resolvedVersion, &error);
        if (report.clientArtifact.isEmpty()) { report.error = error; FS::deletePath(staging); return report; }
        const auto filename = "imported-client-" + report.clientArtifact.value("sha256").toString().left(24) + ".jar";
        const auto destination = QDir(staging).filePath("libraries/" + filename);
        if (!QDir().mkpath(QFileInfo(destination).absolutePath()) || !QFile::copy(layout.clientJar,destination)
            || !ImportedClientProfile::verifyClient(destination, report.clientArtifact, &error)) {
            report.error = error.isEmpty() ? QObject::tr("Could not preserve the imported client JAR.") : error;
            FS::deletePath(staging); return report;
        }
        report.clientArtifact.insert("path", "libraries/" + filename);
        baseVersion.insert("mainJar", ImportedClientProfile::localMainJar(report.clientArtifact, filename));
        if (!writeJson(QDir(staging).filePath("luna-imported/client-artifact.json"),report.clientArtifact,error)) {
            report.error = error; FS::deletePath(staging); return report;
        }
        report.copied.append("libraries/" + filename);
        report.warnings.append(QObject::tr("The imported client JAR is preserved locally; automatic replacement by the official client is disabled."));
    } else if (!standardMetadataOnly && !hasClientDownload(baseVersion) && !hasClientDownload(resolvedVersion)) {
        report.blockers.append(QObject::tr("The source client JAR is missing and the resolved profile has no client download URL."));
        report.error = report.blockers.last();
        FS::deletePath(staging);
        return report;
    } else if (!standardMetadataOnly && !QFileInfo(layout.clientJar).isFile()) {
        report.warnings.append(QObject::tr("The source client JAR was not found; Prism will download the client from the resolved profile URL."));
    }
    if (!standardMetadataOnly && !localizeLibraries(baseVersion, layout.librariesRoot, staging, report.copied, report.warnings, error)) {
        report.error = error;
        FS::deletePath(staging);
        return report;
    }
    QJsonObject basePatch;
    if (!standardMetadataOnly) {
        basePatch = VanillaCompatibleProvider::prismRuntimePatch(baseVersion, QStringLiteral("net.minecraft"), minecraftVersion, false);
        basePatch.insert(QStringLiteral("version"), componentVersion);
    }
    QJsonArray components{ QJsonObject{ { "uid", "net.minecraft" }, { "version", componentVersion }, { "important", true } } };
    if (splitStandardLoader) {
        if (!standardMetadataOnly && !localizeLibraries(loaderProfile, layout.librariesRoot, staging, report.copied, report.warnings, error)) {
            report.error = error;
            FS::deletePath(staging);
            return report;
        }
        components.append(QJsonObject{ { "uid", loaderUid }, { "version", loaderVersion } });
        if (!standardMetadataOnly) {
            const auto loaderPatch = VanillaCompatibleProvider::prismRuntimePatch(loaderProfile, loaderUid, loaderVersion, true);
            if (!writeJson(QDir(staging).filePath(QStringLiteral("patches/%1.json").arg(loaderUid)), loaderPatch, error)) { report.error = error; FS::deletePath(staging); return report; }
        }
        report.warnings.append(QObject::tr("The %1 loader was imported as the standard MMC component %2.").arg(loaderType, loaderUid));
    }
    // Mark converted packs so Luna's external provenance recognizer can tell
    // a Luna-produced MMC bridge from a native Prism/MMC pack.  The marker is
    // additive and ignored by Prism, HMCL and PCL.  Keeping it in the pack
    // also makes the conversion result portable between Luna installations.
    const auto pack = QJsonObject{ { "formatVersion", 1 },
                                   { "components", components },
                                   { "luna", QJsonObject{ { "generatedBy", QStringLiteral("LunaLauncher") },
                                                           { "format", QStringLiteral("vanilla-compatible-mmc") },
                                                           { "version", 1 } } } };
    if (!writeJson(QDir(staging).filePath("mmc-pack.json"), pack, error) ||
        (!standardMetadataOnly && !writeJson(QDir(staging).filePath("patches/net.minecraft.json"), basePatch, error)) ||
        !writeIni(QDir(staging).filePath("instance.cfg"), options.name.isEmpty() ? QFileInfo(sourceRoot).fileName() : options.name, error)) {
        report.error = error; FS::deletePath(staging); return report;
    }
    if (QFileInfo(QDir(staging).filePath("assets")).isDir()) {
        auto assetId = resolvedVersion.value("assetIndex").toObject().value("id").toString(resolvedVersion.value("assets").toString("legacy"));
        const auto indexPath = QDir(layout.assetsRoot).filePath("indexes/" + assetId + ".json");
        QFile indexFile(indexPath);
        const auto indexBytes = indexFile.open(QIODevice::ReadOnly) ? indexFile.readAll() : QByteArray{};
        const auto objects = QJsonDocument::fromJson(indexBytes).object().value("objects").toObject();
        bool complete = !objects.isEmpty();
        for (auto it = objects.begin(); it != objects.end(); ++it) {
            const auto hash = it.value().toObject().value("hash").toString();
            if (hash.size() != 40 || (!QFileInfo(QDir(layout.assetsRoot).filePath("objects/"+hash.left(2)+'/'+hash)).isFile()
                && !QFileInfo(QDir(layout.assetsRoot).filePath("virtual/"+assetId+'/'+it.key())).isFile())) complete = false;
        }
        const bool officialLegacy = assetId == "legacy" && QCryptographicHash::hash(indexBytes,QCryptographicHash::Sha1).toHex() == "c0fd82e8ce9fbc93119e40d96d5a4e62cfa3f729";
        const bool remoteIndex = !resolvedVersion.value("assetIndex").toObject().value("url").toString().isEmpty();
        const bool local = complete || (!officialLegacy && !remoteIndex);
        QFile config(QDir(staging).filePath("instance.cfg"));
        if (!config.open(QIODevice::Append) || config.write(local ? "UseLocalAssets=true\n" : "UseLocalAssets=false\n") < 0) {
            report.error = QObject::tr("Could not configure the imported asset cache."); FS::deletePath(staging); return report;
        }
        if (!complete) report.warnings.append(local ? QObject::tr("The custom asset bundle is incomplete; its files are preserved but cannot be repaired without a verified index source.")
            : QObject::tr("The bundled official assets are incomplete; the standard asset downloader will complete them."));
    }
    if (!loaderUid.isEmpty() && !standardMetadataOnly)
        report.warnings.append(QObject::tr("The full imported runtime is preserved in the base component; the loader component records its identity."));
    else if (!standardMetadataOnly)
        report.warnings.append(QObject::tr("The resolved vanilla version JSON was stored as a Prism net.minecraft component patch."));
    if (cancelled(options.feedback, report.error)) { FS::deletePath(staging); return report; }
    options.feedback.update(QObject::tr("正在写入实例目录"));
    if (!QDir().rename(staging, prismTarget)) { report.error = QObject::tr("Could not commit the Prism conversion."); FS::deletePath(staging); return report; }
    report.warnings.append(QObject::tr("The source vanilla directory was left unchanged."));
    report.destination = prismTarget;
    report.succeeded = true;
    Q_UNUSED(options.keepSource);
    return report;
}

VanillaConversionReport VanillaConverter::importToVanilla(const QString& sourceRoot, const QString& vanillaTarget, const VanillaImportOptions& options)
{
    options.feedback.update(QObject::tr("正在解析实例配置"));
    VanillaConversionReport report;
    const auto layout = detectVanillaInstanceLayout(sourceRoot);
    const auto targetId = QFileInfo(vanillaTarget).fileName();
    if (!layout.isValid() || targetId.isEmpty() || QFileInfo(vanillaTarget).dir().dirName() != "versions") {
        report.error = QObject::tr("A valid source profile and vanilla versions destination are required."); return report;
    }
    if (QFileInfo::exists(vanillaTarget)) {
        report.error = QObject::tr("The destination already exists: %1").arg(vanillaTarget); return report;
    }
    const auto staging = vanillaTarget + ".tmp-import-" + QString::number(QRandomGenerator::global()->generate());
    const auto gameRoot = QDir(vanillaTarget).absoluteFilePath("../..");
    QString error;
    QJsonObject raw;
    if (!readJsonObject(layout.versionJson, raw, error)) { report.error = error; return report; }
    auto resolved = VanillaCompatibleProvider::resolvedProfile(loadVanillaVersionJson(QDir(layout.versionsRoot).absoluteFilePath(".."),layout.versionId,&error));
    if (!error.isEmpty() || resolved.isEmpty()) { report.error = error.isEmpty() ? QObject::tr("Could not resolve the source profile.") : error; return report; }
    if (!QFileInfo(layout.clientJar).isFile() && !hasClientDownload(resolved)) {
        report.error = QObject::tr("The source client JAR is missing and the resolved profile has no client download URL."); return report;
    }
    QDir().mkpath(staging);
    auto excluded = launcherFiles;
    excluded.unite(QSet<QString>{ "instance.cfg", "mmc-pack.json", "patches", "luna.settings.json" });
    excluded.insert((layout.versionId + ".json").toLower());
    excluded.insert((layout.versionId + ".jar").toLower());
    const auto fail = [&]() { report.error = error; FS::deletePath(staging); return report; };
    if (options.copyGameContent && !copyTree(layout.gameRoot, staging, excluded, report.copied, error, options.feedback, QObject::tr("正在复制实例文件"))) return fail();
    // Only immutable caches and explicitly referenced parent profiles are
    // shared. Conflicting existing artifacts are never replaced.
    QMap<QString, QString> artifacts;
    options.feedback.update(QObject::tr("正在扫描运行库和资源文件"));
    for (const auto& folder : { QStringLiteral("libraries"), QStringLiteral("assets") }) {
        const auto source = folder == "libraries" ? layout.librariesRoot : layout.assetsRoot;
        if (!QFileInfo(source).isDir()) continue;
        QDirIterator it(source, QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            if (cancelled(options.feedback, error)) return fail();
            const auto path = it.next();
            const auto relative = QDir(source).relativeFilePath(path);
            if (it.fileInfo().isSymLink() || !safeRelative(relative)) { error = QObject::tr("Unsafe source artifact: %1").arg(path); return fail(); }
            artifacts.insert(QDir(gameRoot).filePath(folder + '/' + relative), path);
            options.feedback.update(QObject::tr("正在扫描运行库和资源文件"), artifacts.size(), 0, "files", relative);
        }
    }
    QSet<QString> parents;
    QStringList pending;
    for (const auto& key : { "inheritsFrom", "jar" }) {
        const auto id = raw.value(key).toString();
        if (!id.isEmpty() && id != layout.versionId) pending.append(id);
    }
    while (!pending.isEmpty()) {
        const auto id = pending.takeFirst();
        if (parents.contains(id)) continue;
        if (id.contains('/') || id.contains('\\') || id.contains(':') || id == ".." || id == ".") { error = QObject::tr("Unsafe parent profile ID."); return fail(); }
        if (id.compare(targetId,Qt::CaseInsensitive) == 0) { error = QObject::tr("The destination ID conflicts with the inherited profile: %1").arg(id); return fail(); }
        parents.insert(id);
        const auto parentRoot = QDir(layout.versionsRoot).filePath(id);
        const auto parent = readObject(QDir(parentRoot).filePath(id + ".json"));
        for (const auto& suffix : { ".json", ".jar" }) {
            const auto file = QDir(parentRoot).filePath(id + suffix);
            if (QFileInfo(file).isFile()) artifacts.insert(QDir(gameRoot).filePath("versions/" + id + '/' + id + suffix), file);
        }
        for (const auto& key : { "inheritsFrom", "jar" }) {
            const auto next = parent.value(key).toString();
            if (!next.isEmpty() && next != id) pending.append(next);
        }
        if (raw.value("inheritsFrom").toString() == id && parent.isEmpty()) {
            error = QObject::tr("The inherited profile is missing: %1").arg(id); return fail();
        }
    }
    options.feedback.update(QObject::tr("正在校验客户端并生成运行配置"));
    const auto originalProfile = raw;
    ImportedClientProfile::inferJavaRequirement(resolved,layout.librariesRoot,layout.clientJar);
    if (!raw.contains("javaVersion") && resolved.contains("javaVersion")) raw.insert("javaVersion",resolved.value("javaVersion"));
    raw.insert("id", targetId);
    for (const auto& key : {"gameDir","gameDirectory","gameRoot"}) raw.remove(key);
    if (QFileInfo(layout.clientJar).isFile()) {
        report.clientArtifact = ImportedClientProfile::inspectClient(layout.clientJar,raw,&error);
        const auto client = QDir(staging).filePath(targetId + ".jar");
        if (report.clientArtifact.isEmpty() || !QFile::copy(layout.clientJar,client)
            || !ImportedClientProfile::verifyClient(client, report.clientArtifact,&error)) {
            if (error.isEmpty()) error = QObject::tr("Could not preserve the source client JAR.");
            return fail();
        }
        report.clientArtifact.insert("path",targetId + ".jar");
        raw.insert("jar",targetId);
        if (report.clientArtifact.value("comparison").toString() != "matches-profile") {
            auto downloads = raw.value("downloads").toObject();
            downloads.remove("client"); raw.insert("downloads",downloads);
        }
    } else if (raw.value("jar").toString() == layout.versionId) raw.insert("jar",targetId);
    if (!writeJson(QDir(staging).filePath(targetId + ".json"),raw,error)
        || !writeJson(QDir(staging).filePath("luna-imported/source-profile.json"),originalProfile,error)) return fail();
    if (!report.clientArtifact.isEmpty()
        && !writeJson(QDir(staging).filePath("luna-imported/client-artifact.json"),report.clientArtifact,error)) return fail();
    auto hmcl = readObject(QDir(QFileInfo(layout.versionJson).absolutePath()).filePath("hmclversion.cfg"));
    hmcl.insert("usesGlobal", false); hmcl.insert("gameDirType", 1); hmcl.remove("gameDir");
    if (!writeJson(QDir(staging).filePath("hmclversion.cfg"), hmcl, error)
        || !writeJson(QDir(staging).filePath("luna.settings.json"), QJsonObject{{"formatVersion",1}, {"clientArtifact",report.clientArtifact}, {"settings",QJsonObject{
            {"name", options.name.isEmpty() ? targetId : options.name}, {"group",options.group}, {"VanillaGameDirectoryMode","isolated"}}}}, error)) return fail();
    // PCL's isolation flag must agree with HMCL and Luna.
    const auto pclPath = QDir(staging).filePath("PCL/Setup.ini");
    if (QFileInfo::exists(pclPath)) {
        QFile file(pclPath);
        if (!file.open(QIODevice::ReadOnly)) { error = file.errorString(); return fail(); }
        auto lines = QString::fromUtf8(file.readAll()).split('\n'); file.close();
        for (auto& line : lines)
            if (line.startsWith("VersionArgumentIndie:") || line.startsWith("VersionArgumentIndieV2:")) line.clear();
        lines.append("VersionArgumentIndie:1"); lines.append("VersionArgumentIndieV2:True");
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(lines.join('\n').toUtf8()) < 0) { error = file.errorString(); return fail(); }
    }
    qint64 processed = 0;
    options.feedback.update(QObject::tr("正在复制并校验运行库和资源文件"), 0, artifacts.size());
    for (auto it = artifacts.begin(); it != artifacts.end(); ++it) {
        if (cancelled(options.feedback, error) || !copyArtifact(it.value(), it.key(), error)) return fail();
        options.feedback.update(QObject::tr("正在复制并校验运行库和资源文件"), ++processed, artifacts.size(), "files", QDir(gameRoot).relativeFilePath(it.key()));
    }
    if (cancelled(options.feedback, error)) return fail();
    options.feedback.update(QObject::tr("正在写入实例目录"));
    if (!QDir().rename(staging, vanillaTarget)) { error = QObject::tr("Could not commit the vanilla directory pack."); return fail(); }
    report.succeeded = true; report.destination = vanillaTarget;
    return report;
}

QString VanillaConverter::gameRootForStore(const QString& storePath)
{
    const QDir store(storePath);
    if (store.dirName().compare(QStringLiteral(".minecraft"), Qt::CaseInsensitive) == 0)
        return store.absolutePath();
    if (QFileInfo(store.filePath(".minecraft")).isDir())
        return store.absoluteFilePath(".minecraft");
    if (QFileInfo(store.filePath("versions")).isDir())
        return store.absolutePath();
    return store.absoluteFilePath(".minecraft");
}

VanillaConversionReport VanillaConverter::exportToVanilla(const QString& prismRoot, const QString& vanillaTarget, const VanillaExportOptions& options)
{
    options.feedback.update(QObject::tr("正在检查并转换实例配置"));
    VanillaConversionReport report;
    const auto compatibility = checkVanillaExportCompatibility(prismRoot);
    report.blockers = compatibility.blockers;
    report.warnings = compatibility.warnings;
    if (!compatibility.compatible) {
        report.error = QObject::tr("The Prism instance contains data that cannot be represented in vanilla format.") + '\n' + report.blockers.join('\n');
        return report;
    }
    const auto versionId = options.versionId.trimmed();
    if (versionId.isEmpty() || versionId == "." || versionId == ".." || versionId.contains('/') || versionId.contains('\\')) {
        report.error = QObject::tr("A valid target version ID is required.");
        return report;
    }
    if (options.isolated && (QFileInfo(vanillaTarget).fileName() != versionId || QFileInfo(vanillaTarget).dir().dirName() != "versions")) {
        report.error = QObject::tr("An isolated vanilla destination must be versions/<version ID>.");
        return report;
    }
    if (QFileInfo::exists(vanillaTarget)) {
        report.error = QObject::tr("The vanilla destination already exists: %1").arg(vanillaTarget);
        return report;
    }
    QJsonObject version, base;
    QMap<QString, QString> artifacts;
    if (!vanillaProfile(prismRoot, options, version, base, artifacts, report.error))
        return report;
    // Extensions cannot replace the launch contract validated above.
    for (auto it = options.extensionFields.begin(); it != options.extensionFields.end(); ++it) {
        const QSet<QString> internalFields{ "uid", "version", "formatVersion", "mainJar", "requires", "conflicts", "mavenFiles", "jarMods", "mods" };
        if (version.contains(it.key()) || internalFields.contains(it.key()) || it.key().startsWith("MMC-") || it.key().startsWith('+') || it.key().startsWith('-')) {
            report.error = QObject::tr("Extension field would replace vanilla launch metadata: %1").arg(it.key());
            return report;
        }
        version.insert(it.key(), it.value());
    }
    const auto staging = vanillaTarget + QStringLiteral(".tmp-convert-") + QString::number(QRandomGenerator::global()->generate());
    const auto versionRoot = options.isolated ? staging : QDir(staging).filePath("versions/" + versionId);
    const auto sharedRoot = options.isolated ? QDir::cleanPath(QDir(vanillaTarget).filePath("../..")) : staging;
    const auto fail = [&report, &staging]() { QDir(staging).removeRecursively(); return report; };
    if (!QDir().mkpath(versionRoot)) {
        report.error = QObject::tr("Could not create the vanilla staging directory.");
        return fail();
    }
    auto sourceGameRoot = QDir(prismRoot).filePath(".minecraft");
    if (!QFileInfo(sourceGameRoot).isDir()) sourceGameRoot = QDir(prismRoot).filePath("minecraft");
    if (!QFileInfo(sourceGameRoot).isDir()) sourceGameRoot = prismRoot;
    const auto contentRoot = options.isolated ? versionRoot : staging;
    if (options.copyGameContent && !copyTree(sourceGameRoot, contentRoot,
            { "instance.cfg", "mmc-pack.json", "patches", "bin", "natives", "libraries", "assets", "versions", "luna-imported", "luna.settings.json" },
            report.copied, report.error, options.feedback, QObject::tr("正在复制实例文件")))
        return fail();
    // PCL/HMCL version isolation uses ordinary content directly beside the
    // version JSON. An empty mods directory also makes a fresh instance
    // independently discoverable before its first launch.
    if (options.isolated && !QDir().mkpath(QDir(versionRoot).filePath("mods"))) {
        report.error = QObject::tr("Could not create the isolated game directory.");
        return fail();
    }
    const auto assetId = version.value("assetIndex").toObject().value("id").toString();
    if (!safeRelative(QDir::cleanPath(assetId)) || assetId.contains('/')) {
        report.error = QObject::tr("Invalid asset index ID.");
        return fail();
    }
    const auto indexPath = QDir(options.assetsRoot).filePath("indexes/" + assetId + ".json");
    if (!options.assetsRoot.isEmpty() && QFileInfo(indexPath).isFile()) {
        artifacts.insert("assets/indexes/" + assetId + ".json", indexPath);
        const auto objects = readObject(indexPath).value("objects").toObject();
        for (auto it = objects.begin(); it != objects.end(); ++it) {
            const auto hash = it.value().toObject().value("hash").toString();
            if (!QRegularExpression(QStringLiteral("^[0-9a-fA-F]{40}$")).match(hash).hasMatch()) {
                report.error = QObject::tr("Invalid asset hash in %1.").arg(indexPath);
                return fail();
            }
            const auto path = "objects/" + hash.left(2) + '/' + hash;
            artifacts.insert("assets/" + path, QDir(options.assetsRoot).filePath(path));
        }
    }
    const auto baseId = base.value("id").toString();
    qint64 processed = 0;
    options.feedback.update(QObject::tr("正在复制并校验运行库和资源文件"), 0, artifacts.size());
    for (auto it = artifacts.begin(); it != artifacts.end(); ++it) {
        if (cancelled(options.feedback, report.error)) return fail();
        auto destination = QDir(sharedRoot).filePath(it.key());
        if (options.isolated && it.key().startsWith("versions/" + versionId + '/'))
            destination = QDir(staging).filePath(it.key().mid(QString("versions/" + versionId + '/').size()));
        if (!copyArtifact(it.value(), destination, report.error)) return fail();
        options.feedback.update(QObject::tr("正在复制并校验运行库和资源文件"), ++processed, artifacts.size(), "files", it.key());
    }
    if (baseId != versionId) {
        const auto basePath = QDir(sharedRoot).filePath(QStringLiteral("versions/%1/%1.json").arg(baseId));
        if (!QFileInfo::exists(basePath) && !writeJson(basePath, base, report.error)) return fail();
    }
    if (!writeJson(QDir(versionRoot).filePath(versionId + ".json"), version, report.error)) return fail();
    { // Persist the chosen mode even for a fresh unnamed profile.
        const QJsonObject settings{ { "formatVersion", 1 }, { "settings", QJsonObject{ { "name", options.name }, { "group", options.group }, { "VanillaGameDirectoryMode", options.isolated ? "isolated" : "shared" } } } };
        if (!writeJson(QDir(versionRoot).filePath("luna.settings.json"), settings, report.error)) return fail();
    }
    if (cancelled(options.feedback, report.error)) return fail();
    options.feedback.update(QObject::tr("正在写入实例目录"));
    if (!QDir().rename(staging, vanillaTarget)) {
        report.error = QObject::tr("Could not commit the vanilla conversion.");
        return fail();
    }
    report.destination = vanillaTarget;
    report.succeeded = true;
    return report;
}

VanillaConversionReport VanillaConverter::installLoader(const QString& vanillaRoot, const VanillaLoaderInstallOptions& options)
{
    VanillaConversionReport report;
    const auto loaderType = VanillaCompatibleProvider::loaderTypeForUid(options.loaderId);
    if (loaderType.isEmpty()) {
        report.error = QObject::tr("Unsupported standard loader: %1").arg(options.loaderId);
        return report;
    }
    if (options.versionId.trimmed().isEmpty() || options.loaderVersion.trimmed().isEmpty()) {
        report.error = QObject::tr("A Minecraft version and loader version are required.");
        return report;
    }
    const auto layout = detectVanillaInstanceLayout(vanillaRoot, options.versionId);
    if (!layout.isValid()) {
        report.error = QObject::tr("The target vanilla directory has no matching Minecraft version.");
        return report;
    }
    if (options.versionJson.isEmpty()) {
        report.error = QObject::tr("A complete loader version profile is required. Resolve the loader metadata before installing it.");
        return report;
    }
    auto version = VanillaCompatibleProvider::makeInheritedLoaderProfile(options.versionId,
                                                                          loaderType,
                                                                          options.loaderVersion,
                                                                          options.versionJson,
                                                                          options.clientJar);
    if (version.isEmpty()) {
        report.error = QObject::tr("The loader metadata could not be converted to a standard version profile.");
        return report;
    }
    const auto targetId = version.value(QStringLiteral("id")).toString();
    // In HMCL/PCL version-isolated layouts the directory passed to this
    // operation can itself be `versions/<instance>`.  Keep sibling profiles
    // beside the existing instance JSON instead of creating the invalid
    // `versions/<instance>/versions/<loader>` nesting.  Conventional shared
    // layouts continue to use the outer `versions/<id>` directory.
    const auto targetDir = QFileInfo(layout.sourceRoot).dir().dirName() == QStringLiteral("versions")
        ? layout.sourceRoot
        : QDir(vanillaRoot).filePath(QStringLiteral("versions/%1").arg(targetId));
    QString error;
    if (!writeJson(QDir(targetDir).filePath(targetId + QStringLiteral(".json")), version, error)) {
        report.error = error;
        return report;
    }
    // Keep an explicit Luna change record beside an external instance. This
    // lets later scans distinguish a loader installed by Luna from a loader
    // that was already present in HMCL/PCL/the official launcher.
    const auto settingsPath = QDir(targetDir).filePath(QStringLiteral("luna.settings.json"));
    QJsonObject sidecar;
    QFile sidecarFile(settingsPath);
    if (sidecarFile.open(QIODevice::ReadOnly)) {
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(sidecarFile.readAll(), &parseError);
        if (parseError.error == QJsonParseError::NoError && document.isObject())
            sidecar = document.object();
    }
    auto settings = sidecar.value(QStringLiteral("settings")).toObject();
    settings.insert(QStringLiteral("loaderEnabled"), true);
    settings.insert(QStringLiteral("loaderTypeOverride"), loaderType);
    settings.insert(QStringLiteral("loaderVersionOverride"), options.loaderVersion);
    settings.insert(QStringLiteral("loaderProfileOverride"), options.versionJson);
    QFile installedVersionFile(QDir(targetDir).filePath(targetId + QStringLiteral(".json")));
    if (installedVersionFile.open(QIODevice::ReadOnly))
        settings.insert(QStringLiteral("loaderManifestFingerprint"), QString::fromLatin1(QCryptographicHash::hash(installedVersionFile.readAll(), QCryptographicHash::Sha256).toHex()));
    sidecar.insert(QStringLiteral("formatVersion"), 1);
    sidecar.insert(QStringLiteral("settings"), settings);
    sidecar.insert(QStringLiteral("lunaChanges"), QJsonObject{
        { "actions", QJsonArray{ QStringLiteral("settings"), QStringLiteral("loader") } },
        { "substantial", true },
        { "loaderType", loaderType },
        { "loaderVersion", options.loaderVersion },
        { "loaderEnabled", true },
    });
    if (!writeJson(settingsPath, sidecar, error)) {
        report.error = error;
        return report;
    }
    report.copied.append(QDir(targetDir).relativeFilePath(targetDir + QStringLiteral("/%1.json").arg(targetId)));
    report.warnings.append(QObject::tr("The loader profile inherits the base Minecraft version. Libraries from its standard download entries will be repaired by the launcher that starts it."));
    report.succeeded = true;
    return report;
}

