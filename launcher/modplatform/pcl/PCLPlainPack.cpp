// SPDX-License-Identifier: GPL-3.0-only

#include "PCLPlainPack.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QtEndian>
#include <algorithm>

#include "Exception.h"
#include "FileSystem.h"
#include "archive/ArchiveReader.h"
#include "minecraft/Component.h"
#include "minecraft/GradleSpecifier.h"
#include "minecraft/Library.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/MojangDownloadInfo.h"
#include "minecraft/MojangVersionFormat.h"
#include "minecraft/OneSixVersionFormat.h"
#include "minecraft/PackProfile.h"
#include "minecraft/VersionFile.h"
#include "modplatform/pcl/PCLPack.h"

namespace PCL {

int classFileJavaMajor(const QByteArray& data)
{
    if (data.size() < 8)
        return 0;
    const auto* bytes = reinterpret_cast<const uchar*>(data.constData());
    if (qFromBigEndian<quint32>(bytes) != 0xCAFEBABE)
        return 0;
    const auto classVersion = qFromBigEndian<quint16>(bytes + 6);
    return classVersion >= 45 ? classVersion - 44 : 0;
}

bool isFishModLoaderLibrary(const QString& coordinate)
{
    const GradleSpecifier spec(coordinate);
    return spec.valid() && spec.groupId() == QLatin1String("net.xiaoyu233.fishmodloader") &&
           spec.artifactId() == QLatin1String("fishmodloader");
}

namespace {

constexpr auto MITE_COMPONENT_UID = "org.lunalauncher.mite";
constexpr auto FISH_MOD_LOADER_UID = "net.xiaoyu233.fishmodloader";
constexpr auto FISH_MOD_LOADER_MAIN_CLASS = "net.xiaoyu233.fml.relaunch.client.Main";
constexpr auto OFFICIAL_LEGACY_INDEX_SHA1 = "c0fd82e8ce9fbc93119e40d96d5a4e62cfa3f729";

struct AssetBundleInfo {
    bool hasIndex = false;
    bool validIndex = false;
    bool officialLegacyIndex = false;
    int totalEntries = 0;
    int presentEntries = 0;

    bool complete() const { return validIndex && totalEntries > 0 && presentEntries == totalEntries; }
};

struct FishModLoaderInfo {
    qsizetype libraryIndex = -1;
    QString version;
    QString sourcePath;

