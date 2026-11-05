// SPDX-License-Identifier: GPL-3.0-only
#include "VanillaCompatibleProvider.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>
#include <QSaveFile>
#include <QSet>
#include <algorithm>
#include <functional>

namespace {
QString clean(const QString& path) { return QDir::cleanPath(QFileInfo(path).absoluteFilePath()); }

QJsonObject mergeForeignVersion(const QJsonObject& base, const QJsonObject& overlay)
{
    auto result = base;
    for (auto it = overlay.begin(); it != overlay.end(); ++it) {
        const auto key = it.key();
        if (key == QStringLiteral("id") || key == QStringLiteral("uid") || key == QStringLiteral("version")
            || key == QStringLiteral("priority"))
            continue;
        if (it.value().isObject() && result.value(key).isObject()) {
            result.insert(key, mergeForeignVersion(result.value(key).toObject(), it.value().toObject()));
        } else if (it.value().isArray() && result.value(key).isArray()
                   && (key == QStringLiteral("libraries") || key == QStringLiteral("arguments"))) {
            auto values = result.value(key).toArray();
            for (const auto& value : it.value().toArray())
                values.append(value);
            result.insert(key, values);
        } else {
            result.insert(key, it.value());
        }
    }
    return result;
}

QJsonObject resolveForeignPatches(const QJsonObject& source)
{
    const auto patches = source.value(QStringLiteral("patches")).toArray();
    if (patches.isEmpty())
        return source;

    QList<QPair<int, QJsonObject>> ordered;
    for (const auto& value : patches) {
        if (!value.isObject())
            continue;
        const auto patch = value.toObject();
        ordered.append({ patch.value(QStringLiteral("priority")).toInt(), patch });
    }
    std::stable_sort(ordered.begin(), ordered.end(), [](const auto& left, const auto& right) {
        return left.first < right.first;
    });

    QJsonObject resolved;
    for (const auto& item : ordered)
        resolved = mergeForeignVersion(resolved, item.second);
    // Keep the profile identity from the outer document. HMCL/PCL use this
    // value when they display and launch an isolated profile.
    for (const auto& key : { QStringLiteral("id"), QStringLiteral("jar"), QStringLiteral("root") }) {
        if (source.contains(key))
            resolved.insert(key, source.value(key));
    }
    return resolved;
}

bool readVersion(const QString& path, QJsonObject& result)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return false;
    result = document.object();
    return true;
}

QString lunaSettingsPath(const QString& instanceRoot)
{
    return QDir(instanceRoot).filePath(QStringLiteral("luna.settings.json"));
}

bool writeJsonFile(const QString& path, const QJsonDocument& document, QString* error, const QString& description)
{
    const auto data = document.toJson(QJsonDocument::Indented);
    QString atomicError;
    {
        QSaveFile file(path);
        // Keep the atomic replacement as the first choice. The direct fallback
        // below is needed on Windows when another launcher or an antivirus
        // scanner temporarily prevents the rename of the temporary file.
        file.setDirectWriteFallback(true);
        if (file.open(QIODevice::WriteOnly) && file.write(data) == data.size() && file.commit())
            return true;
        atomicError = file.errorString();
    }

    // QSaveFile has closed and cleaned up its temporary file at this point.
    // Opening the destination directly preserves its existing ACL and works
    // for files that can be truncated but cannot be atomically replaced.
    QFile direct(path);
    QString directError;
    if (direct.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        const auto written = direct.write(data);
        const bool flushed = written == data.size() && direct.flush();
        direct.close();
        if (flushed)
            return true;
        directError = direct.errorString();
    } else {
        directError = direct.errorString();
    }

    if (error) {
        *error = QObject::tr("Could not write %1 at %2. Atomic write: %3. Direct write: %4")
                     .arg(description, QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath()), atomicError, directError);
    }
    return false;
}

