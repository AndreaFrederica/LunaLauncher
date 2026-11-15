// SPDX-License-Identifier: GPL-3.0-only
#include "PCLPlainPack.h"
#include "PCLPack.h"
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
        const QSet<QString> excluded{ "assets", "libraries", "config", "mods", "logs", "saves", "resourcepacks", "shaderpacks" };
        bool nestedResource = false;
        for (const auto& part : rootParts) nestedResource = nestedResource || excluded.contains(part.toLower()) || part == "..";
        if (nestedResource) continue;
        result.append({ rootParts.join('/') + (rootParts.isEmpty() ? "" : "/"), version });
    }
    return result;
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
