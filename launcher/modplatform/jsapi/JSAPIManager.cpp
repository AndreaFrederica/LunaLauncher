// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
 */

// QuickJS 使用 C99 复合字面量，在 C++ 中会产生 pedantic 警告
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

#include "JSAPIManager.h"

#pragma GCC diagnostic pop

#include "net/HttpMetaCache.h"
#include "Application.h"
#include "BuildConfig.h"
#include <QDir>
#include <QDirIterator>
#include <QStandardPaths>

JSAPIManager& JSAPIManager::instance()
{
    static JSAPIManager instance;
    return instance;
}

void JSAPIManager::initialize(const QStringList& apiDirectories)
{
    qDebug() << "[JSAPIManager] Initializing...";

    QStringList dirs = apiDirectories.isEmpty() ? getDefaultAPIDirectories() : apiDirectories;

    for (const QString& dir : dirs) {
        // qrc 路径需要特殊处理
        if (dir.startsWith(":/")) {
            qDebug() << "[JSAPIManager] Scanning qrc directory:" << dir;
            loadAPIsFromDirectory(dir);
        } else if (QDir(dir).exists()) {
            qDebug() << "[JSAPIManager] Scanning directory:" << dir;
            loadAPIsFromDirectory(dir);
        } else {
            qDebug() << "[JSAPIManager] Directory not found:" << dir;
        }
    }

    qDebug() << "[JSAPIManager] Loaded" << m_apis.size() << "APIs";
}

void JSAPIManager::loadAPIsFromDirectory(const QString& directory)
{
    qDebug() << "[JSAPIManager] Scanning directory:" << directory;

    // 检查是否是 qrc 路径
    if (directory.startsWith(":/")) {
        // Qt 资源系统路径
        QDirIterator it(directory, QStringList() << "*.js", QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QString filePath = it.next();
            qDebug() << "[JSAPIManager] Found qrc file:" << filePath;
            loadAPIFromFile(filePath);
        }
    } else {
        // 文件系统路径
        QDirIterator it(directory, QStringList() << "*.js", QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QString filePath = it.next();
            loadAPIFromFile(filePath);
        }
    }
}

bool JSAPIManager::loadAPIFromFile(const QString& filePath)
{
    if (m_loadedFiles.contains(filePath)) {
        qDebug() << "[JSAPIManager] File already loaded:" << filePath;
        return false;
    }

    QFileInfo fileInfo(filePath);
    QString apiName = fileInfo.baseName();

    qDebug() << "[JSAPIManager] Loading API from:" << filePath;

    auto api = JSResourceAPI::fromFile(filePath, apiName);
    if (!api) {
        qWarning() << "[JSAPIManager] Failed to load API from:" << filePath;
        return false;
    }

    // 存储 shared_ptr
    m_apis.append(api);
    m_loadedFiles.append(filePath);

    emit apiLoaded(api->getMetadata().id);

    // Register cache base for this API
    // The base name convention is ID + "Plugins" (or other types, but currently we use Plugins)
    // See JSApiPluginPage::metaEntryBase()
    QString cacheBase = api->getMetadata().id + "Plugins";
    QString cachePath = "cache/" + cacheBase;

    // We need to access the global HttpMetaCache instance
    // Since JSAPIManager is a singleton, we can assume Application is initialized
    if (APPLICATION->metacache()) {
        APPLICATION->metacache()->addBase(cacheBase, QDir(cachePath).absolutePath());
        qDebug() << "[JSAPIManager] Registered cache base:" << cacheBase << "at" << cachePath;
    }

    qDebug() << "[JSAPIManager] Successfully loaded:" << api->getMetadata().displayName
             << "(" << api->getMetadata().id << ")";

    return true;
}

QList<std::shared_ptr<JSResourceAPI>> JSAPIManager::getAllAPIs() const
{
    return m_apis;
}

QList<std::shared_ptr<JSResourceAPI>> JSAPIManager::getAPIsForType(ModPlatform::ResourceType type) const
{
    QList<std::shared_ptr<JSResourceAPI>> result;
    int typeInt = static_cast<int>(type);

    for (const auto& api : m_apis) {
        const auto& metadata = api->getMetadata();
        if (metadata.enabled && metadata.supportedTypes.contains(typeInt)) {
            result.append(api);
        }
    }

    // 按优先级排序
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        return a->getMetadata().priority > b->getMetadata().priority;
    });

    return result;
}

std::shared_ptr<JSResourceAPI> JSAPIManager::getAPIById(const QString& id) const
{
    for (const auto& api : m_apis) {
        if (api->getMetadata().id == id) {
            return api;
        }
    }
    return nullptr;
}

QList<std::shared_ptr<JSResourceAPI>> JSAPIManager::getAPIsByProvider(const QString& provider) const
{
    QList<std::shared_ptr<JSResourceAPI>> result;

    for (const auto& api : m_apis) {
        if (api->getMetadata().provider.compare(provider, Qt::CaseInsensitive) == 0) {
            result.append(api);
        }
    }

    return result;
}

void JSAPIManager::reloadAll()
{
    qDebug() << "[JSAPIManager] Reloading all APIs...";

    m_apis.clear();
    QStringList files = m_loadedFiles;
    m_loadedFiles.clear();

    for (const QString& file : files) {
        loadAPIFromFile(file);
    }

    emit apisReloaded();

    qDebug() << "[JSAPIManager] Reloaded" << m_apis.size() << "APIs";
}

QStringList JSAPIManager::getDefaultAPIDirectories()
{
    QStringList dirs;

    // 1. 内置 API 目录（Qt 资源文件，编译进程序）
    //    优先级最高，始终可用
    dirs << ":/jsapi";

    // 2. 安装目录（随程序一起分发）
    //    Windows: <exe_dir>/resource/jsapi
    //    Linux: /usr/share/lunalauncher/resources/jsapi
    //    macOS: LunaLauncher.app/Contents/Resources/jsapi
#ifdef Q_OS_WIN
    dirs << APPLICATION->applicationDirPath() + "/resource/jsapi";
#elif defined(Q_OS_MAC)
    dirs << APPLICATION->applicationDirPath() + "/../Resources/jsapi";
#else
    // Linux/Unix
    dirs << APPLICATION->applicationDirPath() + "/../share/" + BuildConfig.LAUNCHER_APP_BINARY_NAME + "/resources/jsapi";
    dirs << "/usr/share/" + BuildConfig.LAUNCHER_APP_BINARY_NAME + "/resources/jsapi";
    dirs << "/usr/local/share/" + BuildConfig.LAUNCHER_APP_BINARY_NAME + "/resources/jsapi";
#endif

    // 3. 用户数据目录（用户自定义 API，可热更新）
    //    Windows: %APPDATA%/LunaLauncher/jsapi
    //    Linux: ~/.local/share/lunalauncher/jsapi
    //    macOS: ~/Library/Application Support/LunaLauncher/jsapi
    QString userDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    dirs << userDataDir + "/jsapi";

    // 4. 开发目录（开发时使用）
    dirs << APPLICATION->applicationDirPath() + "/jsapi";
    dirs << QDir::currentPath() + "/jsapi";

    qDebug() << "[JSAPIManager] API search paths:" << dirs;

    return dirs;
}