    bool detected() const { return libraryIndex >= 0; }
};

void record(QJsonArray& entries, const QString& key, const QString& status, const QString& detail = {})
{
    QJsonObject item{ { "key", key }, { "status", status } };
    if (!detail.isEmpty())
        item.insert("detail", detail);
    entries.append(item);
}

bool readJson(const QString& path, QJsonObject& object, QString& error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = QObject::tr("Could not open PCL version JSON %1: %2").arg(path, file.errorString());
        return false;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        error = QObject::tr("Could not parse PCL version JSON %1: %2").arg(path, parseError.errorString());
        return false;
    }
    object = document.object();
    return true;
}

QJsonArray concatChildFirst(const QJsonValue& child, const QJsonValue& parent)
{
    QJsonArray result;
    for (const auto& value : child.toArray())
        result.append(value);
    for (const auto& value : parent.toArray())
        result.append(value);
    return result;
}

QJsonObject mergeVersion(const QJsonObject& parent, const QJsonObject& child)
{
    QJsonObject result = parent;
    for (auto it = child.begin(); it != child.end(); ++it)
        result.insert(it.key(), it.value());

    result.insert("libraries", concatChildFirst(child.value("libraries"), parent.value("libraries")));
    QJsonObject arguments = parent.value("arguments").toObject();
    const auto childArguments = child.value("arguments").toObject();
    for (const auto& key : { "game", "jvm" }) {
        if (childArguments.contains(key) || arguments.contains(key))
            arguments.insert(key, concatChildFirst(childArguments.value(key), arguments.value(key)));
    }
    if (!arguments.isEmpty())
        result.insert("arguments", arguments);
    result.remove("inheritsFrom");
    return result;
}

bool resolveVersion(const QString& sourceRoot,
                    const QString& version,
                    QSet<QString>& resolving,
                    QJsonObject& result,
                    QString& error,
                    QJsonArray& entries)
{
    if (resolving.contains(version)) {
        error = QObject::tr("PCL version inheritance contains a cycle at %1.").arg(version);
        return false;
    }
    resolving.insert(version);

    const auto path = FS::PathCombine(sourceRoot, "versions", version, version + ".json");
    QJsonObject child;
    if (!readJson(path, child, error))
        return false;

    const auto parentVersion = child.value("inheritsFrom").toString();
    if (parentVersion.isEmpty()) {
        result = child;
    } else {
        QJsonObject parent;
        if (!resolveVersion(sourceRoot, parentVersion, resolving, parent, error, entries)) {
            error = QObject::tr("Could not resolve inherited PCL version %1: %2").arg(parentVersion, error);
            return false;
        }
        result = mergeVersion(parent, child);
        record(entries, "inheritsFrom", "merged", parentVersion);
    }
    resolving.remove(version);
    return true;
}

bool copyFile(const QString& source, const QString& target, QString& error)
{
    if (!FS::ensureFilePathExists(target)) {
        error = QObject::tr("Could not create the destination for %1.").arg(target);
        return false;
    }
    QFile::remove(target);
    if (!QFile::copy(source, target)) {
        error = QObject::tr("Could not copy %1 to %2.").arg(source, target);
        return false;
    }
    return true;
}

bool mergeDirectory(const QString& source, const QString& target, QString& error)
{
    if (!QFileInfo::exists(source))
        return true;
    if (!FS::overrideFolder(target, source)) {
        error = QObject::tr("Could not copy PCL game files from %1 to %2.").arg(source, target);
        return false;
    }
    return true;
}

bool copyGameFiles(const QString& sourceRoot, const QString& version, const QString& gameRoot, QString& error)
{
    if (!FS::ensureFolderPathExists(gameRoot)) {
        error = QObject::tr("Could not create the instance game directory.");
        return false;
    }

    const QSet<QString> excludedRoot{
        "assets", "libraries", "versions", "pcl", "pcl.ini", "launcher_profiles.json", "launcher_accounts.json"
    };
    QDir root(sourceRoot);
    for (const auto& entry : root.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries)) {
        if (excludedRoot.contains(entry.fileName().toLower()))
            continue;
        const auto target = FS::PathCombine(gameRoot, entry.fileName());
        if (entry.isDir()) {
            if (!mergeDirectory(entry.absoluteFilePath(), target, error))
                return false;
        } else if (!copyFile(entry.absoluteFilePath(), target, error)) {
            return false;
        }
    }

    const auto versionRoot = FS::PathCombine(sourceRoot, "versions", version);
    QDir versionDir(versionRoot);
    for (const auto& entry : versionDir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries)) {
        if (entry.fileName().compare(version + ".json", Qt::CaseInsensitive) == 0 ||
            entry.fileName().compare(version + ".jar", Qt::CaseInsensitive) == 0)
            continue;
        const auto target = FS::PathCombine(gameRoot, entry.fileName());
        if (entry.isDir()) {
            if (!mergeDirectory(entry.absoluteFilePath(), target, error))
                return false;
        } else if (!copyFile(entry.absoluteFilePath(), target, error)) {
            return false;
        }
    }
    return true;
}

QString originalMinecraftVersion(const QString& sourceRoot, const QString& version, QJsonArray& entries)
{
    QFile setupFile(FS::PathCombine(sourceRoot, "versions", version, FS::PathCombine("PCL", "Setup.ini")));
    if (setupFile.open(QIODevice::ReadOnly)) {
        const auto setup = parseSetup(setupFile.readAll());
        const auto original = setup.values.value("VersionOriginal").trimmed();
        if (!original.isEmpty()) {
            record(entries, "VersionOriginal", "mapped", original);
            return original;
        }
    }

    static const QRegularExpression versionPrefix(R"(^([0-9]+\.[0-9]+(?:\.[0-9]+)?))");
    const auto match = versionPrefix.match(version);
    if (match.hasMatch()) {
        record(entries, "minecraftVersion", "mapped-approximate",
               QObject::tr("Inferred %1 from the custom version ID %2.").arg(match.captured(1), version));
        return match.captured(1);
    }
    return version;
}

