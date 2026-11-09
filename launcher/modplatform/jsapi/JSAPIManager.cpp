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
#include "LauncherResources.h"
#include "settings/SettingsObject.h"
#include <algorithm>
#include <QDir>
#include <QDirIterator>

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

    loadConfiguration();

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

    if (getAPIById(api->getMetadata().id)) {
        qDebug() << "[JSAPIManager] Ignoring duplicate API ID" << api->getMetadata().id << "from" << filePath;
        m_loadedFiles.append(filePath);
        return false;
    }

    // 存储 shared_ptr
    m_apis.append(api);
    m_loadedFiles.append(filePath);

    emit apiLoaded(api->getMetadata().id);

    if (APPLICATION->metacache()) {
        const auto registerCache = [](const QString& cacheBase) {
            const QString cachePath = "cache/" + cacheBase;
            APPLICATION->metacache()->addBase(cacheBase, QDir(cachePath).absolutePath());
            qDebug() << "[JSAPIManager] Registered cache base:" << cacheBase << "at" << cachePath;
        };
        if (api->getMetadata().supportedTypes.contains(static_cast<int>(ModPlatform::ResourceType::Mod)))
            registerCache(api->getMetadata().id + "Mods");
        if (api->getMetadata().supportedTypes.contains(static_cast<int>(ModPlatform::ResourceType::Plugin)))
            registerCache(api->getMetadata().id + "Plugins");
    }

    qDebug() << "[JSAPIManager] Successfully loaded:" << api->getMetadata().displayName
             << "(" << api->getMetadata().id << ")";

    return true;
}

QList<std::shared_ptr<JSResourceAPI>> JSAPIManager::getAllAPIs() const
{
    return m_apis;
}

void JSAPIManager::loadConfiguration()
{
    m_orderedIds.clear();
    m_disabledIds.clear();
    if (auto* application = qobject_cast<Application*>(QCoreApplication::instance()); application && application->settings()) {
        m_orderedIds = application->settings()->get("JSAPIOrder").toStringList();
        m_disabledIds = application->settings()->get("JSAPIDisabled").toStringList();
    }
}

bool JSAPIManager::isApiEnabled(const QString& id) const
{
    const auto api = getAPIById(id);
    return api && api->getMetadata().enabled && !m_disabledIds.contains(id);
}

QList<std::shared_ptr<JSResourceAPI>> JSAPIManager::getOrderedAPIs(bool includeDisabled) const
{
    auto result = m_apis;
    std::stable_sort(result.begin(), result.end(), [this](const auto& left, const auto& right) {
        const auto& leftMeta = left->getMetadata();
        const auto& rightMeta = right->getMetadata();
        const int leftRank = m_orderedIds.indexOf(leftMeta.id);
        const int rightRank = m_orderedIds.indexOf(rightMeta.id);
        const bool leftConfigured = leftRank >= 0;
        const bool rightConfigured = rightRank >= 0;
        if (leftConfigured != rightConfigured)
            return leftConfigured;
        if (leftConfigured && leftRank != rightRank)
            return leftRank < rightRank;
        if (leftMeta.priority != rightMeta.priority)
            return leftMeta.priority > rightMeta.priority;
        return leftMeta.id < rightMeta.id;
    });

    if (!includeDisabled) {
        result.erase(std::remove_if(result.begin(), result.end(), [this](const auto& api) { return !isApiEnabled(api->getMetadata().id); }), result.end());
    }
    return result;
}

void JSAPIManager::setConfiguration(const QStringList& order, const QStringList& disabled)
{
    m_orderedIds.clear();
    for (const auto& id : order) {
        if (!id.isEmpty() && !m_orderedIds.contains(id))
            m_orderedIds.append(id);
    }
    m_disabledIds.clear();
    for (const auto& id : disabled) {
        if (!id.isEmpty() && !m_disabledIds.contains(id))
            m_disabledIds.append(id);
    }
    if (auto* application = qobject_cast<Application*>(QCoreApplication::instance()); application && application->settings()) {
        application->settings()->set("JSAPIOrder", m_orderedIds);
        application->settings()->set("JSAPIDisabled", m_disabledIds);
    }
}

QList<std::shared_ptr<JSResourceAPI>> JSAPIManager::getAPIsForType(ModPlatform::ResourceType type) const
{
    QList<std::shared_ptr<JSResourceAPI>> result;
    int typeInt = static_cast<int>(type);

    for (const auto& api : getOrderedAPIs()) {
        const auto& metadata = api->getMetadata();
        if (metadata.enabled && metadata.supportedTypes.contains(typeInt)) {
            result.append(api);
        }
    }

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

    for (const auto& api : getOrderedAPIs()) {
        if (api->getMetadata().provider.compare(provider, Qt::CaseInsensitive) == 0) {
            result.append(api);
        }
    }

    return result;
}

QString JSAPIManager::rewriteUrl(const QString& url, const QJsonObject& context) const
{
    QList<std::shared_ptr<JSResourceAPI>> mirrors;
    for (const auto& api : getOrderedAPIs()) {
        const auto& metadata = api->getMetadata();
        if (metadata.enabled && metadata.kind.compare(QStringLiteral("mirror"), Qt::CaseInsensitive) == 0)
            mirrors.append(api);
    }

    for (const auto& api : mirrors) {
        if (const auto rewritten = api->rewriteUrl(url, context); rewritten.has_value())
            return rewritten.value();
    }
    return url;
}

void JSAPIManager::reloadAll()
{
    qDebug() << "[JSAPIManager] Reloading all APIs...";

    m_apis.clear();
    m_loadedFiles.clear();

    // Re-scan every configured directory so newly copied user APIs and
    // removed or replaced files are reflected by the refresh action.
    initialize(getDefaultAPIDirectories());

    emit apisReloaded();

    qDebug() << "[JSAPIManager] Reloaded" << m_apis.size() << "APIs";
}

QStringList JSAPIManager::getDefaultAPIDirectories()
{
    QStringList dirs;

    dirs.append(LauncherResources::searchPaths(QStringLiteral("jsapi")));

    qDebug() << "[JSAPIManager] API search paths:" << dirs;

    return dirs;
}
