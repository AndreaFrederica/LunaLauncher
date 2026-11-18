// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "AppImageUpdate.h"
#include <QJsonDocument>
#include <QVersionNumber>
#ifdef __GLIBC__
#include <gnu/libc-version.h>
#endif

namespace LinuxPortable {
inline QString architecture()
{
    const auto cpu = QSysInfo::buildCpuArchitecture();
    return cpu == "x86_64" ? "x64" : cpu == "arm64" || cpu == "aarch64" ? "arm64" : QString();
}
inline QString glibcVersion()
{
#ifdef __GLIBC__
    return QString::fromLatin1(gnu_get_libc_version());
#else
    return {};
#endif
}
inline bool safePath(const QString& path)
{
    return !path.isEmpty() && !path.startsWith('/') && !path.contains('\\') && !path.contains(':') &&
        !path.contains(QChar::Null) && path == QDir::cleanPath(path) && path != "." &&
        path != ".." && !path.startsWith("../");
}
inline bool userPath(const QString& path)
{
    const auto parts = path.split('/');
    for (const auto& part : parts)
        if (QStringList{"UserData", "instances", "accounts.json", ".luna-data-dir", ".luna-use-system-data",
                        "portable.txt", "portable.txt.disabled", "lunalauncher.cfg", "lunalauncher_update.cfg"}.contains(part)) return true;
    return false;
}
inline bool compatible(const QJsonObject& item, const QString& arch, const QString& glibc)
{
    const auto minimum = item.value("minGlibc").toString();
    const QRegularExpression version("^[0-9]+\\.[0-9]+(?:\\.[0-9]+)?$");
    return !arch.isEmpty() && item.value("architecture").toString() == arch && item.value("libc").toString() == "glibc" &&
        version.match(minimum).hasMatch() && version.match(glibc).hasMatch() &&
        QVersionNumber::fromString(glibc) >= QVersionNumber::fromString(minimum);
}
inline QJsonObject metadata(const QString& root)
{
    QFile file(QDir(root).filePath(".luna-portable.json"));
    if (file.size() > 2 * 1024 * 1024 || !file.open(QIODevice::ReadOnly)) return {};
    const auto data = QJsonDocument::fromJson(file.readAll()).object();
    if (!ReleaseUpdate::validManifest(data) || data.value("format").toString() != "portable-tar" ||
        !compatible(data, architecture(), glibcVersion()) || data.value("files").toArray().isEmpty()) return {};
    return data;
}
inline QString runningRoot(const QString& binaryDirectory)
{
#ifdef Q_OS_LINUX
    // Only our deployed usr/bin layout with an explicit package manifest opts in.
    // An ordinary /usr or package-manager installation has no such marker.
    const auto root = QDir(binaryDirectory).absoluteFilePath("../..");
    const auto canonical = QFileInfo(root).canonicalFilePath();
    if (!canonical.isEmpty() && QFileInfo(binaryDirectory).canonicalFilePath() == canonical + "/usr/bin" &&
        QFileInfo(canonical + "/AppRun").isExecutable() && !metadata(canonical).isEmpty() &&
        AppImageUpdate::runningImage(binaryDirectory).isEmpty()) return canonical;
#endif
    return {};
}
inline QJsonObject artifact(const QJsonObject& manifest, const QString& arch, bool neo, const QString& glibc = glibcVersion())
{
    if (!ReleaseUpdate::validManifest(manifest)) return {};
    QJsonObject selected;
    for (const auto value : manifest.value("artifacts").toArray()) {
        const auto item = value.toObject();
        const auto asset = item.value("asset").toString();
        if (item.value("platform").toString() != "linux" || item.value("format").toString() != "portable-tar" ||
            !compatible(item, arch, glibc) || (neo && !item.value("neoUi").toBool()) || !safePath(asset) || asset.contains('/') ||
            !asset.startsWith("LunaLauncher-Linux-") || !asset.endsWith("-portable.tar.gz") ||
            item.value("size").toInteger() <= 0 || item.value("unpackedSize").toInteger() <= 0 ||
            item.value("unpackedSize").toInteger() > 12LL * 1024 * 1024 * 1024 ||
            !QRegularExpression("^[a-fA-F0-9]{64}$").match(item.value("sha256").toString()).hasMatch()) continue;
        if (!selected.isEmpty()) return {};
        selected = item;
    }
    return selected;
}
}
