// SPDX-License-Identifier: GPL-3.0-only

#include "VanillaInstanceLayout.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QFile>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>

namespace {

const QSet<QString> standardLoaderIds{
    QStringLiteral("net.fabricmc.fabric-loader"),
    QStringLiteral("net.minecraftforge"),
    QStringLiteral("net.neoforged"),
    QStringLiteral("org.quiltmc.quilt-loader"),
};

bool hasGameContent(const QString& path)
{
    const QDir dir(path);
    for (const auto& name : { QStringLiteral("mods"), QStringLiteral("config"), QStringLiteral("saves"),
                              QStringLiteral("resourcepacks"), QStringLiteral("shaderpacks") }) {
        if (dir.exists(name))
            return true;
    }
    return false;
}

QString firstVersion(const QString& versionsRoot, const QString& requested)
{
    if (!requested.isEmpty() && QFileInfo::exists(QDir(versionsRoot).filePath(requested + '/' + requested + ".json")))
        return requested;

    const auto entries = QDir(versionsRoot).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const auto& entry : entries) {
        const auto id = entry.fileName();
        if (QFileInfo::exists(QDir(versionsRoot).filePath(id + '/' + id + ".json")))
            return id;
    }
    return {};
}

QString explicitGameRoot(const QJsonObject& version, const QString& sourceRoot)
{
    for (const auto& key : { QStringLiteral("gameDir"), QStringLiteral("gameDirectory"), QStringLiteral("gameRoot") }) {
        const auto value = version.value(key).toString().trimmed();
        if (!value.isEmpty())
            return QDir(sourceRoot).absoluteFilePath(value);
    }
    return {};
}

QJsonObject mergeVersionObjects(const QJsonObject& parent, const QJsonObject& child)
{
    QJsonObject merged = parent;
    for (auto it = child.begin(); it != child.end(); ++it) {
        const auto key = it.key();
        if (key == QStringLiteral("inheritsFrom"))
            continue;
        if (it.value().isObject() && merged.value(key).isObject())
            merged.insert(key, mergeVersionObjects(merged.value(key).toObject(), it.value().toObject()));
        else if (it.value().isArray() && merged.value(key).isArray() && (key == QStringLiteral("libraries") || key == QStringLiteral("arguments"))) {
            auto values = merged.value(key).toArray();
            for (const auto& value : it.value().toArray())
                values.append(value);
            merged.insert(key, values);
        } else {
            merged.insert(key, it.value());
        }
    }
    return merged;
}

QJsonObject loadRawVersion(const QString& sourceRoot, const QString& versionId, QString* error)
{
    const auto path = QDir(sourceRoot).filePath(QStringLiteral("versions/%1/%1.json").arg(versionId));
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = QObject::tr("Could not open version JSON %1: %2").arg(path, file.errorString());
        return {};
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) *error = QObject::tr("Could not parse version JSON %1: %2").arg(path, parseError.errorString());
        return {};
    }
    return document.object();
}

QJsonObject resolveVersion(const QString& sourceRoot, const QString& versionId, QSet<QString>& visiting, QString* error)
{
    if (visiting.contains(versionId)) {
        if (error) *error = QObject::tr("Version inheritance cycle detected at %1.").arg(versionId);
        return {};
    }
    visiting.insert(versionId);
    const auto raw = loadRawVersion(sourceRoot, versionId, error);
    if (raw.isEmpty()) {
        visiting.remove(versionId);
        return {};
    }
    QJsonObject resolved;
    const auto parent = raw.value(QStringLiteral("inheritsFrom")).toString().trimmed();
    if (!parent.isEmpty())
        resolved = resolveVersion(sourceRoot, parent, visiting, error);
    resolved = mergeVersionObjects(resolved, raw);
    resolved.insert(QStringLiteral("id"), raw.value(QStringLiteral("id")).toString(versionId));
    visiting.remove(versionId);
    return resolved;
}

}  // namespace

QJsonObject loadVanillaVersionJson(const QString& sourceRoot, const QString& versionId, QString* error)
{
    QSet<QString> visiting;
    return resolveVersion(QDir(sourceRoot).absolutePath(), versionId, visiting, error);
}