int classJavaMajorInArchive(const QString& archivePath, const QString& className)
{
    if (archivePath.isEmpty() || className.isEmpty())
        return 0;
    auto classPath = className;
    classPath.replace('.', '/');
    classPath += ".class";

    MMCZip::ArchiveReader archive(archivePath);
    auto file = archive.goToFile(classPath);
    if (!file)
        return 0;
    int status = 0;
    const auto data = file->readAll(&status);
    return status < 0 ? 0 : classFileJavaMajor(data);
}

AssetBundleInfo inspectAssetBundle(const QString& assetsRoot, const QString& assetId)
{
    AssetBundleInfo result;
    const auto indexPath = FS::PathCombine(assetsRoot, "indexes", assetId + ".json");
    QFile indexFile(indexPath);
    if (!indexFile.open(QIODevice::ReadOnly))
        return result;

    result.hasIndex = true;
    const auto indexData = indexFile.readAll();
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(indexData, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject() || !document.object().value("objects").isObject())
        return result;

    result.validIndex = true;
    result.officialLegacyIndex =
        assetId == QLatin1String("legacy") &&
        QCryptographicHash::hash(indexData, QCryptographicHash::Sha1).toHex() == QByteArray(OFFICIAL_LEGACY_INDEX_SHA1);
    const auto objects = document.object().value("objects").toObject();
    const QDir objectRoot(FS::PathCombine(assetsRoot, "objects"));
    const QDir virtualRoot(FS::PathCombine(assetsRoot, "virtual", assetId));
    static const QRegularExpression hashPattern(QRegularExpression::anchoredPattern("[0-9a-fA-F]{40}"));
    for (auto it = objects.constBegin(); it != objects.constEnd(); ++it) {
        ++result.totalEntries;
        const auto hash = it.value().toObject().value("hash").toString();
        if (!hashPattern.match(hash).hasMatch())
            continue;
        const auto objectPath = objectRoot.filePath(hash.left(2) + '/' + hash);
        auto virtualPath = QDir::cleanPath(it.key());
        const bool safeVirtualPath = !QDir::isAbsolutePath(virtualPath) && virtualPath != ".." && !virtualPath.startsWith("../");
        if (QFileInfo(objectPath).isFile() || (safeVirtualPath && QFileInfo(virtualRoot.filePath(virtualPath)).isFile()))
            ++result.presentEntries;
    }
    return result;
}

void requireComponent(const VersionFilePtr& component, const QString& uid, const QString& version)
{
    Meta::Require requirement;
    requirement.uid = uid;
    requirement.equalsVersion = version;
    component->m_requires.insert(requirement);
}

bool argumentRulesAllow(const QJsonArray& rules)
{
    if (rules.isEmpty())
        return true;
    for (const auto& value : rules) {
        if (value.toObject().contains("features"))
            return false;
    }
    bool allowed = false;
#if defined(Q_OS_WIN)
    const QString currentOs = "windows";
#elif defined(Q_OS_MACOS)
    const QString currentOs = "osx";
#else
    const QString currentOs = "linux";
#endif
    for (const auto& value : rules) {
        const auto rule = value.toObject();
        const auto os = rule.value("os").toObject().value("name").toString();
        if (!os.isEmpty() && os != currentOs)
            continue;
        allowed = rule.value("action").toString() == "allow";
    }
    return allowed;
}

QStringList flattenArguments(const QJsonArray& source)
{
    QStringList result;
    for (const auto& value : source) {
        if (value.isString()) {
            result.append(value.toString());
            continue;
        }
        const auto object = value.toObject();
        if (!argumentRulesAllow(object.value("rules").toArray()))
            continue;
        const auto argument = object.value("value");
        if (argument.isString())
            result.append(argument.toString());
        else
            for (const auto& item : argument.toArray())
                result.append(item.toString());
    }
    return result;
}

QString quoteArgument(QString argument)
{
    if (!argument.contains(QRegularExpression("[\\s\"]")))
        return argument;
    argument.replace('\\', "\\\\");
    argument.replace('"', "\\\"");
    return '"' + argument + '"';
}

