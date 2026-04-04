/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
*/
#pragma once
#include "BaseInstance.h"
#include "modplatform/ModIndex.h"

class ServerInstance : public BaseInstance
{
    Q_OBJECT
public:
    explicit ServerInstance(SettingsObject* globalSettings, std::unique_ptr<SettingsObject> settings, const QString& rootDir);
    virtual ~ServerInstance();

    void saveNow() override;
    QString id() const override;

    // Core Interface
    LaunchTask* createLaunchTask(AuthSessionPtr account, MinecraftTarget::Ptr targetToJoin) override;

    // Unused/Stubbed
    QList<Task::Ptr> createUpdateTask() override { return {}; }
    QString getStatusbarDescription() override { return "Server Instance"; }
    QSet<QString> traits() const override { return {"server"}; }

    QString modsRoot() const override { return FS::PathCombine(instanceRoot(), "mods"); }
    void loadSpecificSettings() override {}
    QProcessEnvironment createEnvironment() override { return QProcessEnvironment::systemEnvironment(); }
    QProcessEnvironment createLaunchEnvironment() override { return QProcessEnvironment::systemEnvironment(); }
    QStringList getLogFileSearchPaths() override { return { FS::PathCombine(instanceRoot(), "logs"), instanceRoot() }; }
    QString instanceConfigFolder() const override { return instanceRoot(); }
    QMap<QString, QString> getVariables() override { return {}; }
    QString typeName() const override { return "Server"; }
    bool canEdit() const override { return true; }
    bool canExport() const override { return true; }
    void populateLaunchMenu(QMenu* menu) override {}
    QStringList verboseDescription(AuthSessionPtr session, MinecraftTarget::Ptr targetToJoin) override { return {"Server Instance"}; }

    // Properties
    QString executablePath() const;
    QStringList arguments() const;
    QString workingDir() const;

    void setExecutablePath(const QString &path);
    void setArguments(const QStringList &args);
    void setWorkingDir(const QString &dir);

    // Server lifecycle management
    bool startServer();
    void stopServer();

    LaunchTask* launchTask() { return getLaunchTask(); }

    std::shared_ptr<class ModFolderModel> loaderModList() const;
    std::shared_ptr<class PluginFolderModel> pluginList() const;

    // ModLoader configuration
    ModPlatform::ModLoaderTypes getModLoaderTypes() const;
    void setModLoaderTypes(ModPlatform::ModLoaderTypes types);
    QString getMinecraftVersion() const;
    void setMinecraftVersion(const QString &version);

    // PluginLoader configuration (for server plugins like Paper, Spigot, etc.)
    ModPlatform::PluginLoaderTypes getPluginLoaderTypes() const;
    void setPluginLoaderTypes(ModPlatform::PluginLoaderTypes types);

private:
    mutable std::shared_ptr<class ModFolderModel> m_loaderModList;
    mutable std::shared_ptr<class PluginFolderModel> m_pluginList;
};