VanillaInstanceLayout detectVanillaInstanceLayout(const QString& sourceRoot, const QString& requestedVersion)
{
    VanillaInstanceLayout result;
    result.sourceRoot = QDir(sourceRoot).absolutePath();
    // HMCL/PCL isolated instances use versions/<id>/<id>.json as the
    // instance root. Accept that directory directly while still resolving
    // shared assets/libraries from the enclosing game root.
    const auto directName = QFileInfo(result.sourceRoot).fileName();
    const auto directJson = QDir(result.sourceRoot).filePath(directName + QStringLiteral(".json"));
    const bool directVersionRoot = QFileInfo::exists(directJson) && QFileInfo(result.sourceRoot).dir().dirName() == QStringLiteral("versions");
    const auto sharedRoot = directVersionRoot ? QDir(result.sourceRoot).filePath(QStringLiteral("../..")) : result.sourceRoot;
    result.versionsRoot = QDir::cleanPath(directVersionRoot ? QDir(result.sourceRoot).filePath(QStringLiteral(".."))
                                                              : QDir(result.sourceRoot).filePath(QStringLiteral("versions")));
    result.librariesRoot = QDir::cleanPath(QDir(sharedRoot).filePath(QStringLiteral("libraries")));
    result.assetsRoot = QDir::cleanPath(QDir(sharedRoot).filePath(QStringLiteral("assets")));
    if (directVersionRoot) {
        result.versionId = directName;
        result.versionJson = directJson;
        const auto version = loadVanillaVersionJson(sharedRoot, result.versionId);
        const auto jarVersion = version.value(QStringLiteral("jar")).toString().trimmed();
        const auto jarId = jarVersion.isEmpty() ? result.versionId : jarVersion;
        result.clientJar = QDir::cleanPath(QDir(result.sourceRoot).filePath(jarId + QStringLiteral(".jar")));
        // HMCL/PCL may isolate the profile JSON and game content while
        // keeping the inherited/base client jar in the shared versions tree.
        // Prefer the profile-local jar, then fall back to that shared jar.
        if (!QFileInfo::exists(result.clientJar))
            result.clientJar = QDir::cleanPath(QDir(result.versionsRoot).filePath(jarId + QStringLiteral("/") + jarId + QStringLiteral(".jar")));
        result.gameRoot = QFileInfo(QDir(result.sourceRoot).filePath(QStringLiteral("game"))).isDir()
            ? QDir::cleanPath(QDir(result.sourceRoot).filePath(QStringLiteral("game")))
            : result.sourceRoot;
        result.mode = VanillaGameLayout::IsolatedVersionRoot;
        result.isolated = true;
        return result;
    }
    result.versionId = firstVersion(result.versionsRoot, requestedVersion);
    if (result.versionId.isEmpty())
        return result;

    const auto versionRoot = QDir(result.versionsRoot).filePath(result.versionId);
    result.versionJson = QDir(versionRoot).filePath(result.versionId + ".json");

    QJsonObject version = loadVanillaVersionJson(result.sourceRoot, result.versionId);
    // A profile that inherits a release (common in PCL/HMCL and official
    // launcher metadata) may not have its own jar. The resolved `jar` field
    // names the version directory containing the executable client jar.
    const auto jarVersion = version.value(QStringLiteral("jar")).toString().trimmed();
    const auto jarId = jarVersion.isEmpty() ? result.versionId : jarVersion;
    result.clientJar = QDir(result.versionsRoot).filePath(jarId + "/" + jarId + ".jar");

    const auto explicitRoot = explicitGameRoot(version, result.sourceRoot);
    if (!explicitRoot.isEmpty()) {
        result.gameRoot = QDir::cleanPath(explicitRoot);
        result.mode = VanillaGameLayout::ExplicitGameDir;
        result.isolated = true;
    } else {
        const auto isolatedRoot = QDir(versionRoot).filePath(QStringLiteral("game"));
        if (hasGameContent(isolatedRoot)) {
            result.gameRoot = isolatedRoot;
            result.mode = VanillaGameLayout::IsolatedVersionRoot;
            result.isolated = true;
        } else if (hasGameContent(versionRoot)) {
            result.gameRoot = versionRoot;
            result.mode = VanillaGameLayout::IsolatedVersionRoot;
            result.isolated = true;
        } else {
            result.gameRoot = result.sourceRoot;
            result.mode = VanillaGameLayout::SharedGameRoot;
            result.isolated = false;
        }
    }
    return result;
}

VanillaCompatibilityReport checkVanillaExportCompatibility(const QString& prismInstanceRoot)
{
    VanillaCompatibilityReport result;
    const QDir root(prismInstanceRoot);
    if (!root.exists()) {
        result.compatible = false;
        result.blockers.append(QStringLiteral("The Prism instance directory does not exist."));
        return result;
    }

    // These files describe Prism's component graph and cannot be represented
    // in a vanilla/PCL/HMCL instance without losing launcher behavior.
    const auto packPath = root.filePath(QStringLiteral("mmc-pack.json"));
    if (root.exists(QStringLiteral("mmc-pack.json"))) {
        QFile packFile(packPath);
        QJsonParseError parseError;
        const auto document = packFile.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(packFile.readAll(), &parseError) : QJsonDocument();
        bool vanillaBaseOnly = false;
        if (parseError.error == QJsonParseError::NoError && document.isObject()) {
            const auto components = document.object().value(QStringLiteral("components")).toArray();
            vanillaBaseOnly = !components.isEmpty();
            for (const auto& value : components) {
                if (!value.isObject()) {
                    vanillaBaseOnly = false;
                    break;
                }
                const auto uid = value.toObject().value(QStringLiteral("uid")).toString();
                if (uid != QStringLiteral("net.minecraft") && !standardLoaderIds.contains(uid)) {
                    vanillaBaseOnly = false;
                    break;
                }
            }
        }
        if (!vanillaBaseOnly)
            result.blockers.append(QStringLiteral("mmc-pack.json contains Prism component metadata beyond the vanilla base component."));
    }
    const auto patches = root.entryInfoList(QStringList{ QStringLiteral("patches") }, QDir::Dirs | QDir::NoDotAndDotDot);
    if (!patches.isEmpty()) {
        const auto patchEntries = QDir(root.filePath(QStringLiteral("patches"))).entryInfoList(QStringList{ QStringLiteral("*.json") }, QDir::Files);
        for (const auto& patch : patchEntries) {
            if (patch.baseName() != QStringLiteral("net.minecraft") && !standardLoaderIds.contains(patch.baseName()))
                result.blockers.append(QStringLiteral("patches contains Prism component override %1.").arg(patch.fileName()));
        }
    }
    if (root.exists(QStringLiteral("instance.cfg")))
        result.warnings.append(QStringLiteral("instance.cfg will remain in the source; it is not written to the vanilla destination."));

    result.compatible = result.blockers.isEmpty();
    return result;
}