void reportConditionalArgumentLimits(const QJsonArray& arguments, const QString& key, QJsonArray& entries)
{
    bool hasFeatureRules = false;
    bool hasDetailedOsRules = false;
    for (const auto& value : arguments) {
        const auto rules = value.toObject().value("rules").toArray();
        for (const auto& ruleValue : rules) {
            const auto rule = ruleValue.toObject();
            hasFeatureRules = hasFeatureRules || rule.contains("features");
            const auto os = rule.value("os").toObject();
            hasDetailedOsRules = hasDetailedOsRules || os.contains("version") || os.contains("arch");
        }
    }
    if (hasFeatureRules) {
        record(entries, key + ".feature-rules", "unsupported",
               QObject::tr("Feature-dependent arguments cannot be represented in the legacy MMC argument string and were omitted."));
    }
    if (hasDetailedOsRules) {
        record(entries, key + ".os-rules", "mapped-platform",
               QObject::tr("OS version or architecture constraints were approximated using the current operating system."));
    }
}

void convertArguments(QJsonObject& version, QJsonArray& entries)
{
    if (!version.value("minecraftArguments").toString().isEmpty())
        return;
    const auto arguments = version.value("arguments").toObject();
    reportConditionalArgumentLimits(arguments.value("game").toArray(), "arguments.game", entries);
    reportConditionalArgumentLimits(arguments.value("jvm").toArray(), "arguments.jvm", entries);
    const auto game = flattenArguments(arguments.value("game").toArray());
    if (!game.isEmpty()) {
        QStringList quoted;
        for (const auto& argument : game)
            quoted.append(quoteArgument(argument));
        version.insert("minecraftArguments", quoted.join(' '));
        record(entries, "arguments.game", "mapped-platform", QObject::tr("Converted for the current operating system."));
    }
}

QStringList convertedJvmArguments(const QJsonObject& version, QJsonArray& entries)
{
    const auto source = flattenArguments(version.value("arguments").toObject().value("jvm").toArray());
    QStringList result;
    bool skipNext = false;
    for (const auto& argument : source) {
        if (skipNext) {
            skipNext = false;
            continue;
        }
        if (argument == "-cp" || argument == "-classpath") {
            skipNext = true;
            continue;
        }
        if (argument.contains("${classpath}") || argument.contains("${natives_directory}") || argument.contains("${launcher_name}") ||
            argument.contains("${launcher_version}"))
            continue;
        result.append(argument);
    }
    if (!result.isEmpty())
        record(entries, "arguments.jvm", "mapped-platform", QObject::tr("Launcher-owned classpath and native arguments were removed."));
    return result;
}

QSet<QString> sanitizeDownloads(QJsonObject& version, QJsonArray& entries)
{
    QSet<QString> repaired;
    auto libraries = version.value("libraries").toArray();
    for (qsizetype i = 0; i < libraries.size(); ++i) {
        auto library = libraries.at(i).toObject();
        const GradleSpecifier spec(library.value("name").toString());
        if (!spec.valid())
            continue;
        auto downloads = library.value("downloads").toObject();
        if (downloads.isEmpty())
            continue;

        bool changed = false;
        if (downloads.value("artifact").isObject()) {
            auto artifact = downloads.value("artifact").toObject();
            if (artifact.value("url").toString().isEmpty()) {
                downloads.remove("artifact");
                changed = true;
            } else {
                const auto declaredPath = QDir::fromNativeSeparators(artifact.value("path").toString());
                const auto expectedPath = QDir::fromNativeSeparators(spec.toPath());
                if (!declaredPath.isEmpty() && declaredPath != expectedPath) {
                    artifact.insert("path", expectedPath);
                    artifact.insert("sha1", QString());
                    artifact.insert("size", 0);
                    changed = true;
                }
                if (!artifact.value("sha1").isString()) {
                    artifact.insert("sha1", QString());
                    changed = true;
                }
                if (!artifact.value("size").isDouble()) {
                    artifact.insert("size", 0);
                    changed = true;
                }
                downloads.insert("artifact", artifact);
            }
        }

        auto classifiers = downloads.value("classifiers").toObject();
        for (auto classifier = classifiers.begin(); classifier != classifiers.end();) {
            auto info = classifier.value().toObject();
            if (info.value("url").toString().isEmpty()) {
                classifier = classifiers.erase(classifier);
                changed = true;
                continue;
            }
            if (!info.value("sha1").isString()) {
                info.insert("sha1", QString());
                changed = true;
            }
            if (!info.value("size").isDouble()) {
                info.insert("size", 0);
                changed = true;
            }
            classifier.value() = info;
            ++classifier;
        }
        if (classifiers.isEmpty())
            downloads.remove("classifiers");
        else
            downloads.insert("classifiers", classifiers);

        if (changed) {
            if (!downloads.contains("artifact") && !downloads.contains("classifiers"))
                library.remove("downloads");
            else
                library.insert("downloads", downloads);
            libraries.replace(i, library);
            const auto name = library.value("name").toString();
            repaired.insert(name);
            record(entries, library.value("name").toString(), "repaired-download-metadata",
                   QObject::tr("Incomplete or inconsistent download metadata was repaired; affected checksum validation was disabled."));
        }
    }
    version.insert("libraries", libraries);
    return repaired;
}