QString argumentValue(const QJsonObject& version, const QStringList& names)
{
    const auto arguments = version.value(QStringLiteral("arguments")).toObject();
    const auto game = arguments.value(QStringLiteral("game")).toArray();
    QString previous;
    for (const auto& value : game) {
        if (value.isString()) {
            const auto text = value.toString();
            if (!previous.isEmpty() && names.contains(previous)) return text;
            previous = text;
            continue;
        }
        const auto object = value.toObject();
        const auto nested = object.value(QStringLiteral("value"));
        if (nested.isString()) {
            const auto text = nested.toString();
            if (!previous.isEmpty() && names.contains(previous)) return text;
            previous = text;
        } else if (nested.isArray()) {
            for (const auto& nestedValue : nested.toArray()) {
                if (!nestedValue.isString()) continue;
                const auto text = nestedValue.toString();
                if (!previous.isEmpty() && names.contains(previous)) return text;
                previous = text;
            }
        }
    }
    return {};
}

QString libraryVersion(const QJsonObject& version, const QStringList& prefixes)
{
    for (const auto& value : version.value(QStringLiteral("libraries")).toArray()) {
        const auto name = value.toObject().value(QStringLiteral("name")).toString();
        for (const auto& prefix : prefixes) {
            if (name.startsWith(prefix)) {
                const auto parts = name.split(QLatin1Char(':'));
                if (parts.size() >= 3)
                    return parts.at(2).split(QLatin1Char('@')).first();
            }
        }
    }
    return {};
}

bool hasMarkerInPathOrAncestors(const QString& root, const QStringList& markerNames, int maxDepth = 4)
{
    auto current = QDir::cleanPath(QFileInfo(root).absoluteFilePath());
    for (int depth = 0; depth <= maxDepth && !current.isEmpty(); ++depth) {
        const QDir directory(current);
        for (const auto& marker : markerNames) {
            if (QFileInfo::exists(directory.filePath(marker)))
                return true;
        }

        const auto parent = QDir::cleanPath(directory.filePath(QStringLiteral("..")));
        if (parent == current)
            break;
        current = parent;
    }
    return false;
}

QList<QString> rootsToInspect(const QString& root)
{
    QList<QString> result;
    const auto normalized = clean(root);
    std::function<void(const QString&, int)> visit = [&result, &visit](const QString& current, int depth) {
        const auto versions = QDir(current).filePath(QStringLiteral("versions"));
        if (QFileInfo(versions).isDir()) {
            bool hasIsolatedContent = false;
            for (const auto& entry : QDir(versions).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable, QDir::Name)) {
                const auto versionId = entry.fileName();
                if (!QFileInfo::exists(QDir(entry.filePath()).filePath(versionId + QStringLiteral(".json")))) continue;
                const auto isolatedRoot = clean(entry.filePath());
                const auto hasContent = QFileInfo(QDir(isolatedRoot).filePath(QStringLiteral("game"))).isDir()
                    || QFileInfo(QDir(isolatedRoot).filePath(QStringLiteral("mods"))).isDir()
                    || QFileInfo(QDir(isolatedRoot).filePath(QStringLiteral("config"))).isDir()
                    || QFileInfo(QDir(isolatedRoot).filePath(QStringLiteral("saves"))).isDir();
                if (hasContent) {
                    hasIsolatedContent = true;
                    result.append(isolatedRoot);
                }
            }
            // Keep the outer root for the conventional shared layout. If all
            // content is placed under version roots, the version roots above
            // are the actual instances and the outer root is only a cache.
            if (!hasIsolatedContent || QFileInfo(QDir(current).filePath(QStringLiteral("mods"))).isDir()
                || QFileInfo(QDir(current).filePath(QStringLiteral("config"))).isDir()
                || QFileInfo(QDir(current).filePath(QStringLiteral("saves"))).isDir())
                result.append(clean(current));
        }
        if (depth >= 3)
            return;
        const auto children = QDir(current).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable | QDir::Hidden, QDir::Name);
        for (const auto& child : children) {
            const auto name = child.fileName().toLower();
            if (name == QStringLiteral("assets") || name == QStringLiteral("libraries") || name == QStringLiteral("versions") || name == QStringLiteral("logs"))
                continue;
            visit(clean(child.filePath()), depth + 1);
        }
    };
    if (!normalized.isEmpty())
        visit(normalized, 0);
    return result;
}

}  // namespace

