#pragma once

#include "ReleaseUpdateManifest.h"

#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QString>
#include <QCryptographicHash>
#include <QFile>

namespace WindowsUpdate {
using ReleaseUpdate::verifyPackage;
using ReleaseUpdate::normalizedVersion;

inline QJsonObject artifact(const QJsonObject& manifest, const QString& architecture, bool needsNeo)
{
    if (!ReleaseUpdate::validManifest(manifest)) return {};
    QJsonObject selected;
    for (const auto value : manifest.value("artifacts").toArray()) {
        const auto item = value.toObject();
        const auto name = item.value("asset").toString();
        if (item.value("platform").toString() != "windows" || item.value("architecture").toString() != architecture ||
            (needsNeo && !item.value("neoUi").toBool()) ||
            !name.startsWith("LunaLauncher-Windows-", Qt::CaseInsensitive) || !name.endsWith("-Setup.exe", Qt::CaseInsensitive) ||
            name.contains('/') || name.contains('\\') || name.contains(':') || name.contains("..") ||
            item.value("size").toInteger() <= 0 ||
            !QRegularExpression("^[a-fA-F0-9]{64}$").match(item.value("sha256").toString()).hasMatch()) continue;
        if (!selected.isEmpty()) return {}; // Ambiguous release: require one exact package.
        selected = item;
    }
    return selected;
}
}