QString findPathCaseInsensitive(const QString& root, const QString& relativePath)
{
    QDir current(root);
    const auto parts = QDir::fromNativeSeparators(relativePath).split('/', Qt::SkipEmptyParts);
    for (qsizetype i = 0; i < parts.size(); ++i) {
        const auto& part = parts.at(i);
        const auto entries = current.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
        auto match = std::find_if(entries.cbegin(), entries.cend(), [&part](const QFileInfo& entry) { return entry.fileName() == part; });
        if (match == entries.cend()) {
            match = std::find_if(entries.cbegin(), entries.cend(),
                                 [&part](const QFileInfo& entry) { return entry.fileName().compare(part, Qt::CaseInsensitive) == 0; });
        }
        if (match == entries.cend())
            return {};
        if (i == parts.size() - 1)
            return match->absoluteFilePath();
        if (!match->isDir())
            return {};
        current.setPath(match->absoluteFilePath());
    }
    return {};
}

QString localLibrarySource(const QString& librariesRoot, const QJsonObject& library)
{
    auto path = library.value("downloads").toObject().value("artifact").toObject().value("path").toString();
    if (path.isEmpty()) {
        const GradleSpecifier spec(library.value("name").toString());
        if (!spec.valid())
            return {};
        path = spec.toPath();
    }
    const auto candidate = FS::PathCombine(librariesRoot, QDir::fromNativeSeparators(path));
    if (QFileInfo::exists(candidate))
        return candidate;
    return findPathCaseInsensitive(librariesRoot, path);
}

bool hasDownloadSource(const QJsonObject& library)
{
    const auto downloads = library.value("downloads").toObject();
    if (!downloads.value("artifact").toObject().value("url").toString().isEmpty() || !library.value("url").toString().isEmpty())
        return true;
    const auto classifiers = downloads.value("classifiers").toObject();
    for (auto classifier = classifiers.constBegin(); classifier != classifiers.constEnd(); ++classifier) {
        if (!classifier.value().toObject().value("url").toString().isEmpty())
            return true;
    }
    return false;
}

bool writeJson(const QString& path, const QJsonDocument& document, QString& error)
{
    if (!FS::ensureFilePathExists(path)) {
        error = QObject::tr("Could not create %1.").arg(path);
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(document.toJson()) < 0 || !file.commit()) {
        error = QObject::tr("Could not write %1: %2").arg(path, file.errorString());
        return false;
    }
    return true;
}

bool installLocalComponent(PackProfile* profile, const VersionFilePtr& patch, bool important, QString& error)
{
    if (!writeJson(profile->patchFilePathForUid(patch->uid), OneSixVersionFormat::versionFileToJson(patch), error))
        return false;
    auto component = makeShared<Component>(profile, patch->uid, patch);
    component->setImportant(important);
    component->updateCachedData();
    profile->appendComponent(component);
    return true;
}

}  // namespace

QList<PlainPackCandidate> findPlainPackCandidates(const QStringList& archiveFiles)
{
    QList<PlainPackCandidate> result;
    for (const auto& rawPath : archiveFiles) {
        auto path = QDir::fromNativeSeparators(rawPath);
        while (path.startsWith('/'))
            path.removeFirst();
        const auto parts = path.split('/', Qt::SkipEmptyParts);
        if (parts.size() < 3 || parts.at(parts.size() - 3).compare("versions", Qt::CaseInsensitive) != 0)
            continue;
        const auto version = parts.at(parts.size() - 2);
        if (version == "." || version == "..")
            continue;
        if (parts.constLast().compare(version + ".json", Qt::CaseInsensitive) != 0)
            continue;
        const auto rootParts = parts.mid(0, parts.size() - 3);
        result.append({ rootParts.join('/') + (rootParts.isEmpty() ? "" : "/"), version });
    }
    return result;
}

