// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

#include "VanillaInstanceLayout.h"
#include "ExternalInstanceProvenance.h"

enum class VanillaDialect {
    Unknown,
    Mojang,
    PCL,
    HMCL,
};

struct VanillaInstanceDescriptor {
    QString stableId;
    QString storeId;
    QString root;
    QString name;
    QString iconKey;
    QString notes;
    QString group;
    QString dialect;
    ExternalInstanceProvenance provenance;
    // The profile id is the directory/file selected by the foreign launcher.
    // minecraftVersion is the actual base game version (the `jar`/fml value),
    // while loaderType/loaderVersion describe the detected loader profile.
    QString versionId;
    QString profileVersionId;
    QString minecraftVersion;
    QString loaderType;
    QString loaderVersion;
    // Retains the standard loader identity after an interoperable disable
    // removes the loader patch from the foreign version JSON.
    QString loaderTypeOverride;
    bool loaderEnabled = true;
    QString loaderVersionOverride;
    QJsonObject loaderProfileOverride;
    QString loaderManifestFingerprint;
    VanillaInstanceLayout layout;
    QJsonObject version;
    bool writable = false;

    bool isValid() const { return !stableId.isEmpty() && layout.isValid(); }
};

/**
 * Reads PCL, HMCL and official launcher layouts. Luna settings and the
 * component graph are stored in that instance's directory so it remains
 * portable between Luna installations.
 */
class VanillaCompatibleProvider {
   public:
    static QList<VanillaInstanceDescriptor> scan(const QString& storeId, const QString& storeRoot);
    static bool loadSidecar(VanillaInstanceDescriptor& descriptor, QString* error = nullptr);
    static bool saveSidecar(const VanillaInstanceDescriptor& descriptor, QString* error = nullptr);
    static QString stableId(const QString& storeId, const QString& root, const QString& versionId);
    static QString dialectName(const QJsonObject& version, const QString& root);
    static ExternalInstanceProvenance detectProvenance(const QJsonObject& version,
                                                       const QString& root,
                                                       const VanillaInstanceLayout& layout = {});
    static QString detectedLoaderType(const QJsonObject& version);
    static QString detectedLoaderVersion(const QJsonObject& version);
    static QString detectedMinecraftVersion(const QJsonObject& version, const QString& profileId = {});
    // Shared HMCL/PCL writer used by both external-instance editing and new
    // loader-profile installation. Keeping this in the provider prevents the
    // two entry points from producing subtly different version JSON.
    static QString loaderUidForType(const QString& loaderType);
    static QString loaderTypeForUid(const QString& loaderUid);
    static QJsonObject canonicalLoaderPatch(QJsonObject profile,
                                            const QString& loaderType,
                                            const QString& version);
    static QJsonObject makeInheritedLoaderProfile(const QString& baseVersionId,
                                                  const QString& loaderType,
                                                  const QString& version,
                                                  QJsonObject profile,
                                                  const QString& clientJar = {});
    static bool writeLoaderManifest(const QString& versionJsonPath,
                                    const QString& loaderType,
                                    const QString& version,
                                    const QJsonObject& loaderProfile,
                                    const QJsonObject& baseProfile = {},
                                    bool enabled = true,
                                    QString* error = nullptr);
    static bool isStandardLoaderProfile(const QJsonObject& profile,
                                        const QString& loaderType,
                                        const QJsonObject& loaderPatch);
};