QString VanillaCompatibleProvider::detectedLoaderType(const QJsonObject& version)
{
    const auto resolved = resolveForeignPatches(version);
    const auto gameArgs = resolved.value(QStringLiteral("arguments")).toObject().value(QStringLiteral("game")).toArray();
    QStringList args;
    for (const auto& value : gameArgs) {
        if (value.isString()) args.append(value.toString());
    }
    if (!libraryVersion(resolved, { QStringLiteral("net.neoforged:neoforge:"), QStringLiteral("net.neoforged.fancymodloader:") }).isEmpty()
        || args.contains(QStringLiteral("neoforgeclient"))
        || !argumentValue(resolved, { QStringLiteral("--fml.neoFormVersion"), QStringLiteral("--fml.neoforgeVersion") }).isEmpty())
        return QStringLiteral("neoforge");
    if (args.contains(QStringLiteral("forgeclient"))
        || !libraryVersion(resolved, { QStringLiteral("net.minecraftforge:fmlloader:"), QStringLiteral("net.minecraftforge:forge:") }).isEmpty()
        || !argumentValue(resolved, { QStringLiteral("--fml.forgeVersion"), QStringLiteral("--fml.mcVersion") }).isEmpty())
        return QStringLiteral("forge");
    if (!libraryVersion(resolved, { QStringLiteral("net.fabricmc:fabric-loader:") }).isEmpty())
        return QStringLiteral("fabric");
    if (!libraryVersion(resolved, { QStringLiteral("org.quiltmc:quilt-loader:") }).isEmpty())
        return QStringLiteral("quilt");
    return QStringLiteral("vanilla");
}

QString VanillaCompatibleProvider::detectedLoaderVersion(const QJsonObject& version)
{
    const auto resolved = resolveForeignPatches(version);
    const auto type = detectedLoaderType(resolved);
    const auto minecraft = detectedMinecraftVersion(resolved);
    // Forge and NeoForge Maven coordinates include the Minecraft version
    // (`1.18.2-40.3.12`), while MMC component metadata stores only the loader
    // patch version (`40.3.12`).  The loader libraries are authoritative for
    // the installed runtime; launcher arguments can be stale after a loader
    // update and must not win over them.
    const auto normalize = [&minecraft](QString value) {
        value = value.trimmed();
        const auto prefix = minecraft.trimmed() + QLatin1Char('-');
        if (!minecraft.trimmed().isEmpty() && value.startsWith(prefix))
            value.remove(0, prefix.size());
        return value;
    };
    if (type == QStringLiteral("forge")) {
        auto value = libraryVersion(resolved, { QStringLiteral("net.minecraftforge:fmlloader:"), QStringLiteral("net.minecraftforge:forge:") });
        if (value.isEmpty()) value = argumentValue(resolved, { QStringLiteral("--fml.forgeVersion") });
        return normalize(value);
    }
    if (type == QStringLiteral("neoforge")) {
        auto value = libraryVersion(resolved, { QStringLiteral("net.neoforged:neoforge:"), QStringLiteral("net.neoforged.fancymodloader:") });
        if (value.isEmpty()) value = argumentValue(resolved, { QStringLiteral("--fml.neoforgeVersion"), QStringLiteral("--fml.neoFormVersion") });
        return normalize(value);
    }
    if (type == QStringLiteral("fabric")) return libraryVersion(resolved, { QStringLiteral("net.fabricmc:fabric-loader:") });
    if (type == QStringLiteral("quilt")) return libraryVersion(resolved, { QStringLiteral("org.quiltmc:quilt-loader:") });
    return {};
}

QString VanillaCompatibleProvider::detectedMinecraftVersion(const QJsonObject& version, const QString& profileId)
{
    const auto fmlVersion = argumentValue(version, { QStringLiteral("--fml.mcVersion") });
    if (!fmlVersion.isEmpty()) return fmlVersion;
    const auto jar = version.value(QStringLiteral("jar")).toString().trimmed();
    if (!jar.isEmpty() && (jar != profileId || profileId.isEmpty())) return jar;
    const auto inherited = version.value(QStringLiteral("inheritsFrom")).toString().trimmed();
    if (!inherited.isEmpty()) return inherited;
    return version.value(QStringLiteral("id")).toString(profileId);
}

