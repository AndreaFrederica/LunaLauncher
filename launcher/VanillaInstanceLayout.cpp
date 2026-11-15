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
    QStringLiteral("net.fabricmc.intermediary"),
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

QJsonObject readObject(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()).object() : QJsonObject{};
}

bool safeVersionId(const QString& id)
{
    return !id.isEmpty() && id != "." && id != ".." && !id.contains('/') && !id.contains('\\') && !id.contains(':');
}

QString selectedClientJar(const QString& versionJson, const QString& versionsRoot, const QString& versionId)
{
    if (!safeVersionId(versionId)) return {};
    auto raw = readObject(versionJson);
    auto id = raw.value("jar").toString().trimmed();
    if (id.isEmpty() && QFileInfo(QDir(QFileInfo(versionJson).absolutePath()).filePath(versionId + ".jar")).isFile()) id = versionId;
    QSet<QString> visited;
    if (id.isEmpty()) {
        id = versionId;
        while (!visited.contains(id)) {
            visited.insert(id);
            const auto explicitJar = raw.value("jar").toString().trimmed();
            if (!explicitJar.isEmpty()) { id = explicitJar; break; }
            if (QFileInfo(QDir(versionsRoot).filePath(id + '/' + id + ".jar")).isFile()) break;
            const auto parent = raw.value("inheritsFrom").toString().trimmed();
            if (parent.isEmpty()) break;
            if (!safeVersionId(parent)) return {};
            id = parent; raw = readObject(QDir(versionsRoot).filePath(id + '/' + id + ".json"));
        }
    }
    if (!safeVersionId(id)) return {};
    const auto sibling = QDir(QFileInfo(versionJson).absolutePath()).filePath(id + ".jar");
    if (QFileInfo(sibling).isFile()) return QDir::cleanPath(sibling);
    return QDir::cleanPath(QDir(versionsRoot).filePath(id + '/' + id + ".jar"));
}

void resolveGameDirectory(VanillaInstanceLayout& result, const QString& sharedRoot, const QString& versionRoot)
{
    const auto shared = QDir::cleanPath(sharedRoot);
    const auto settings = readObject(QDir(versionRoot).filePath("luna.settings.json")).value("settings").toObject();
    const auto mode = settings.value("VanillaGameDirectoryMode").toString();
    QString gameRoot;
    if (mode == "shared") gameRoot = shared;
    if (mode == "isolated") gameRoot = versionRoot;
    if (gameRoot.isEmpty()) {
        auto hmcl = readObject(QDir(versionRoot).filePath("hmclversion.cfg"));
        if (hmcl.isEmpty() || hmcl.value("usesGlobal").toBool()) {
            for (const auto& folder : { shared, QDir(shared).absoluteFilePath("..") }) {
                const auto profiles = readObject(QDir(folder).filePath("hmcl.json")).value("configurations").toObject();
                for (const auto& value : profiles) {
                    const auto profile = value.toObject();
                    const auto path = QDir::cleanPath(QDir(folder).absoluteFilePath(profile.value("gameDir").toString()));
                    if (path.compare(shared, Qt::CaseInsensitive) == 0) {
                        hmcl = profile.value("global").toObject();
                        hmcl.remove("usesGlobal");
                    }
                }
            }
        }
        if (hmcl.contains("gameDirType") && !hmcl.value("usesGlobal").toBool(false)) {
            const auto type = hmcl.value("gameDirType").toInt(-1);
            if (type == 0) gameRoot = shared;
            if (type == 1) gameRoot = versionRoot;
            if (type == 2 && !hmcl.value("gameDir").toString().isEmpty())
                gameRoot = QDir(shared).absoluteFilePath(hmcl.value("gameDir").toString());
        }
        if (gameRoot.isEmpty()) {
            QFile pcl(QDir(versionRoot).filePath("PCL/Setup.ini"));
            if (pcl.open(QIODevice::ReadOnly)) {
                const auto lines = QString::fromUtf8(pcl.readAll()).split('\n');
                QString legacy;
                for (const auto& line : lines) {
                    const auto key = line.section(':', 0, 0).trimmed();
                    const auto value = line.section(':', 1).trimmed();
                    if (key == "VersionArgumentIndieV2") {
                        if (value.compare("True", Qt::CaseInsensitive) == 0) gameRoot = versionRoot;
                        if (value.compare("False", Qt::CaseInsensitive) == 0) gameRoot = shared;
                    }
                    if (key == "VersionArgumentIndie") legacy = value;
                }
                if (gameRoot.isEmpty() && legacy == "1") gameRoot = versionRoot;
                if (gameRoot.isEmpty() && legacy == "2") gameRoot = shared;
            }
        }
    }
    if (gameRoot.isEmpty()) {
        const auto version = readObject(result.versionJson);
        QString explicitRoot;
        for (const auto& key : { "gameDir", "gameDirectory", "gameRoot" }) {
            if (!version.value(key).toString().trimmed().isEmpty()) { explicitRoot = version.value(key).toString(); break; }
        }
        if (!explicitRoot.isEmpty()) gameRoot = QDir(shared).absoluteFilePath(explicitRoot);
        else if (hasGameContent(QDir(versionRoot).filePath("game"))) gameRoot = QDir(versionRoot).filePath("game");
        else if (hasGameContent(versionRoot)) gameRoot = versionRoot;
        else if (hasGameContent(shared) || QFileInfo::exists(QDir(shared).filePath("options.txt"))) gameRoot = shared;
        else gameRoot = versionRoot; // New/empty profiles default to isolation.
    }
    result.gameRoot = QDir::cleanPath(gameRoot);
    result.isolated = result.gameRoot != shared;
    result.mode = !result.isolated ? VanillaGameLayout::SharedGameRoot
        : result.gameRoot == QDir::cleanPath(versionRoot) ? VanillaGameLayout::IsolatedVersionRoot : VanillaGameLayout::ExplicitGameDir;
}

