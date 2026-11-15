// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QString>
#include <QStringList>
#include <QJsonObject>

enum class VanillaGameLayout {
    SharedGameRoot,
    IsolatedVersionRoot,
    ExplicitGameDir,
    AutoDetect,
};

struct VanillaInstanceLayout {
    QString sourceRoot;
    QString versionId;
    QString versionJson;
    QString clientJar;
    QString gameRoot;
    QString versionsRoot;
    QString librariesRoot;
    QString assetsRoot;
    VanillaGameLayout mode = VanillaGameLayout::AutoDetect;
    bool isolated = false;

    bool isValid() const { return !sourceRoot.isEmpty() && !versionId.isEmpty() && !versionJson.isEmpty(); }
};

struct VanillaCompatibilityReport {
    bool compatible = true;
    QStringList blockers;
    QStringList warnings;
};

/** Detect the local version and game directory layout used by PCL/HMCL/vanilla. */
VanillaInstanceLayout detectVanillaInstanceLayout(const QString& sourceRoot, const QString& requestedVersion = {});

/** Load a version JSON and expand its local inheritsFrom chain. */
QJsonObject loadVanillaVersionJson(const QString& sourceRoot, const QString& versionId, QString* error = nullptr);

/** Check whether a Prism instance can be represented by a vanilla-compatible directory. */
VanillaCompatibilityReport checkVanillaExportCompatibility(const QString& prismInstanceRoot);