QString VanillaCompatibleProvider::loaderUidForType(const QString& loaderType)
{
    const auto type = loaderType.trimmed().toLower();
    if (type == QStringLiteral("forge")) return QStringLiteral("net.minecraftforge");
    if (type == QStringLiteral("neoforge")) return QStringLiteral("net.neoforged");
    if (type == QStringLiteral("fabric")) return QStringLiteral("net.fabricmc.fabric-loader");
    if (type == QStringLiteral("quilt")) return QStringLiteral("org.quiltmc.quilt-loader");
    if (type == QStringLiteral("net.minecraftforge")) return QStringLiteral("net.minecraftforge");
    if (type == QStringLiteral("net.neoforged")) return QStringLiteral("net.neoforged");
    if (type == QStringLiteral("net.fabricmc.fabric-loader")) return QStringLiteral("net.fabricmc.fabric-loader");
    if (type == QStringLiteral("org.quiltmc.quilt-loader")) return QStringLiteral("org.quiltmc.quilt-loader");
    return {};
}

QString VanillaCompatibleProvider::loaderTypeForUid(const QString& loaderUid)
{
    const auto uid = loaderUid.trimmed().toLower();
    if (uid == QStringLiteral("forge") || uid == QStringLiteral("net.minecraftforge")) return QStringLiteral("forge");
    if (uid == QStringLiteral("neoforge") || uid == QStringLiteral("net.neoforged")) return QStringLiteral("neoforge");
    if (uid == QStringLiteral("fabric") || uid == QStringLiteral("net.fabricmc.fabric-loader")) return QStringLiteral("fabric");
    if (uid == QStringLiteral("quilt") || uid == QStringLiteral("org.quiltmc.quilt-loader")) return QStringLiteral("quilt");
    return {};
}