PlainPackConversionResult convertPlainPack(MinecraftInstance& instance, const QString& sourceRoot, const QString& version)
{
    PlainPackConversionResult result;
    QSet<QString> resolving;
    QJsonObject sourceVersion;
    if (!resolveVersion(sourceRoot, version, resolving, sourceVersion, result.error, result.entries))
        return result;

    const auto minecraftVersion = originalMinecraftVersion(sourceRoot, version, result.entries);
    const auto repairedDownloads = sanitizeDownloads(sourceVersion, result.entries);
    convertArguments(sourceVersion, result.entries);
    VersionFilePtr patch;
    try {
        patch = MojangVersionFormat::versionFileFromJson(QJsonDocument(sourceVersion), version + ".json");
    } catch (const Exception& exception) {
        result.error = QObject::tr("Could not convert PCL version JSON: %1").arg(exception.cause());
        return result;
    }

    patch->uid = "net.minecraft";
    patch->name = QObject::tr("Minecraft (PCL import)");
    patch->version = version;
    patch->minecraftVersion = version;
    patch->addnJvmArguments = convertedJvmArguments(sourceVersion, result.entries);
    if (patch->mainClass.isEmpty()) {
        result.error = QObject::tr("The PCL version JSON does not define a mainClass after resolving inheritance.");
        return result;
    }
    record(result.entries, "mainClass", "mapped", patch->mainClass);

    const auto sourceVersionRoot = FS::PathCombine(sourceRoot, "versions", version);
    const auto sourceJar = FS::PathCombine(sourceVersionRoot, version + ".jar");
    const auto localLibraries = instance.getLocalLibraryPath();
    const auto sourceLibraries = sourceVersion.value("libraries").toArray();
    FishModLoaderInfo fishLoader;
    for (qsizetype i = 0; i < sourceLibraries.size(); ++i) {
        const auto libraryObject = sourceLibraries.at(i).toObject();
        const auto coordinate = libraryObject.value("name").toString();
        if (!isFishModLoaderLibrary(coordinate))
            continue;
        const GradleSpecifier spec(coordinate);
        fishLoader.libraryIndex = i;
        fishLoader.version = spec.version();
        fishLoader.sourcePath = localLibrarySource(FS::PathCombine(sourceRoot, "libraries"), libraryObject);
        break;
    }
    if (patch->mainClass != QLatin1String(FISH_MOD_LOADER_MAIN_CLASS))
        fishLoader = {};

    int javaMajor = sourceVersion.value("javaVersion").toObject().value("majorVersion").toInt();
    if (javaMajor <= 0) {
        if (fishLoader.detected())
            javaMajor = classJavaMajorInArchive(fishLoader.sourcePath, patch->mainClass);
        if (javaMajor <= 0)
            javaMajor = classJavaMajorInArchive(sourceJar, patch->mainClass);
        if (javaMajor > 0) {
            record(result.entries, "javaVersion", "mapped",
                   QObject::tr("Java %1 was inferred from the imported main class bytecode.").arg(javaMajor));
        }
    }
    if (javaMajor > 0) {
        patch->compatibleJavaMajors.clear();
        patch->compatibleJavaMajors.append(javaMajor);
    }

    if (fishLoader.detected()) {
        record(result.entries, "FishModLoader", "mapped",
               QObject::tr("Detected FishModLoader %1 from its Maven coordinate and launch main class.").arg(fishLoader.version));
    }

    if (QFileInfo::exists(sourceJar)) {
        const auto jarHash = QCryptographicHash::hash(version.toUtf8(), QCryptographicHash::Sha256).toHex().left(12);
        const auto targetName = QString("pcl-main-%1.jar").arg(QString::fromLatin1(jarHash));
        if (!copyFile(sourceJar, FS::PathCombine(localLibraries, targetName), result.error))
            return result;
        auto mainJar = std::make_shared<Library>();
        mainJar->setRawName(GradleSpecifier("org.lunalauncher.pcl:mainjar:1"));
        mainJar->setFilename(targetName);
        mainJar->setHint("local");
        patch->mainJar = mainJar;
        record(result.entries, "mainJar", "mapped-local", targetName);
    } else {
        const auto clientDownload = patch->mojangDownloads.value("client");
        if (!clientDownload || clientDownload->url.isEmpty()) {
            result.error =
                QObject::tr("The PCL pack does not contain %1.jar and its version JSON does not provide a client download.").arg(version);
            return result;
        }
        auto mainJar = std::make_shared<Library>();
        mainJar->setRawName(GradleSpecifier(QString("com.mojang:minecraft:%1:client").arg(version)));
        auto download = std::make_shared<MojangLibraryDownloadInfo>();
        download->artifact = clientDownload;
        mainJar->setMojangDownloadInfo(download);
        patch->mainJar = mainJar;
        record(result.entries, "mainJar", "download-required", clientDownload->url);
    }

    for (qsizetype i = 0; i < sourceLibraries.size() && i < patch->libraries.size(); ++i) {
        const auto libraryObject = sourceLibraries.at(i).toObject();
        auto library = patch->libraries.at(i);
        const auto source = localLibrarySource(FS::PathCombine(sourceRoot, "libraries"), libraryObject);
        if (source.isEmpty()) {
            const auto name = libraryObject.value("name").toString();
            if (libraryObject.contains("natives")) {
                record(result.entries, name, hasDownloadSource(libraryObject) ? "download-required" : "download-unverified",
                       QObject::tr("Native library files were not bundled in the archive."));
            } else if (repairedDownloads.contains(name)) {
                record(result.entries, name, "download-unverified",
                       QObject::tr("The archive did not bundle this library; its repaired URL will be used without checksum validation."));
            } else {
                record(result.entries, name, hasDownloadSource(libraryObject) ? "download-required" : "download-unverified",
                       QObject::tr("The library was not bundled in the archive."));
            }
            continue;
        }
        if (libraryObject.contains("natives")) {
            record(result.entries, libraryObject.value("name").toString(), "download-required",
                   QObject::tr("Bundled native classifiers cannot be represented by a single local artifact and will be downloaded."));
            continue;
        }
        const auto prefix =
            QCryptographicHash::hash(libraryObject.value("name").toString().toUtf8(), QCryptographicHash::Sha256).toHex().left(12);
        const auto targetName = QString("pcl-%1-%2").arg(QString::fromLatin1(prefix), QFileInfo(source).fileName());
        if (!copyFile(source, FS::PathCombine(localLibraries, targetName), result.error))
            return result;
        library->setFilename(targetName);
        library->setHint("local");
        record(result.entries, libraryObject.value("name").toString(), "mapped-local", targetName);
    }

    if (!copyGameFiles(sourceRoot, version, instance.gameRoot(), result.error))
        return result;
    if (QFileInfo::exists(FS::PathCombine(sourceRoot, "PCL.ini"))) {
        record(result.entries, "PCL.ini", "ignored-launcher-global",
               QObject::tr("PCL.ini contains launcher-global state and was not copied into the instance."));
    }

    const auto sourceAssets = FS::PathCombine(sourceRoot, "assets");
    if (QFileInfo::exists(sourceAssets)) {
        if (patch->assets.isEmpty())
            patch->assets = "legacy";
        if (!patch->mojangAssetIndex)
            patch->mojangAssetIndex = std::make_shared<MojangAssetIndexInfo>(patch->assets);
        const auto assetId = patch->mojangAssetIndex->id.isEmpty() ? patch->assets : patch->mojangAssetIndex->id;
        const auto assetBundle = inspectAssetBundle(sourceAssets, assetId);
        if (assetBundle.hasIndex && !assetBundle.complete() && assetBundle.officialLegacyIndex) {
            instance.settings()->set("UseLocalAssets", false);
            record(result.entries, "assets", "download-required",
                   QObject::tr("The pack contains the official %1 index but only %2 of %3 referenced files. Luna will use its standard asset downloader instead of treating this as a complete local asset store.")
                       .arg(assetId)
                       .arg(assetBundle.presentEntries)
                       .arg(assetBundle.totalEntries));
            record(result.entries, "assets.prism-compatibility", "mapped",
                   QObject::tr("Official Minecraft assets use the standard MMC/Prism asset store."));
        } else {
            instance.settings()->set("UseLocalAssets", true);
            const auto localAssets = FS::PathCombine(instance.instanceRoot(), "assets");
            if (!mergeDirectory(sourceAssets, localAssets, result.error))
                return result;
            record(result.entries, "assets", assetBundle.complete() ? "mapped-instance-local" : "mapped-with-warning", "assets/");
            if (assetBundle.hasIndex && !assetBundle.validIndex) {
                record(result.entries, "assets.completeness", "invalid",
                       QObject::tr("The bundled %1 asset index is not a valid Minecraft asset index, so Luna cannot verify or repair it.")
                           .arg(assetId));
            } else if (assetBundle.hasIndex && !assetBundle.complete()) {
                record(result.entries, "assets.completeness", "unsupported",
                       QObject::tr("The custom asset index references %1 files, but only %2 are bundled and no verified standard index can replace it.")
                           .arg(assetBundle.totalEntries)
                           .arg(assetBundle.presentEntries));
            } else if (!assetBundle.hasIndex) {
                record(result.entries, "assets.completeness", "unsupported",
                       QObject::tr("The bundled assets do not contain indexes/%1.json, so Luna cannot verify or repair them.")
                           .arg(assetId));
            }
            record(result.entries, "assets.prism-compatibility", "unsupported",
                   QObject::tr(
                       "Prism Launcher does not recognize Luna's UseLocalAssets setting; exported files are preserved but may not be used."));
        }
    }

    const auto originalRoot = FS::PathCombine(instance.instanceRoot(), "pcl-import", "original");
    if (!copyFile(FS::PathCombine(sourceVersionRoot, version + ".json"), FS::PathCombine(originalRoot, version + ".json"), result.error))
        return result;

    auto profile = instance.getPackProfile();
    profile->buildingFromScratch();
    const bool splitMiteComponents = fishLoader.detected() && QFileInfo::exists(sourceJar) && minecraftVersion != version;
    if (splitMiteComponents) {
        auto mite = std::make_shared<VersionFile>();
        mite->uid = MITE_COMPONENT_UID;
        mite->name = QObject::tr("MITE core");
        mite->version = version;
        mite->mainJar = patch->mainJar;
        mite->minecraftArguments = patch->minecraftArguments;
        mite->appletClass = patch->appletClass;
        requireComponent(mite, "net.minecraft", minecraftVersion);

        auto loader = std::make_shared<VersionFile>();
        loader->uid = FISH_MOD_LOADER_UID;
        loader->name = "FishModLoader";
        loader->version = fishLoader.version;
        loader->mainClass = patch->mainClass;
        loader->addnJvmArguments = patch->addnJvmArguments;
        loader->addTweakers = patch->addTweakers;
        loader->libraries = patch->libraries;
        loader->mavenFiles = patch->mavenFiles;
        loader->agents = patch->agents;
        loader->traits = patch->traits;
        loader->jarMods = patch->jarMods;
        loader->mods = patch->mods;
        loader->compatibleJavaMajors = patch->compatibleJavaMajors;
        loader->compatibleJavaName = patch->compatibleJavaName;
        requireComponent(loader, mite->uid, mite->version);

        if (!profile->setComponentVersion("net.minecraft", minecraftVersion, true)) {
            result.error = QObject::tr("Could not create the Minecraft %1 component.").arg(minecraftVersion);
            return result;
        }
        if (!installLocalComponent(profile.get(), mite, true, result.error) ||
            !installLocalComponent(profile.get(), loader, false, result.error)) {
            return result;
        }
        record(result.entries, "components", "mapped",
               QObject::tr("Created separate Minecraft %1, MITE core %2, and FishModLoader %3 components.")
                   .arg(minecraftVersion, version, fishLoader.version));
    } else if (!installLocalComponent(profile.get(), patch, true, result.error)) {
        return result;
    }
    profile->saveNow();

    QJsonObject report{ { "formatVersion", 1 },
                        { "source", QString("versions/%1/%1.json").arg(version) },
                        { "version", version },
                        { "minecraftVersion", minecraftVersion },
                        { "entries", result.entries } };
    if (!writeJson(FS::PathCombine(instance.instanceRoot(), "lunaui", "migration", "pcl-plain-report.json"), QJsonDocument(report),
                   result.error))
        return result;

    result.succeeded = true;
    return result;
}

}  // namespace PCL
