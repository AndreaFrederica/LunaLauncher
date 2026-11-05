// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include "VanillaInstanceLayout.h"

struct VanillaConversionReport {
    bool succeeded = false;
    QStringList blockers;
    QStringList warnings;
    QStringList copied;
    QString error;
    QString destination;

    QJsonObject toJson() const;
};

struct VanillaImportOptions {
    QString name;
    QString group;
    bool copyGameContent = true;
    bool keepSource = true;
};

struct VanillaExportOptions {
    QString versionId;
    QJsonObject versionJson;
    // Optional launcher dialect metadata used by the conversion workflow.
    // Explicit extensionFields are copied into the generated version JSON;
    // Luna does not invent PCL/HMCL-owned top-level fields.
    QString dialect;
    QString name;
    QString group;
    QJsonObject extensionFields;
    bool isolated = false;
    bool copyGameContent = true;
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
    static VanillaConversionReport importToPrism(const QString& sourceRoot, const QString& prismTarget, const VanillaImportOptions& options = {});
    static VanillaConversionReport exportToVanilla(const QString& prismRoot, const QString& vanillaTarget, const VanillaExportOptions& options = {});
    static VanillaConversionReport installLoader(const QString& vanillaRoot, const VanillaLoaderInstallOptions& options);
};