QJsonObject VanillaCompatibleProvider::canonicalLoaderPatch(QJsonObject profile,
                                                            const QString& loaderType,
                                                            const QString& version)
{
    const auto patchId = loaderTypeForUid(loaderType);
    if (patchId.isEmpty() || version.trimmed().isEmpty())
        return {};

    // Convert Prism metadata into the common HMCL/PCL patch schema. Keep the
    // loader libraries, arguments, rules and main class, while dropping Prism
    // bookkeeping and fields owned by the Minecraft base component.
    const auto jvmArgs = profile.value(QStringLiteral("+jvmArgs")).toArray();
    for (const auto& key : { QStringLiteral("uid"), QStringLiteral("formatVersion"), QStringLiteral("+traits"),
                             QStringLiteral("requires"), QStringLiteral("conflicts"), QStringLiteral("volatile"),
                             QStringLiteral("name"), QStringLiteral("patches"), QStringLiteral("inheritsFrom"),
                             QStringLiteral("jar"), QStringLiteral("assetIndex"), QStringLiteral("assets"),
                             QStringLiteral("downloads"), QStringLiteral("releaseTime"), QStringLiteral("time"),
                             QStringLiteral("minimumLauncherVersion"), QStringLiteral("+jvmArgs") })
        profile.remove(key);

    // Older Forge installers put the complete vanilla command line in the
    // loader patch's legacy `minecraftArguments` field. HMCL/PCL append that
    // field to the game's structured arguments, so retaining the vanilla part
    // produces duplicate options such as `--gameDir` and makes ModLauncher
    // reject the command. Convert it to loader-only structured arguments.
    if (profile.contains(QStringLiteral("minecraftArguments"))) {
        const auto legacyArguments = QProcess::splitCommand(profile.value(QStringLiteral("minecraftArguments")).toString());
        const QSet<QString> vanillaValueOptions{
            QStringLiteral("--username"), QStringLiteral("--version"), QStringLiteral("--gameDir"),
            QStringLiteral("--assetsDir"), QStringLiteral("--assetIndex"), QStringLiteral("--uuid"),
            QStringLiteral("--accessToken"), QStringLiteral("--clientId"), QStringLiteral("--xuid"),
            QStringLiteral("--userType"), QStringLiteral("--versionType"), QStringLiteral("--width"),
            QStringLiteral("--height"), QStringLiteral("--quickPlayPath"), QStringLiteral("--quickPlaySingleplayer"),
            QStringLiteral("--quickPlayMultiplayer"), QStringLiteral("--quickPlayRealms") };
        const QSet<QString> vanillaFlags{ QStringLiteral("--demo") };
        QStringList loaderArguments;
        for (qsizetype i = 0; i < legacyArguments.size(); ++i) {
            const auto argument = legacyArguments.at(i);
            if (vanillaFlags.contains(argument))
                continue;
            if (vanillaValueOptions.contains(argument)) {
                if (i + 1 < legacyArguments.size())
                    ++i;
                continue;
            }
            loaderArguments.append(argument);
        }

        auto arguments = profile.value(QStringLiteral("arguments")).toObject();
        auto game = arguments.value(QStringLiteral("game")).toArray();
        for (const auto& argument : loaderArguments) {
            bool alreadyPresent = false;
            for (const auto& existing : game) {
                if (existing.isString() && existing.toString() == argument) {
                    alreadyPresent = true;
                    break;
                }
            }
            if (!alreadyPresent)
                game.append(argument);
        }
        if (!game.isEmpty()) {
            arguments.insert(QStringLiteral("game"), game);
            profile.insert(QStringLiteral("arguments"), arguments);
        }
        profile.remove(QStringLiteral("minecraftArguments"));
    }

    if (!jvmArgs.isEmpty()) {
        auto arguments = profile.value(QStringLiteral("arguments")).toObject();
        auto existing = arguments.value(QStringLiteral("jvm")).toArray();
        for (const auto& value : jvmArgs)
            existing.append(value);
        arguments.insert(QStringLiteral("jvm"), existing);
        profile.insert(QStringLiteral("arguments"), arguments);
    }

    // HMCL/PCL's ForgeWrapper does not use Mojang's -Dminecraft.client.jar
    // property when it resolves the vanilla input.  Their MultiMC
    // compatibility patch supplies these two launcher variables instead.  A
    // foreign profile written by Luna must carry the same contract or the
    // wrapper reports "Unable to detect the Minecraft jar" even when the jar
    // is present in the instance's versions directory.
    if (profile.value(QStringLiteral("mainClass")).toString().contains(QStringLiteral("forgewrapper.installer.Main"), Qt::CaseInsensitive)) {
        auto arguments = profile.value(QStringLiteral("arguments")).toObject();
        auto existing = arguments.value(QStringLiteral("jvm")).toArray();
        const auto appendUnique = [&existing](const QString& argument) {
            for (const auto& value : existing) {
                if (value.toString() == argument)
                    return;
            }
            existing.append(argument);
        };
        appendUnique(QStringLiteral("-Dforgewrapper.librariesDir=${library_directory}"));
        appendUnique(QStringLiteral("-Dforgewrapper.minecraft=${primary_jar}"));
        arguments.insert(QStringLiteral("jvm"), existing);
        profile.insert(QStringLiteral("arguments"), arguments);
    }
    profile.insert(QStringLiteral("id"), patchId);
    profile.insert(QStringLiteral("version"), version.trimmed());
    profile.insert(QStringLiteral("priority"), 30000);
    return profile;
}

QJsonObject VanillaCompatibleProvider::makeInheritedLoaderProfile(const QString& baseVersionId,
                                                                   const QString& loaderType,
                                                                   const QString& version,
                                                                   QJsonObject profile,
                                                                   const QString& clientJar)
{
    const auto type = loaderTypeForUid(loaderType);
    if (baseVersionId.trimmed().isEmpty() || type.isEmpty() || version.trimmed().isEmpty())
        return {};

    // Meta::Version JSON may contain Prism-only bookkeeping. Remove those
    // fields before putting the profile into a vanilla/PCL/HMCL versions
    // directory; libraries, rules, arguments and mainClass are retained.
    for (const auto& key : { QStringLiteral("uid"), QStringLiteral("version"), QStringLiteral("formatVersion"),
                             QStringLiteral("priority"), QStringLiteral("requires"), QStringLiteral("conflicts"),
                             QStringLiteral("volatile"), QStringLiteral("name"), QStringLiteral("patches") })
        profile.remove(key);
    const auto targetId = QStringLiteral("%1-%2-%3").arg(baseVersionId.trimmed(), type, version.trimmed());
    profile.insert(QStringLiteral("id"), targetId);
    profile.insert(QStringLiteral("inheritsFrom"), baseVersionId.trimmed());
    profile.insert(QStringLiteral("type"), QStringLiteral("release"));
    if (!clientJar.trimmed().isEmpty())
        profile.insert(QStringLiteral("jar"), QFileInfo(clientJar).fileName());
    return profile;
}

