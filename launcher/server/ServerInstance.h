/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
*/
#pragma once
#include "BaseInstance.h"

class ServerInstance : public BaseInstance
{
    Q_OBJECT
public:
    explicit ServerInstance(SettingsObjectPtr globalSettings, SettingsObjectPtr settings, const QString &rootDir);
    virtual ~ServerInstance();

    void saveNow() override;
    QString id() const override;

    // Core Interface
    shared_qobject_ptr<LaunchTask> createLaunchTask(AuthSessionPtr account, MinecraftTarget::Ptr targetToJoin) override;

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

    Task::Ptr launchTask() const { return m_launchTask; }

    std::shared_ptr<class ModFolderModel> loaderModList() const;
    std::shared_ptr<class ModFolderModel> pluginList() const;

private:
    Task::Ptr m_launchTask;
    mutable std::shared_ptr<class ModFolderModel> m_loaderModList;
    mutable std::shared_ptr<class ModFolderModel> m_pluginList;
};
