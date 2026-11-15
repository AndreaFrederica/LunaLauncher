// SPDX-License-Identifier: GPL-3.0-only
#include "MinecraftClientRoute.h"

#include <QDir>
#include <QFileInfo>
#include <utility>

MinecraftClientRoute MinecraftClientRoute::external(QString clientJar, QString version, QString loaderType)
{
    MinecraftClientRoute route;
    route.m_clientJar = QDir::cleanPath(std::move(clientJar));
    route.m_version = std::move(version);
    route.m_loaderType = std::move(loaderType).trimmed().toLower();
    return route;
}

QStringList MinecraftClientRoute::applyToClassPath(const QStringList& jars) const
{
    if (m_clientJar.isEmpty() || !QFileInfo::exists(m_clientJar))
        return jars;

    const auto normalizedClientJar = QDir::fromNativeSeparators(QFileInfo(m_clientJar).absoluteFilePath()).toLower();
    const auto cachedClientName = QStringLiteral("minecraft-%1-client.jar").arg(m_version).toLower();
    const auto clientDirectoryMarker = QStringLiteral("/com/mojang/minecraft/");
    QStringList filtered;
    for (const auto& jar : jars) {
        const auto normalized = QDir::fromNativeSeparators(QFileInfo(jar).absoluteFilePath()).toLower();
        const auto fileName = QFileInfo(normalized).fileName();
        const bool isForeignClient = normalized == normalizedClientJar;
        const bool isMojangClient = normalized.contains(clientDirectoryMarker)
            || fileName == cachedClientName
            || fileName == QStringLiteral("minecraft-game-client.jar");
        if (isMojangClient && !isForeignClient)
            continue;
        if (!filtered.contains(jar, Qt::CaseInsensitive))
            filtered.append(jar);
    }

    const auto clientPath = QFileInfo(m_clientJar).absoluteFilePath();
    if (!filtered.contains(clientPath, Qt::CaseInsensitive))
        filtered.append(clientPath);
    return filtered;
}

QStringList MinecraftClientRoute::jvmArguments() const
{
    if (m_loaderType == QStringLiteral("forge") || m_loaderType == QStringLiteral("neoforge"))
        return { QStringLiteral("-Dforgewrapper.minecraft=") + QFileInfo(m_clientJar).absoluteFilePath() };
    return {};
}