bool VanillaCompatibleProvider::writeLoaderManifest(const QString& versionJsonPath,
                                                    const QString& loaderType,
                                                    const QString& version,
                                                    const QJsonObject& loaderProfile,
                                                    const QJsonObject& baseProfile,
                                                    bool enabled,
                                                    QString* error)
{
    QFile input(versionJsonPath);
    if (!input.open(QIODevice::ReadOnly)) {
        if (error) *error = QObject::tr("Could not open the foreign version metadata: %1").arg(input.errorString());
        return false;
    }
    QJsonParseError parseError;
    // Close the source handle before opening QSaveFile. Windows does not
    // allow the atomic replacement while the same version JSON is still open
    // by this process, which otherwise appears as a misleading "Access
    // denied" commit failure.
    const auto sourceData = input.readAll();
    input.close();
    const auto document = QJsonDocument::fromJson(sourceData, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) *error = QObject::tr("Could not parse the foreign version metadata: %1").arg(parseError.errorString());
        return false;
    }

    const auto patchId = loaderTypeForUid(loaderType);
    const auto canonical = canonicalLoaderPatch(loaderProfile, loaderType, version);
    if (patchId.isEmpty() || canonical.isEmpty()) {
        if (error) *error = QObject::tr("This instance does not contain a supported standard mod loader.");
        return false;
    }

    const auto raw = document.object();
    const auto existingPatches = raw.value(QStringLiteral("patches")).toArray();
    QJsonObject output = raw;
    QJsonArray patches;
    bool replaced = false;
    for (const auto& value : existingPatches) {
        const auto patch = value.toObject();
        const auto id = patch.value(QStringLiteral("id")).toString();
        const bool isLoader = id.compare(patchId, Qt::CaseInsensitive) == 0
            || VanillaCompatibleProvider::detectedLoaderType(patch) == loaderTypeForUid(loaderType);
        if (isLoader) {
            if (enabled && !replaced) {
                auto replacement = patch;
                if (canonical.contains(QStringLiteral("arguments")))
                    replacement.remove(QStringLiteral("minecraftArguments"));
                if (canonical.contains(QStringLiteral("minecraftArguments")))
                    replacement.remove(QStringLiteral("arguments"));
                for (auto it = canonical.begin(); it != canonical.end(); ++it)
                    replacement.insert(it.key(), it.value());
                if (patch.contains(QStringLiteral("priority")))
                    replacement.insert(QStringLiteral("priority"), patch.value(QStringLiteral("priority")));
                patches.append(replacement);
                replaced = true;
            }
            continue;
        }
        patches.append(value);
    }

    if (existingPatches.isEmpty()) {
        auto gamePatch = baseProfile.isEmpty() ? raw : baseProfile;
        gamePatch.remove(QStringLiteral("patches"));
        gamePatch.remove(QStringLiteral("inheritsFrom"));
        gamePatch.remove(QStringLiteral("uid"));
        gamePatch.remove(QStringLiteral("formatVersion"));
        gamePatch.remove(QStringLiteral("priority"));
        gamePatch.insert(QStringLiteral("id"), QStringLiteral("game"));
        gamePatch.insert(QStringLiteral("version"), detectedMinecraftVersion(gamePatch, raw.value(QStringLiteral("id")).toString()));
        gamePatch.insert(QStringLiteral("priority"), 0);
        patches.prepend(gamePatch);
        if (enabled)
            patches.append(canonical);
        output = QJsonObject{ { "id", raw.value(QStringLiteral("id")).toString() }, { "root", true }, { "patches", patches } };
        for (const auto& key : { QStringLiteral("type"), QStringLiteral("hidden") })
            if (raw.contains(key)) output.insert(key, raw.value(key));
    }
    if (enabled && !replaced && !existingPatches.isEmpty())
        patches.append(canonical);
    output.insert(QStringLiteral("patches"), patches);
    // PCL uses the absence of `time` to identify HMCL's patch format.
    output.remove(QStringLiteral("time"));

    return writeJsonFile(versionJsonPath,
                         QJsonDocument(output),
                         error,
                         QObject::tr("foreign version metadata"));
}

