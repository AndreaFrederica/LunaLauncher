// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QString>
#include <QStringList>

/** Selects the authoritative Minecraft client for a launch. */
class MinecraftClientRoute {
public:
    static MinecraftClientRoute external(QString clientJar, QString version, QString loaderType);

    QStringList applyToClassPath(const QStringList& jars) const;
    QStringList jvmArguments() const;

private:
    QString m_clientJar;
    QString m_version;
    QString m_loaderType;
};
