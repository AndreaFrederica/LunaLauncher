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
#include <algorithm>

#include "FileSystem.h"
#include "VanillaCompatibleProvider.h"
#include "minecraft/GradleSpecifier.h"

namespace {
const QSet<QString> gameDirectories{ "mods", "config", "saves", "resourcepacks", "shaderpacks", "screenshots", "logs", "crash-reports", "datapacks" };
const QSet<QString> launcherFiles{ "assets", "libraries", "versions", "launcher_profiles.json", "launcher_accounts.json", "pcl", "pcl.ini", "hmclversion.cfg" };
const QSet<QString> standardLoaderIds{ "net.fabricmc.fabric-loader", "net.minecraftforge", "net.neoforged", "org.quiltmc.quilt-loader" };

QJsonObject mergeVersionJson(const QJsonObject& base, const QJsonObject& overlay)
{
    auto result = base;
    for (auto it = overlay.begin(); it != overlay.end(); ++it) {
        const auto key = it.key();
        if (key == QStringLiteral("uid") || key == QStringLiteral("version"))
            continue;
        if (it.value().isArray() && result.value(key).isArray() && (key == QStringLiteral("libraries") || key == QStringLiteral("arguments"))) {
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

QJsonObject normalizePatchStack(QJsonObject source, QStringList& warnings)
{
    const auto patches = source.value(QStringLiteral("patches")).toArray();
    if (patches.isEmpty())
        return source;

    QList<QPair<int, QJsonObject>> ordered;
    for (const auto& value : patches) {
        if (!value.isObject())
            continue;
        const auto object = value.toObject();
        ordered.append({ object.value(QStringLiteral("priority")).toInt(), object });
    }
    std::stable_sort(ordered.begin(), ordered.end(), [](const auto& left, const auto& right) { return left.first < right.first; });
    source.remove(QStringLiteral("patches"));
    for (const auto& entry : ordered)
        source = mergeVersionJson(source, entry.second);
    warnings.append(QObject::tr("The HMCL/PCL patch stack was flattened into the imported MMC component; the original profile is preserved under luna-imported."));
    return source;
}

QString findLocalLibrary(const QString& librariesRoot, const QJsonObject& library)
{
    auto path = library.value(QStringLiteral("downloads")).toObject().value(QStringLiteral("artifact")).toObject().value(QStringLiteral("path")).toString();
    if (path.isEmpty()) {
        const GradleSpecifier spec(library.value(QStringLiteral("name")).toString());
        if (spec.valid())
            path = spec.toPath();
    }
    if (path.isEmpty())
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
            if (library.contains(QStringLiteral("natives")) && !name.isEmpty())
                warnings.append(QObject::tr("Native library %1 was left as a normal dependency; its platform classifier cannot be represented by one local file.").arg(name));
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

QStringList modernArguments(const QJsonObject& version, const QString& key)
{
    QStringList result;
    const auto array = version.value(QStringLiteral("arguments")).toObject().value(key).toArray();
    for (const auto& item : array) {
        if (item.isString()) {
            result.append(item.toString());
            continue;
        }
        const auto object = item.toObject();
        const auto value = object.value(QStringLiteral("value"));
        if (value.isString()) result.append(value.toString());
        else if (value.isArray()) for (const auto& nested : value.toArray()) if (nested.isString()) result.append(nested.toString());
    }
    return result;
}

QJsonObject prismPatch(QJsonObject source, const QString& uid, const QString& componentVersion, bool loader)
{
    source.insert(QStringLiteral("uid"), uid);
    source.insert(QStringLiteral("version"), componentVersion);
    source.insert(QStringLiteral("formatVersion"), 1);
    if (source.contains(QStringLiteral("arguments"))) {
        const auto game = modernArguments(source, QStringLiteral("game"));
        const auto jvm = modernArguments(source, QStringLiteral("jvm"));
        source.remove(QStringLiteral("arguments"));
        if (!game.isEmpty()) source.insert(QStringLiteral("minecraftArguments"), game.join(QLatin1Char(' ')));
        if (!jvm.isEmpty()) {
            QJsonArray values;
            for (const auto& value : jvm) values.append(value);
            source.insert(QStringLiteral("+jvmArgs"), values);
        }
    }
    if (loader) {
        for (const auto& key : { QStringLiteral("id"), QStringLiteral("jar"), QStringLiteral("assetIndex"), QStringLiteral("assets"), QStringLiteral("downloads"), QStringLiteral("releaseTime"), QStringLiteral("time"), QStringLiteral("minimumLauncherVersion") })
            source.remove(key);
    }
    return source;
}

void addLocalClientJar(const QString& source, const QString& target, const QString& minecraftVersion, QStringList& copied, QString& error)
{
    if (source.isEmpty() || !QFileInfo(source).isFile()) return;
    const auto filename = QStringLiteral("minecraft-%1-client.jar").arg(minecraftVersion);
    const auto destination = QDir(target).filePath(QStringLiteral("libraries/%1").arg(filename));
    if (!QDir().mkpath(QFileInfo(destination).absolutePath()) || !QFile::copy(source, destination)) {
        QFile::remove(destination);
        if (!QFile::copy(source, destination)) {
            error = QObject::tr("Could not copy the client JAR %1.").arg(source);
            return;
        }
    }
    copied.append(QDir(target).relativeFilePath(destination));
}

QJsonObject localMainJar(const QString& minecraftVersion)
{
    return QJsonObject{
        { "name", QStringLiteral("com.mojang:minecraft:%1:client").arg(minecraftVersion) },
        { "MMC-hint", QStringLiteral("local") },
        { "MMC-filename", QStringLiteral("minecraft-%1-client.jar").arg(minecraftVersion) },
    };
}

bool safeRelative(const QString& relative) { return !relative.isEmpty() && !QDir::isAbsolutePath(relative) && relative != ".." && !relative.startsWith("../"); }

bool copyTree(const QString& source, const QString& target, const QSet<QString>& excluded, QStringList& copied, QString& error)
{
    if (!QFileInfo(source).exists()) return true;
    if (!QDir().mkpath(target)) { error = QObject::tr("Could not create %1.").arg(target); return false; }
    QDirIterator iterator(source, QDir::NoDotAndDotDot | QDir::AllEntries, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const auto sourcePath = iterator.next();
        const auto relative = QDir(source).relativeFilePath(sourcePath);
        if (!safeRelative(relative)) { error = QObject::tr("Unsafe path in source: %1").arg(relative); return false; }
        const auto first = relative.section('/', 0, 0).toLower();
        if (excluded.contains(first)) continue;
        const auto destination = QDir(target).filePath(relative);
        const auto info = QFileInfo(sourcePath);
        if (info.isDir()) {
            if (!QDir().mkpath(destination)) { error = QObject::tr("Could not create %1.").arg(destination); return false; }
        } else if (info.isFile()) {
            if (!QDir().mkpath(QFileInfo(destination).absolutePath()) || !QFile::copy(sourcePath, destination)) {
                QFile::remove(destination);
                if (!QFile::copy(sourcePath, destination)) { error = QObject::tr("Could not copy %1.").arg(sourcePath); return false; }
            }
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

QJsonObject patchFromProfile(const QJsonObject& profile, const QString& id)
{
    for (const auto& value : profile.value(QStringLiteral("patches")).toArray()) {
        if (!value.isObject())
            continue;
        const auto patch = value.toObject();
        if (patch.value(QStringLiteral("id")).toString().trimmed().compare(id, Qt::CaseInsensitive) == 0)
            return patch;
    }
    return {};
}

}

QJsonObject VanillaConversionReport::toJson() const
{
    QJsonArray blockersJson; for (const auto& value : blockers) blockersJson.append(value);
    QJsonArray warningsJson; for (const auto& value : warnings) warningsJson.append(value);
    QJsonArray copiedJson; for (const auto& value : copied) copiedJson.append(value);
    return { { "succeeded", succeeded }, { "blockers", blockersJson }, { "warnings", warningsJson }, { "copied", copiedJson }, { "error", error }, { "destination", destination } };
}

VanillaConversionReport VanillaConverter::importToPrism(const QString& sourceRoot, const QString& prismTarget, const VanillaImportOptions& options)
{
    VanillaConversionReport report;
    const auto layout = detectVanillaInstanceLayout(sourceRoot);
    if (!layout.isValid()) { report.error = QObject::tr("No vanilla version JSON was found in %1.").arg(sourceRoot); return report; }
    if (QFileInfo::exists(prismTarget)) { report.error = QObject::tr("The Prism destination already exists: %1").arg(prismTarget); return report; }
    const auto staging = prismTarget + QStringLiteral(".tmp-convert-") + QString::number(QRandomGenerator::global()->generate());
    QDir().mkpath(staging);
    QString error;
    const auto targetGameRoot = QDir(staging).filePath(QStringLiteral("minecraft"));
    if (options.copyGameContent && !copyTree(layout.gameRoot, targetGameRoot, launcherFiles, report.copied, error)) { report.error = error; FS::deletePath(staging); return report; }
    // Keep the source's resolved libraries beside the Prism instance. This is
    // needed for Forge installer profiles whose libraries are only available
    // in the external launcher cache and have no public download URL.
    if (options.copyGameContent && !copyTree(layout.librariesRoot, QDir(staging).filePath(QStringLiteral("libraries")), {}, report.copied, error)) {
        report.error = error;
        FS::deletePath(staging);
        return report;
    }
    // In an isolated HMCL/PCL layout `sourceRoot` already points at
    // `<minecraft-root>/versions/<version>`.  The version loader expects the
    // outer minecraft root, otherwise it appends `versions/<version>` twice.
    auto versionLookupRoot = layout.sourceRoot;
    if (layout.mode == VanillaGameLayout::IsolatedVersionRoot
        && QFileInfo(layout.sourceRoot).dir().dirName() == QStringLiteral("versions"))
        versionLookupRoot = QDir::cleanPath(QDir(layout.sourceRoot).filePath(QStringLiteral("../..")));
    auto resolvedVersion = loadVanillaVersionJson(versionLookupRoot, layout.versionId, &error);
    if (resolvedVersion.isEmpty()) { report.error = error.isEmpty() ? QObject::tr("Could not resolve the vanilla version JSON.") : error; FS::deletePath(staging); return report; }
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
            if (!copyTree(markerPath, QDir(staging).filePath(QStringLiteral("luna-imported/%1").arg(marker)), {}, report.copied, error)) {
                report.error = error;
                FS::deletePath(staging);
                return report;
            }
            report.warnings.append(QObject::tr("The %1 launcher metadata was preserved under luna-imported and is not activated as Prism configuration.").arg(marker));
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
            report.warnings.append(QObject::tr("The %1 launcher metadata was preserved under luna-imported and is not activated as Prism configuration.").arg(marker));
        }
    }
    const auto rawGamePatch = patchFromProfile(rawProfile, QStringLiteral("game"));
    const auto loaderType = VanillaCompatibleProvider::detectedLoaderType(resolvedVersion);
    auto loaderUid = VanillaCompatibleProvider::loaderUidForType(loaderType);
    auto minecraftVersion = VanillaCompatibleProvider::detectedMinecraftVersion(resolvedVersion, layout.versionId);
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
    const auto explicitParentId = rawProfile.value(QStringLiteral("inheritsFrom")).toString().trimmed();
    const auto parentId = explicitParentId.isEmpty() ? hmclPatchVersion : explicitParentId;
    QJsonObject baseVersion = resolvedVersion;
    QJsonObject loaderProfile;
    bool splitStandardLoader = !loaderUid.isEmpty();
    const auto hasCustomRuntime = rawProfile.contains(QStringLiteral("libraries")) || rawProfile.contains(QStringLiteral("mainClass"))
        || rawProfile.contains(QStringLiteral("arguments")) || rawProfile.contains(QStringLiteral("minecraftArguments"))
        || rawProfile.contains(QStringLiteral("jar"));
    if (loaderUid.isEmpty() && !parentId.isEmpty() && !hasCustomRuntime) {
        baseVersion = loadVanillaVersionJson(versionLookupRoot, parentId, &error);
        if (baseVersion.isEmpty()) { report.error = error; FS::deletePath(staging); return report; }
    } else if (loaderUid.isEmpty() && hasCustomRuntime) {
        report.warnings.append(QObject::tr("The profile contains a non-standard runtime. It will be imported as one local MMC component, and its original JSON is preserved under luna-imported."));
    }
    if (splitStandardLoader) {
        const auto declaredJar = rawProfile.value(QStringLiteral("jar")).toString().trimmed();
        // HMCL/PCL profiles commonly declare the base Minecraft version in a
        // `patches` entry (`id: game`) instead of keeping `inheritsFrom` on
        // the top-level profile. `parentId` already contains that normalized
        // value, so requiring `explicitParentId` incorrectly downgraded a
        // perfectly standard Forge profile to a custom component.
        const bool canSplit = !loaderVersion.isEmpty() && !minecraftVersion.isEmpty()
            && (declaredJar.isEmpty() || declaredJar == parentId || declaredJar == minecraftVersion);
        if (!canSplit) {
            report.warnings.append(QObject::tr("The %1 profile cannot be split into a standard MMC loader component because its base version, loader version, or client JAR metadata is incomplete. It will be imported as one local component instead.").arg(loaderType));
            splitStandardLoader = false;
            loaderUid.clear();
            baseVersion = resolvedVersion;
        } else {
            if (!parentId.isEmpty()) {
                baseVersion = loadVanillaVersionJson(versionLookupRoot, parentId, &error);
                if (baseVersion.isEmpty()) {
                    // A flattened HMCL/PCL profile can use the Minecraft ID
                    // as its own profile ID. In that case the component
                    // metadata is authoritative and no local base patch is
                    // required.
                    baseVersion = rawGamePatch;
                    error.clear();
                }
            } else {
                baseVersion = rawGamePatch;
            }
            loaderProfile = loaderPatchFromProfile(rawProfile, loaderType, loaderUid);
            loaderProfile.remove(QStringLiteral("inheritsFrom"));
            loaderProfile.remove(QStringLiteral("id"));
        }
    }
    const bool standardMetadataOnly = splitStandardLoader && VanillaCompatibleProvider::isStandardLoaderProfile(rawProfile, loaderType, loaderProfile);
    if (standardMetadataOnly)
        report.warnings.append(QObject::tr("The standard %1 loader profile was imported as metadata-backed MMC components.").arg(loaderType));
    const auto componentVersion = splitStandardLoader ? minecraftVersion : layout.versionId;
    if (!standardMetadataOnly) {
        baseVersion.remove(QStringLiteral("inheritsFrom"));
        baseVersion.insert(QStringLiteral("id"), componentVersion);
        baseVersion.insert(QStringLiteral("uid"), QStringLiteral("net.minecraft"));
        baseVersion.insert(QStringLiteral("version"), componentVersion);
    }
    if (!standardMetadataOnly && QFileInfo(layout.clientJar).isFile() && !hasClientDownload(baseVersion)) {
        baseVersion.insert(QStringLiteral("mainJar"), localMainJar(componentVersion));
        addLocalClientJar(layout.clientJar, staging, componentVersion, report.copied, error);
        if (!error.isEmpty()) { report.error = error; FS::deletePath(staging); return report; }
        report.warnings.append(QObject::tr("The source client JAR was copied into the Prism instance library cache."));
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
        basePatch = prismPatch(baseVersion, QStringLiteral("net.minecraft"), minecraftVersion, false);
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
            const auto loaderPatch = prismPatch(loaderProfile, loaderUid, loaderVersion, true);
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
    if (!loaderUid.isEmpty() && !standardMetadataOnly)
        report.warnings.append(QObject::tr("Loader arguments and libraries were separated from the base Minecraft component."));
    else if (!standardMetadataOnly)
        report.warnings.append(QObject::tr("The resolved vanilla version JSON was stored as a Prism net.minecraft component patch."));
    if (!QDir().rename(staging, prismTarget)) { report.error = QObject::tr("Could not commit the Prism conversion."); FS::deletePath(staging); return report; }
    report.warnings.append(QObject::tr("The source vanilla directory was left unchanged."));
    report.destination = prismTarget;
    report.succeeded = true;
    Q_UNUSED(options.keepSource);
    return report;
}

VanillaConversionReport VanillaConverter::exportToVanilla(const QString& prismRoot, const QString& vanillaTarget, const VanillaExportOptions& options)
{
    VanillaConversionReport report;
    const auto compatibility = checkVanillaExportCompatibility(prismRoot);
    report.blockers = compatibility.blockers; report.warnings = compatibility.warnings;
    if (!compatibility.compatible) { report.error = QObject::tr("The Prism instance contains data that cannot be represented in vanilla format."); return report; }
    const auto versionId = options.versionId.trimmed();
    if (versionId.isEmpty()) { report.error = QObject::tr("A target Minecraft version ID is required."); return report; }
    if (QFileInfo::exists(vanillaTarget)) { report.error = QObject::tr("The vanilla destination already exists: %1").arg(vanillaTarget); return report; }
    const auto staging = vanillaTarget + QStringLiteral(".tmp-convert-") + QString::number(QRandomGenerator::global()->generate());
    QDir().mkpath(staging);
    QString error;
    const auto gameRoot = QDir(prismRoot).filePath(QStringLiteral(".minecraft"));
    const auto sourceGame = QFileInfo(gameRoot).isDir() ? gameRoot : QDir(prismRoot).filePath(QStringLiteral("minecraft"));
    const auto sourceGameRoot = QFileInfo(sourceGame).isDir() ? sourceGame : prismRoot;
    if (options.copyGameContent && !copyTree(sourceGameRoot, staging,
                                              { "instance.cfg", "mmc-pack.json", "patches", "bin", "libraries", "versions", "luna-imported", "luna.settings.json" },
                                              report.copied, error)) {
        report.error = error;
        FS::deletePath(staging);
        return report;
    }
    QJsonObject version = options.versionJson;
    if (version.isEmpty()) {
        QFile patchFile(QDir(prismRoot).filePath(QStringLiteral("patches/net.minecraft.json")));
        if (patchFile.open(QIODevice::ReadOnly)) {
            QJsonParseError parseError;
            const auto document = QJsonDocument::fromJson(patchFile.readAll(), &parseError);
            if (parseError.error == QJsonParseError::NoError && document.isObject())
                version = document.object();
        }
    }
    if (version.isEmpty()) version = { { "id", versionId }, { "type", "release" } };
    version.insert("id", versionId);
    version.remove(QStringLiteral("uid"));
    version.remove(QStringLiteral("version"));
    // Standard loader component patches can be represented by the vanilla
    // version JSON. Merge their libraries, arguments and main class so the
    // exported directory remains launchable by PCL/HMCL/official launchers.
    QFile packFile(QDir(prismRoot).filePath(QStringLiteral("mmc-pack.json")));
    if (packFile.open(QIODevice::ReadOnly)) {
        QJsonParseError parseError;
        const auto pack = QJsonDocument::fromJson(packFile.readAll(), &parseError).object();
        for (const auto& value : pack.value(QStringLiteral("components")).toArray()) {
            const auto uid = value.toObject().value(QStringLiteral("uid")).toString();
            if (!standardLoaderIds.contains(uid)) continue;
            QFile loaderFile(QDir(prismRoot).filePath(QStringLiteral("patches/%1.json").arg(uid)));
            if (!loaderFile.open(QIODevice::ReadOnly)) continue;
            const auto loaderDoc = QJsonDocument::fromJson(loaderFile.readAll(), &parseError);
            if (loaderDoc.isObject()) {
                version = mergeVersionJson(version, loaderDoc.object());
                report.warnings.append(QObject::tr("Installed standard loader component %1 into the exported version JSON.").arg(uid));
            }
        }
    }
    for (auto it = options.extensionFields.begin(); it != options.extensionFields.end(); ++it)
        version.insert(it.key(), it.value());
    const auto versionRoot = QDir(staging).filePath(QStringLiteral("versions/%1").arg(versionId));
    if (!writeJson(QDir(versionRoot).filePath(versionId + QStringLiteral(".json")), version, error)) { report.error = error; FS::deletePath(staging); return report; }
    if (!options.name.trimmed().isEmpty() || !options.group.trimmed().isEmpty()) {
        const auto settings = QJsonObject{
            { "formatVersion", 1 },
            { "settings", QJsonObject{ { "name", options.name }, { "group", options.group } } },
        };
        if (!writeJson(QDir(staging).filePath(QStringLiteral("luna.settings.json")), settings, error)) { report.error = error; FS::deletePath(staging); return report; }
    }
    if (!QDir().rename(staging, vanillaTarget)) { report.error = QObject::tr("Could not commit the vanilla conversion."); FS::deletePath(staging); return report; }
    report.destination = vanillaTarget;
    report.succeeded = true;
    Q_UNUSED(options.isolated);
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
    const auto targetDir = layout.mode == VanillaGameLayout::IsolatedVersionRoot
        && QFileInfo(layout.sourceRoot).dir().dirName() == QStringLiteral("versions")
        ? layout.sourceRoot
        : QDir(vanillaRoot).filePath(QStringLiteral("versions/%1").arg(targetId));
    QString error;
    if (!writeJson(QDir(targetDir).filePath(targetId + QStringLiteral(".json")), version, error)) {
        report.error = error;
        return report;
    }
    report.copied.append(QDir(targetDir).relativeFilePath(targetDir + QStringLiteral("/%1.json").arg(targetId)));
    report.warnings.append(QObject::tr("The loader profile inherits the base Minecraft version. Libraries from its standard download entries will be repaired by the launcher that starts it."));
    report.succeeded = true;
    return report;
}