bool VanillaCompatibleProvider::isStandardLoaderProfile(const QJsonObject& profile,
                                                        const QString& loaderType,
                                                        const QJsonObject& loaderPatch)
{
    if (loaderPatch.isEmpty())
        return false;

    const auto patches = profile.value(QStringLiteral("patches")).toArray();
    if (!patches.isEmpty()) {
        bool hasGame = false;
        bool hasLoader = false;
        for (const auto& value : patches) {
            const auto id = value.toObject().value(QStringLiteral("id")).toString().trimmed().toLower();
            if (id == QStringLiteral("game"))
                hasGame = true;
            else if (id == loaderType || id.contains(loaderType))
                hasLoader = true;
            else if (!id.isEmpty())
                return false;
        }
        // HMCL's normal Forge/Fabric/NeoForge install has exactly the game
        // patch plus one loader patch. The merged profile is a launcher view,
        // not a Prism component override.
        return hasGame && hasLoader;
    }

    // PCL can flatten the same standard profile into one JSON document.
    const auto mainClass = profile.value(QStringLiteral("mainClass")).toString();
    return mainClass.contains(QStringLiteral("bootstraplauncher"), Qt::CaseInsensitive)
        || mainClass.contains(QStringLiteral("fabric"), Qt::CaseInsensitive)
        || mainClass.contains(QStringLiteral("quilt"), Qt::CaseInsensitive);
}

QString VanillaCompatibleProvider::stableId(const QString& storeId, const QString& root, const QString& versionId)
{
    const auto key = storeId + QLatin1Char('\n') + clean(root) + QLatin1Char('\n') + versionId;
    return QStringLiteral("vanilla-%1").arg(QString::fromLatin1(QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha256).toHex().left(20)));
}

QString VanillaCompatibleProvider::dialectName(const QJsonObject& version, const QString& root)
{
    const auto provenance = detectProvenance(version, root);
    return provenance.primaryProvider.isEmpty() ? QStringLiteral("mojang") : provenance.primaryProvider;
}

ExternalInstanceProvenance VanillaCompatibleProvider::detectProvenance(const QJsonObject& version,
                                                                        const QString& root,
                                                                        const VanillaInstanceLayout& layout)
{
    return ExternalLauncherRecognizerRegistry::instance().detect({ root, layout, version });
}

QList<VanillaInstanceDescriptor> VanillaCompatibleProvider::scan(const QString& storeId, const QString& storeRoot)
{
    QList<VanillaInstanceDescriptor> result;
    for (const auto& root : rootsToInspect(storeRoot)) {
        auto layout = detectVanillaInstanceLayout(root);
        if (!layout.isValid())
            continue;
        // For an HMCL/PCL isolated profile, sourceRoot is
        // `.minecraft/versions/<profile>` while the profile JSON is resolved
        // relative to the enclosing `.minecraft` root.
        const auto versionSourceRoot = layout.versionsRoot.isEmpty() || layout.versionsRoot == layout.sourceRoot
            ? layout.sourceRoot
            : QDir(layout.versionsRoot).filePath(QStringLiteral(".."));
        QJsonObject version = resolveForeignPatches(loadVanillaVersionJson(versionSourceRoot, layout.versionId));
        VanillaInstanceDescriptor descriptor;
        descriptor.storeId = storeId;
        descriptor.root = root;
        descriptor.versionId = layout.versionId;
        descriptor.profileVersionId = layout.versionId;
        descriptor.layout = layout;
        descriptor.version = version;
        descriptor.stableId = stableId(storeId, root, layout.versionId);
        descriptor.name = QFileInfo(root).fileName();
        if (descriptor.name.isEmpty())
            descriptor.name = layout.versionId;
        descriptor.provenance = detectProvenance(version, root, layout);
        descriptor.dialect = descriptor.provenance.mixed ? QStringLiteral("mixed") : descriptor.provenance.primaryProvider;
        descriptor.loaderType = detectedLoaderType(version);
        descriptor.loaderVersion = detectedLoaderVersion(version);
        descriptor.minecraftVersion = detectedMinecraftVersion(version, layout.versionId);
        descriptor.writable = QFileInfo(root).isWritable();
        loadSidecar(descriptor);
        result.append(std::move(descriptor));
    }
    return result;
}

