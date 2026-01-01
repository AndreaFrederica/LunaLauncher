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
    shared_qobject_ptr<ServerLaunchTask> task(new ServerLaunchTask(this));
    setLaunchTask(task);
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

void ServerInstance::setLaunchTask(Task::Ptr task)
{
    m_launchTask = task;
    emit runningStatusChanged(isRunning());

    connect(task.data(), &Task::finished, this, [this]() {
        emit runningStatusChanged(isRunning());
    });
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
