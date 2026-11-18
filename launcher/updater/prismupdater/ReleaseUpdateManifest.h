// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QCryptographicHash>
#include <QFile>
#include <QJsonObject>
#include <QRegularExpression>

namespace ReleaseUpdate {
inline bool verifyPackage(const QString& path, const QJsonObject& artifact, bool allowEmpty = false)
{
    QFile file(path);
    QCryptographicHash hash(QCryptographicHash::Sha256);
    const auto size = artifact.value("size").toInteger(-1);
    return size >= (allowEmpty ? 0 : 1) && file.size() == size &&
        file.open(QIODevice::ReadOnly) && hash.addData(&file) &&
        hash.result().toHex() == artifact.value("sha256").toString().toLower().toLatin1();
}

inline QString normalizedVersion(QString version)
{
    version = version.trimmed();
    if (version.startsWith('v', Qt::CaseInsensitive)) version.remove(0, 1);
    return version;
}

inline bool validManifest(const QJsonObject& manifest)
{
    return manifest.value("schemaVersion").toInt() == 1 && manifest.value("protocol").toInt() == 1 &&
        QRegularExpression("^v?[0-9]+\\.[0-9]+\\.[0-9]+(?:[-+][A-Za-z0-9.-]+)?$").match(manifest.value("version").toString()).hasMatch();
}
}