bool VanillaCompatibleProvider::loadSidecar(VanillaInstanceDescriptor& descriptor, QString* error)
{
    QFile file(lunaSettingsPath(descriptor.root));
    if (!file.exists())
        return true;
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) *error = parseError.errorString();
        return false;
    }
    const auto root = document.object();
    const auto object = root.value(QStringLiteral("settings")).toObject();
    if (object.isEmpty() && !root.value(QStringLiteral("settings")).isObject()) {
        if (error) *error = QStringLiteral("luna.settings.json has no settings object");
        return false;
    }
    if (!object.value(QStringLiteral("name")).toString().isEmpty()) descriptor.name = object.value("name").toString();
    descriptor.iconKey = object.value("iconKey").toString(descriptor.iconKey);
    descriptor.notes = object.value("notes").toString(descriptor.notes);
    descriptor.group = object.value("group").toString(descriptor.group);
    if (object.contains(QStringLiteral("loaderEnabled")))
        descriptor.loaderEnabled = object.value(QStringLiteral("loaderEnabled")).toBool(true);
    descriptor.loaderVersionOverride = object.value(QStringLiteral("loaderVersionOverride")).toString();
    descriptor.loaderTypeOverride = object.value(QStringLiteral("loaderTypeOverride")).toString();
    descriptor.loaderManifestFingerprint = object.value(QStringLiteral("loaderManifestFingerprint")).toString();
    QByteArray currentFingerprint;
    QFile fingerprintFile(descriptor.layout.versionJson);
    if (fingerprintFile.open(QIODevice::ReadOnly))
        currentFingerprint = QCryptographicHash::hash(fingerprintFile.readAll(), QCryptographicHash::Sha256).toHex();
    const bool sidecarMatchesManifest = descriptor.loaderManifestFingerprint.isEmpty()
        || QString::fromLatin1(currentFingerprint) == descriptor.loaderManifestFingerprint;
    if (!sidecarMatchesManifest) {
        descriptor.loaderTypeOverride.clear();
        descriptor.loaderVersionOverride.clear();
        descriptor.loaderProfileOverride = {};
        descriptor.loaderEnabled = true;
    }
    if (sidecarMatchesManifest && descriptor.loaderType == QStringLiteral("vanilla") && !descriptor.loaderTypeOverride.isEmpty())
        descriptor.loaderType = descriptor.loaderTypeOverride;
    if (object.value(QStringLiteral("loaderProfileOverride")).isObject())
        descriptor.loaderProfileOverride = object.value(QStringLiteral("loaderProfileOverride")).toObject();
    return true;
}

bool VanillaCompatibleProvider::saveSidecar(const VanillaInstanceDescriptor& descriptor, QString* error)
{
    if (descriptor.root.isEmpty()) {
        if (error) *error = QStringLiteral("An instance root is required.");
        return false;
    }
    if (!QDir().mkpath(descriptor.root)) {
        if (error) *error = QStringLiteral("Could not create the instance directory.");
        return false;
    }
    const QJsonObject settings{ { "name", descriptor.name }, { "iconKey", descriptor.iconKey }, { "notes", descriptor.notes },
                                { "group", descriptor.group }, { "loaderEnabled", descriptor.loaderEnabled },
                                { "loaderTypeOverride", descriptor.loaderTypeOverride },
                                { "loaderVersionOverride", descriptor.loaderVersionOverride },
                                { "loaderProfileOverride", descriptor.loaderProfileOverride },
                                { "loaderManifestFingerprint", descriptor.loaderManifestFingerprint } };
    const QJsonObject object{ { "formatVersion", 1 }, { "settings", settings } };
    return writeJsonFile(lunaSettingsPath(descriptor.root),
                         QJsonDocument(object),
                         error,
                         QObject::tr("Luna instance settings"));
}

