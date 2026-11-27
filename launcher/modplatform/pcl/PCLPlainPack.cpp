// SPDX-License-Identifier: GPL-3.0-only
#include "PCLPlainPack.h"
#include "PCLPack.h"
#include "modplatform/LegacyJarModPack.h"
#include "VanillaConversion.h"
#include "VanillaCompatibleProvider.h"
#include "FileSystem.h"
#include "archive/ArchiveReader.h"
#include "minecraft/GradleSpecifier.h"
#include "minecraft/MinecraftInstance.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QTemporaryDir>
#include <QtEndian>
#include <algorithm>

namespace PCL {
namespace {
QStringList versionPathParts(const QString& rawPath)
{
    auto path = QDir::fromNativeSeparators(rawPath);
    const auto parts = path.split('/', Qt::SkipEmptyParts);
    if (parts.size() < 3 || parts.at(parts.size() - 3) != "versions" || path.startsWith('/') || path.contains(':'))
        return {};
    const QSet<QString> excluded{ "assets", "libraries", "config", "mods", "logs", "saves", "resourcepacks", "shaderpacks" };
    for (const auto& part : parts.mid(0, parts.size() - 2)) {
        if (part == ".." || excluded.contains(part.toLower()))
            return {};
    }
    const auto version = parts.at(parts.size() - 2);
    if (version == "." || version == ".." || version.contains('\\'))
        return {};
    return parts;
}

bool isProfile(const QJsonObject& profile)
{
    return !profile.value("mainClass").toString().isEmpty()
        || !profile.value("inheritsFrom").toString().isEmpty()
        || !profile.value("patches").toArray().isEmpty()
        || !profile.value("downloads").toObject().value("client").toObject().isEmpty();
}

QString canonicalProfile(const QString& root, const QString& version)
{
    return root + "versions/" + version + '/' + version + ".json";
}
}  // namespace

bool isVersionProfilePath(const QString& path)
{
    return path.endsWith(".json", Qt::CaseInsensitive) && !versionPathParts(path).isEmpty();
}
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

QList<PlainPackCandidate> findPlainPackCandidates(const QStringList& archiveFiles,
                                                const QHash<QString, QJsonObject>& profiles)
{
    QList<PlainPackCandidate> result;
    const QSet<QString> files(archiveFiles.cbegin(), archiveFiles.cend());
    QSet<QString> discovered;
    for (const auto& path : archiveFiles) {
        if (!path.endsWith(".json", Qt::CaseInsensitive))
            continue;
        const auto parts = versionPathParts(path);
        if (parts.isEmpty()) continue;
        const auto version = parts.at(parts.size() - 2);
        const auto prefix = parts.mid(0, parts.size() - 3).join('/');
        const auto root = prefix.isEmpty() ? QString() : prefix + '/';
        const auto expected = canonicalProfile(root, version);
        if (path != expected && (files.contains(expected) || !isProfile(profiles.value(path))))
            continue;
        result.append({ root, version, path, {}, {} });
        discovered.insert(expected);
    }
    if (!profiles.isEmpty()) {
        for (const auto& path : archiveFiles) {
            if (!path.endsWith(".jar", Qt::CaseInsensitive)) continue;
            const auto parts = versionPathParts(path);
            if (parts.isEmpty()) continue;
            const auto version = parts.at(parts.size() - 2);
            if (parts.last() != version + ".jar") continue;
            const auto prefix = parts.mid(0, parts.size() - 3).join('/');
            const auto root = prefix.isEmpty() ? QString() : prefix + '/';
            const auto expected = canonicalProfile(root, version);
            if (discovered.contains(expected)) continue;
            discovered.insert(expected);
            result.append({ root, version, expected, {}, {} });
        }
    }
    return result;
}

void validatePlainPackCandidate(PlainPackCandidate& candidate, const QHash<QString, QJsonObject>& profiles,
                                const QSet<QString>& files, bool legacy)
{
    const auto raw = profiles.value(candidate.profilePath);
    if (raw.isEmpty() || !isProfile(raw)) {
        candidate.error = files.contains(candidate.profilePath)
            ? QObject::tr("Invalid or unreadable version JSON: %1").arg(QFileInfo(candidate.profilePath).fileName())
            : QObject::tr("Missing version JSON: %1").arg(candidate.profilePath);
        return;
    }
    if (legacy) {
        const auto base = LegacyJarModPack::baseVersion(raw, candidate.version, candidate.error);
        if (!base.isEmpty()) candidate.details = QObject::tr("Minecraft %1 · Legacy JAR patch").arg(base);
        return;
    }

    auto version = candidate.version;
    auto profile = raw;
    QSet<QString> visited;
    bool hasClient = false;
    bool hasRuntime = false;
    bool explicitJar = false;
    QString minecraft;
    for (;;) {
        if (visited.contains(version)) {
            candidate.error = QObject::tr("Version inheritance cycle at %1.").arg(version);
            return;
        }
        visited.insert(version);
        hasRuntime |= !profile.value("mainClass").toString().isEmpty() || !profile.value("patches").toArray().isEmpty();
        const auto jar = profile.value("jar").toString().trimmed();
        const auto download = profile.value("downloads").toObject().value("client").toObject().value("url").toString();
        const auto folder = candidate.root + "versions/" + version + '/';
        if (!explicitJar && !hasClient) {
            explicitJar = !jar.isEmpty();
            const auto jarId = explicitJar ? jar : version;
            hasClient = files.contains(folder + jarId + ".jar")
                || files.contains(candidate.root + "versions/" + jarId + '/' + jarId + ".jar");
        }
        // Client downloads remain available even when the archive only carries a profile.
        if (!download.isEmpty()) hasClient = true;
        if (minecraft.isEmpty())
            minecraft = profile.value("minecraftVersion").toString(profile.value("clientVersion").toString());
        const auto parent = profile.value("inheritsFrom").toString().trimmed();
        if (parent.isEmpty()) break;
        if (parent == "." || parent == ".." || parent.contains('/') || parent.contains('\\') || parent.contains(':')) {
            candidate.error = QObject::tr("Invalid parent version ID: %1").arg(parent);
            return;
        }
        const auto path = canonicalProfile(candidate.root, parent);
        profile = profiles.value(path);
        if (profile.isEmpty()) {
            candidate.error = files.contains(path)
                ? QObject::tr("Parent version %1 has an invalid or unreadable JSON file.").arg(parent)
                : QObject::tr("Missing parent version %1 (%1.json).").arg(parent);
            return;
        }
        version = parent;
    }
    if (!hasRuntime) {
        candidate.error = QObject::tr("The version profile has no launch configuration.");
        return;
    }
    const auto loader = VanillaCompatibleProvider::detectedLoaderType(raw);
    const auto loaderVersion = VanillaCompatibleProvider::detectedLoaderVersion(raw);
    const bool standardLoader = !loaderVersion.isEmpty() && !VanillaCompatibleProvider::loaderUidForType(loader).isEmpty()
        && VanillaCompatibleProvider::isStandardLoaderProfile(raw, loader, raw);
    if (!hasClient && !standardLoader) {
        candidate.error = QObject::tr("The client JAR is missing and the profile has no client download URL.");
        return;
    }
    if (minecraft.isEmpty() && version != candidate.version) minecraft = version;
    if (!minecraft.isEmpty()) candidate.details = QObject::tr("Minecraft %1").arg(minecraft);
    if (!loader.isEmpty() && !loaderVersion.isEmpty()) {
        if (!candidate.details.isEmpty()) candidate.details += " · ";
        candidate.details += loader + ' ' + loaderVersion;
    }
}

PlainPackConversionResult convertPlainPack(MinecraftInstance& instance, const QString& sourceRoot, const QString& version)
{
    PlainPackConversionResult result;
    QTemporaryDir temporary;
    if (!temporary.isValid()) { result.error = QObject::tr("Could not create a conversion directory."); return result; }
    const auto target = QDir(temporary.path()).filePath("instance");
    VanillaImportOptions options; options.name = instance.name();
    const auto report = VanillaConverter::importToPrism(FS::PathCombine(sourceRoot,"versions",version),target,options);
    result.error = report.error;
    if (!report.succeeded) return result;
    if (!FS::overrideFolder(instance.instanceRoot(),target)) {
        result.error = QObject::tr("Could not copy the converted instance."); return result;
    }
    result.succeeded = true;
    return result;
}
} // namespace PCL