QString firstVersion(const QString& versionsRoot, const QString& requested)
{
    if (!requested.isEmpty() && !safeVersionId(requested)) return {};
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
        else if (it.value().isArray() && merged.value(key).isArray() && (key == QStringLiteral("libraries") || key == QStringLiteral("game") || key == QStringLiteral("jvm"))) {
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
    if (!safeVersionId(versionId)) {
        if (error) *error = QObject::tr("Unsafe version profile ID: %1").arg(versionId);
        return {};
    }
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
    if (!parent.isEmpty()) resolved.insert("inheritsFrom",parent);
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
        result.clientJar = selectedClientJar(result.versionJson,result.versionsRoot,result.versionId);
        result.gameRoot = QFileInfo(QDir(result.sourceRoot).filePath(QStringLiteral("game"))).isDir()
            ? QDir::cleanPath(QDir(result.sourceRoot).filePath(QStringLiteral("game")))
            : result.sourceRoot;
        result.mode = VanillaGameLayout::IsolatedVersionRoot;
        result.isolated = true;
        resolveGameDirectory(result, sharedRoot, result.sourceRoot);
        return result;
    }
    result.versionId = firstVersion(result.versionsRoot, requestedVersion);
    if (result.versionId.isEmpty())
        return result;

    const auto versionRoot = QDir(result.versionsRoot).filePath(result.versionId);
    result.versionJson = QDir(versionRoot).filePath(result.versionId + ".json");

    QJsonObject version = loadVanillaVersionJson(result.sourceRoot, result.versionId);
    result.clientJar = selectedClientJar(result.versionJson,result.versionsRoot,result.versionId);

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
    resolveGameDirectory(result, sharedRoot, versionRoot);
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
                if (value.toObject().value(QStringLiteral("disabled")).toBool())
                    continue;
                if (uid != QStringLiteral("net.minecraft") && uid != QStringLiteral("org.lwjgl") && uid != QStringLiteral("org.lwjgl3") && !standardLoaderIds.contains(uid)) {
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
            if (patch.completeBaseName() != QStringLiteral("net.minecraft") && patch.completeBaseName() != QStringLiteral("org.lwjgl") && patch.completeBaseName() != QStringLiteral("org.lwjgl3") && !standardLoaderIds.contains(patch.completeBaseName()))
                result.blockers.append(QStringLiteral("patches contains Prism component override %1.").arg(patch.fileName()));
        }
    }
    if (root.exists(QStringLiteral("instance.cfg")))
        result.warnings.append(QStringLiteral("instance.cfg will remain in the source; it is not written to the vanilla destination."));

    result.compatible = result.blockers.isEmpty();
    return result;
}

