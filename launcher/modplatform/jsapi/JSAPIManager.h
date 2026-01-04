// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
 */

#pragma once

#include "JSResourceAPI.h"
#include "modplatform/ModIndex.h"
#include <QObject>
#include <QString>
#include <QStringList>
#include <memory>

/**
 * JS API 管理器
 *
 * 负责：
 * - 扫描和加载 JS API 文件
 * - 管理已注册的 API
 * - 提供 API 查询和选择功能
 */
class JSAPIManager : public QObject {
    Q_OBJECT

public:
    static JSAPIManager& instance();

    /**
     * 初始化管理器，扫描并加载所有 JS API
     * @param apiDirectories API 文件目录列表
     */
    void initialize(const QStringList& apiDirectories = {});

    /**
     * 从目录加载所有 JS API
     */
    void loadAPIsFromDirectory(const QString& directory);

    /**
     * 从文件加载单个 JS API
     */
    bool loadAPIFromFile(const QString& filePath);

    /**
     * 获取所有已注册的 API
     */
    QList<std::shared_ptr<JSResourceAPI>> getAllAPIs() const;

    /**
     * 根据资源类型获取可用的 API
     */
    QList<std::shared_ptr<JSResourceAPI>> getAPIsForType(ModPlatform::ResourceType type) const;

    /**
     * 根据 ID 获取 API
     */
    std::shared_ptr<JSResourceAPI> getAPIById(const QString& id) const;

    /**
     * 根据提供商获取 API
     */
    QList<std::shared_ptr<JSResourceAPI>> getAPIsByProvider(const QString& provider) const;

    /**
     * 重新加载所有 API
     */
    void reloadAll();

    /**
     * 获取默认 API 目录
     */
    static QStringList getDefaultAPIDirectories();

signals:
    void apiLoaded(const QString& apiId);
    void apiUnloaded(const QString& apiId);
    void apisReloaded();

private:
    JSAPIManager() = default;
    ~JSAPIManager() = default;

    JSAPIManager(const JSAPIManager&) = delete;
    JSAPIManager& operator=(const JSAPIManager&) = delete;

    QList<std::shared_ptr<JSResourceAPI>> m_apis;
    QStringList m_loadedFiles;
};
