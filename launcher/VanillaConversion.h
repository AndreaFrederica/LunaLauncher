// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QStringList>
#include <functional>

#include "VanillaInstanceLayout.h"

struct VanillaConversionReport {
    bool succeeded = false;
    QStringList blockers;
    QStringList warnings;
    QStringList copied;
    QString error;
    QString destination;
    QJsonObject clientArtifact;

    QJsonObject toJson() const;
};

struct VanillaConversionProgress {
    QString stage;
    qint64 current = 0;
    qint64 total = 0;
    QString unit = "files";
    QString details;
};

// Callbacks run on the filesystem worker, never on an application model.
struct VanillaConversionFeedback {
    std::function<void(const VanillaConversionProgress&)> progress;
    std::function<bool()> cancelled;
    void update(const QString& stage, qint64 current = 0, qint64 total = 0,
                const QString& unit = "files", const QString& details = {}) const
    {
        if (progress) progress({stage, current, total, unit, details});
    }
};

struct VanillaImportOptions {
    QString name;
    QString group;
    bool copyGameContent = true;
    bool keepSource = true;
    VanillaConversionFeedback feedback;
};

struct VanillaExportOptions {
    QString versionId;
    QJsonObject versionJson;
    // Resolved MMC components, including dependencies such as LWJGL, in
    // application order. Missing metadata must never produce a partial profile.
    QJsonArray componentProfiles;
    QString librariesRoot;
    QString assetsRoot;
    // Optional launcher dialect metadata used by the conversion workflow.
    // Explicit extensionFields are copied into the generated version JSON;
    // Luna does not invent PCL/HMCL-owned top-level fields.
    QString dialect;
    QString name;
    QString group;
    QJsonObject extensionFields;
    bool isolated = false;
    bool copyGameContent = true;
    VanillaConversionFeedback feedback;
};

struct VanillaLoaderInstallOptions {
    QString versionId;
    QString loaderId;
    QString loaderVersion;
    QJsonObject versionJson;
    QString clientJar;
};

/** Transactional content conversion. MMC-only control files are never copied. */
class VanillaConverter {
   public:
    static QString gameRootForStore(const QString& storePath);
    static VanillaConversionReport importToPrism(const QString& sourceRoot, const QString& prismTarget, const VanillaImportOptions& options = {});
    static VanillaConversionReport importToVanilla(const QString& sourceRoot, const QString& vanillaTarget, const VanillaImportOptions& options = {});
    static VanillaConversionReport exportToVanilla(const QString& prismRoot, const QString& vanillaTarget, const VanillaExportOptions& options = {});
    static VanillaConversionReport installLoader(const QString& vanillaRoot, const VanillaLoaderInstallOptions& options);
};

