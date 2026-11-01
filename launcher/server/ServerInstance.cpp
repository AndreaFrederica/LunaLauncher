/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
*/
#include "ServerInstance.h"
#include "ServerLaunchTask.h"
#include "settings/INISettingsObject.h"
#include "FileSystem.h"
#include "minecraft/mod/ModFolderModel.h"
#include "minecraft/mod/PluginFolderModel.h"
#include "Application.h"
#include "modplatform/ModIndex.h"

ServerInstance::ServerInstance(SettingsObject* globalSettings, std::unique_ptr<SettingsObject> settings, const QString& rootDir)
    : BaseInstance(globalSettings, std::move(settings), rootDir)
{
    // Define settings with defaults
    m_settings->registerSetting("ExecutablePath", "");
    m_settings->registerSetting("Arguments", QStringList());
    m_settings->registerSetting("WorkingDir", "");
    // A newline-terminated command sent to the server console for graceful
    // shutdown. An empty value disables graceful shutdown.
    m_settings->registerSetting("StopCommand", "stop");
    // Server-side authlib-injector configuration. The jar is shared with the
    // normal Minecraft launcher cache, while the API URL is per server.
    m_settings->registerSetting("AuthlibInjectorEnabled", false);
    m_settings->registerSetting("AuthlibInjectorApiUrl", "");

    // Java Settings
    auto locationOverride = m_settings->registerSetting("OverrideJavaLocation", false);
    m_settings->registerSetting("JavaPath", "");
    m_settings->registerSetting("IgnoreJavaCompatibility", false);

    // Memory Settings
    auto memoryOverride = m_settings->registerSetting("OverrideMemory", false);
    m_settings->registerSetting("MinMemAlloc", 512);
    m_settings->registerSetting("MaxMemAlloc", 4096);
    m_settings->registerSetting("PermGen", 128);

    // Java Arguments (needed by JavaSettingsWidget)
    m_settings->registerSetting("OverrideJavaArgs", false);
    m_settings->registerSetting("JvmArgs", "");

    // ModLoader configuration for server
    m_settings->registerSetting("ServerModLoaderType", 0);  // ModPlatform::ModLoaderTypes as int
    m_settings->registerSetting("ServerMinecraftVersion", "");

    // PluginLoader configuration for server (for future plugin download system)
    m_settings->registerSetting("ServerPluginLoaderType", 0);  // ModPlatform::PluginLoaderTypes as int
}

ServerInstance::~ServerInstance()
{
    qDebug() << "ServerInstance::~ServerInstance() - Destroying instance";
    // Ensure server is stopped and task is released
    if (!stopServer(false))
        stopServer(true);
}

void ServerInstance::saveNow()
{
    // Settings are auto-saved
}

QString ServerInstance::id() const
{
    return QFileInfo(instanceRoot()).fileName();
}

LaunchTask* ServerInstance::createLaunchTask(AuthSessionPtr account, MinecraftTarget::Ptr targetToJoin)
{
    Q_UNUSED(account)
    Q_UNUSED(targetToJoin)

    m_launchProcess = std::make_unique<ServerLaunchTask>(this);
    emit launchTaskChanged(m_launchProcess.get());
    return m_launchProcess.get();
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

QString ServerInstance::stopCommand() const
{
    return m_settings->get("StopCommand").toString();
}

bool ServerInstance::authlibInjectorEnabled() const
{
    return m_settings->get("AuthlibInjectorEnabled").toBool();
}

QString ServerInstance::authlibInjectorApiUrl() const
{
    return m_settings->get("AuthlibInjectorApiUrl").toString();
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

void ServerInstance::setStopCommand(const QString &command)
{
    m_settings->set("StopCommand", command);
}

void ServerInstance::setAuthlibInjectorEnabled(bool enabled)
{
    m_settings->set("AuthlibInjectorEnabled", enabled);
}

void ServerInstance::setAuthlibInjectorApiUrl(const QString &url)
{
    m_settings->set("AuthlibInjectorApiUrl", url);
}

bool ServerInstance::startServer()
{
    qDebug() << "ServerInstance::startServer() - Creating new task";

    // A second start must never leave the previous process behind. Use the
    // explicit force path here after any configured graceful stop has had a
    // chance to run through stopServer(false).
    if (isRunning() && !stopServer(false) && !stopServer(true))
        return false;

    // Create and start new task
    auto task = createLaunchTask({}, {});
    auto serverTask = dynamic_cast<ServerLaunchTask*>(task);

    if (!serverTask) {
        qWarning() << "ServerInstance::startServer() - Failed to cast to ServerLaunchTask";
        return false;
    }

    // Start the server
    serverTask->start();

    return isRunning();
}

bool ServerInstance::stopServer(bool force)
{
    qDebug() << "ServerInstance::stopServer() - Stopping server";

    if (!isRunning())
        return true;

    auto serverTask = dynamic_cast<ServerLaunchTask*>(m_launchProcess.get());
    const bool stopped = serverTask && serverTask->stopServer(force) && !isRunning();

    qDebug() << "ServerInstance::stopServer() - Server stopped";
    return stopped;
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

std::shared_ptr<PluginFolderModel> ServerInstance::pluginList() const
{
    if (!m_pluginList)
    {
        bool is_indexed = !APPLICATION->settings()->get("ModMetadataDisabled").toBool();
        m_pluginList.reset(new PluginFolderModel(QDir(FS::PathCombine(instanceRoot(), "plugins")), const_cast<ServerInstance*>(this), is_indexed, true));
    }
    return m_pluginList;
}

ModPlatform::ModLoaderTypes ServerInstance::getModLoaderTypes() const
{
    return static_cast<ModPlatform::ModLoaderTypes>(m_settings->get("ServerModLoaderType").toInt());
}

void ServerInstance::setModLoaderTypes(ModPlatform::ModLoaderTypes types)
{
    m_settings->set("ServerModLoaderType", static_cast<int>(types));
}

QString ServerInstance::getMinecraftVersion() const
{
    return m_settings->get("ServerMinecraftVersion").toString();
}

void ServerInstance::setMinecraftVersion(const QString &version)
{
    m_settings->set("ServerMinecraftVersion", version);
}

ModPlatform::PluginLoaderTypes ServerInstance::getPluginLoaderTypes() const
{
    return static_cast<ModPlatform::PluginLoaderTypes>(m_settings->get("ServerPluginLoaderType").toInt());
}

void ServerInstance::setPluginLoaderTypes(ModPlatform::PluginLoaderTypes types)
{
    m_settings->set("ServerPluginLoaderType", static_cast<int>(types));
}
