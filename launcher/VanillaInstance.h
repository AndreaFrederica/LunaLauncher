// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "minecraft/MinecraftInstance.h"
#include "VanillaCompatibleProvider.h"

/** Minecraft instance adapter for a vanilla/PCL/HMCL directory.
 *
 * Luna settings and Prism component files are stored inside the instance
 * root, alongside the foreign launcher's ordinary game content.
 */
class VanillaInstance final : public MinecraftInstance {
    Q_OBJECT
   public:
    VanillaInstance(SettingsObject* globalSettings,
                    std::unique_ptr<SettingsObject> settings,
                    const VanillaInstanceDescriptor& descriptor);
    QString id() const override { return m_descriptor.stableId; }
    QString gameRoot() const override { return m_descriptor.layout.gameRoot; }
    QString componentRoot() const override;
    std::shared_ptr<VersionFile> loadForeignComponent(const QString& uid, const QString& version) const override;
    QString getLocalLibraryPath() const override { return m_descriptor.layout.librariesRoot; }
    std::optional<MinecraftClientRoute> clientArtifactRoute() const override;
    QString instanceConfigFolder() const override { return QDir(gameRoot()).filePath("config"); }
    // Editing is allowed for Luna settings and ordinary game content. Prism
    // component editing is deliberately not exposed for this adapter; it
    // requires an explicit conversion to a Prism instance.
    bool canEdit() const override { return true; }
    bool canExport() const override { return true; }
    bool canLaunch() const override;
    /**
     * Copy the loader's mavenFiles from Luna's resolved cache into the
     * external instance's libraries directory. HMCL/PCL ForgeWrapper needs
     * the Forge installer beside the foreign profile and cannot see Luna's
     * global cache.
     */
    bool materializeForeignMavenFiles(QString* error = nullptr) const;
    bool isForeignLoaderEnabled() const { return m_descriptor.loaderEnabled; }
    bool setForeignLoaderEnabled(bool enabled, QString* error = nullptr);
    bool setForeignLoaderProfile(const QString& version, const QJsonObject& profile, QString* error = nullptr);
    // Flush the generated Prism component profile in the external instance
    // directory so the instance remains portable between Luna installations.
    void saveNow() override { MinecraftInstance::saveNow(); }
    QString typeName() const override { return QStringLiteral("Vanilla"); }
    QString getStatusbarDescription() override
    {
        const auto mc = m_descriptor.minecraftVersion.isEmpty() ? m_descriptor.versionId : m_descriptor.minecraftVersion;
        const auto loaderVersion = m_descriptor.loaderVersionOverride.isEmpty() ? m_descriptor.loaderVersion : m_descriptor.loaderVersionOverride;
        const auto loader = loaderVersion.isEmpty() || m_descriptor.loaderType == QStringLiteral("vanilla")
            ? tr("Vanilla")
            : tr("%1 %2").arg(m_descriptor.loaderType.toUpper(), loaderVersion);
        return tr("%1 %2 (%3)").arg(mc, loader, m_descriptor.dialect);
    }
    QSet<QString> traits() const override { return { "vanilla-compatible" }; }
    const VanillaInstanceDescriptor& descriptor() const { return m_descriptor; }

   private:
    void ensureRuntimeFiles() const;
    QString standardLoaderUid() const;
    QJsonObject sourceMinecraftVersion() const;
    QJsonObject sourceLoaderProfile() const;
    QString foreignLoaderManifestFingerprint() const;
    VanillaInstanceDescriptor m_descriptor;
    mutable bool m_runtimeReady = false;
};

