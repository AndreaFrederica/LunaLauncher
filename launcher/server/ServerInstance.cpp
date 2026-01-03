/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
*/
#include "ServerInstance.h"
#include "ServerLaunchTask.h"
#include "settings/INISettingsObject.h"
#include "FileSystem.h"
#include "minecraft/mod/ModFolderModel.h"
#include "Application.h"

ServerInstance::ServerInstance(SettingsObjectPtr globalSettings, SettingsObjectPtr settings, const QString &rootDir)
    : BaseInstance(globalSettings, settings, rootDir)
{
    // Define settings with defaults
    settings->registerSetting("ExecutablePath", "");
    settings->registerSetting("Arguments", QStringList());
    settings->registerSetting("WorkingDir", "");

    // Java Settings
    auto locationOverride = settings->registerSetting("OverrideJavaLocation", false);
    settings->registerSetting("JavaPath", "");
    settings->registerSetting("IgnoreJavaCompatibility", false);

    // Memory Settings
    auto memoryOverride = settings->registerSetting("OverrideMemory", false);
    settings->registerSetting("MinMemAlloc", 512);
    settings->registerSetting("MaxMemAlloc", 4096);
    settings->registerSetting("PermGen", 128);

    // Java Arguments (needed by JavaSettingsWidget)
    settings->registerSetting("OverrideJavaArgs", false);
    settings->registerSetting("JvmArgs", "");
}

ServerInstance::~ServerInstance()
{
    qDebug() << "ServerInstance::~ServerInstance() - Destroying instance";
    // Ensure server is stopped and task is released
    stopServer();
}

void ServerInstance::saveNow()
{
    // Settings are auto-saved
}

QString ServerInstance::id() const
{
    return QFileInfo(instanceRoot()).fileName();
}

shared_qobject_ptr<LaunchTask> ServerInstance::createLaunchTask(AuthSessionPtr account, MinecraftTarget::Ptr targetToJoin)
{
    Q_UNUSED(account)
    Q_UNUSED(targetToJoin)

    // Always create a fresh task
    m_launchTask.reset();
    shared_qobject_ptr<ServerLaunchTask> task(new ServerLaunchTask(this));
    m_launchTask = task;
    return task;
}

QString ServerInstance::executablePath() const
{
    return m_settings->get("ExecutablePath").toString();
}

QStringList ServerInstance::arguments() const
{
    return m_settings->get("Arguments").toStringList();
}

QString ServerInstance::workingDir() const
{
    return m_settings->get("WorkingDir").toString();
}

void ServerInstance::setExecutablePath(const QString &path)
{
    m_settings->set("ExecutablePath", path);
}

void ServerInstance::setArguments(const QStringList &args)
{
    m_settings->set("Arguments", args);
}

void ServerInstance::setWorkingDir(const QString &dir)
{
    m_settings->set("WorkingDir", dir);
}

bool ServerInstance::startServer()
{
    qDebug() << "ServerInstance::startServer() - Creating new task";

    // Clean up any existing task first
    stopServer();

    // Create and start new task
    auto task = createLaunchTask({}, {});
    auto serverTask = task.dynamicCast<ServerLaunchTask>();

    if (!serverTask) {
        qWarning() << "ServerInstance::startServer() - Failed to cast to ServerLaunchTask";
        return false;
    }

    // Start the server
    serverTask->start();

    return isRunning();
}

void ServerInstance::stopServer()
{
    qDebug() << "ServerInstance::stopServer() - Stopping server";

    // Stop the current task if running
    if (m_launchTask) {
        auto serverTask = m_launchTask.dynamicCast<ServerLaunchTask>();
        if (serverTask && serverTask->canStop()) {
            serverTask->stop();
        }
    }

    qDebug() << "ServerInstance::stopServer() - Server stopped";
}



std::shared_ptr<ModFolderModel> ServerInstance::loaderModList() const
{
    if (!m_loaderModList)
    {
        bool is_indexed = !APPLICATION->settings()->get("ModMetadataDisabled").toBool();
        m_loaderModList.reset(new ModFolderModel(QDir(FS::PathCombine(instanceRoot(), "mods")), const_cast<ServerInstance*>(this), is_indexed, true));
    }
    return m_loaderModList;
}

std::shared_ptr<ModFolderModel> ServerInstance::pluginList() const
{
    if (!m_pluginList)
    {
        bool is_indexed = !APPLICATION->settings()->get("ModMetadataDisabled").toBool();
        m_pluginList.reset(new ModFolderModel(QDir(FS::PathCombine(instanceRoot(), "plugins")), const_cast<ServerInstance*>(this), is_indexed, true));
    }
    return m_pluginList;
}
