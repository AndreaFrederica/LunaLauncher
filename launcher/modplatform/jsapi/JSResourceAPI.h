// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#pragma once

#include "modplatform/ResourceAPI.h"

// QuickJS 使用 C99 复合字面量，在 C++ 中是 GNU 扩展
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include "quickjs.h"
#pragma GCC diagnostic pop

#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <memory>

/**
 * API 元数据结构
 * 从 JavaScript api.metadata 中读取
 */
struct ApiMetadata {
    QString id;                      // API 唯一标识符
    QString displayName;             // 显示名称
    QString version;                 // API 版本
    QString provider;                // 资源提供商
    QList<int> supportedTypes;       // 支持的资源类型
    QString icon;                    // 图标名称
    QString description;             // 描述
    QString homepage;                // 主页 URL
    bool enabled = true;             // 是否启用
    int priority = 0;                // 优先级
};

/**
 * JSResourceAPI - 基于 QuickJS 的资源 API 桥接层
 *
 * 允许使用 JavaScript 实现 ResourceAPI 接口，提供：
 * - 动态加载和执行 JS API 实现
 * - C++ 和 JavaScript 之间的数据转换
 * - 网络请求、JSON 处理等辅助功能
 *
 * 使用场景：
 * - 快速开发新的平台 API（Modrinth、CurseForge、Hangar 等）
 * - 测试和原型设计
 * - 社区贡献的第三方平台支持
 */
class JSResourceAPI : public ResourceAPI {
public:
    /**
     * 从 JavaScript 文件创建 API 实例
     * @param jsFilePath JS 文件路径
     * @param apiName API 名称（用于调试和日志）
     * @return API 实例或 nullptr
     */
    static std::shared_ptr<JSResourceAPI> fromFile(const QString& jsFilePath, const QString& apiName);

    /**
     * 从 JavaScript 代码字符串创建 API 实例
     * @param jsCode JavaScript 代码
     * @param apiName API 名称
     * @return API 实例或 nullptr
     */
    static std::shared_ptr<JSResourceAPI> fromString(const QString& jsCode, const QString& apiName);

    ~JSResourceAPI() override;

    // ResourceAPI 接口实现
    Task::Ptr getProjects(QStringList addonIds, std::shared_ptr<QByteArray> response) const override;
    void loadIndexedPack(ModPlatform::IndexedPack& pack, QJsonObject& obj) const override;
    ModPlatform::IndexedVersion loadIndexedPackVersion(QJsonObject& obj, ModPlatform::ResourceType type) const override;
    QJsonArray documentToArray(QJsonDocument& doc) const override;
    void loadExtraPackInfo(ModPlatform::IndexedPack& pack, QJsonObject& obj) const override;
    auto getSortingMethods() const -> QList<ResourceAPI::SortingMethod> override;

    auto getSearchURL(SearchArgs const& args) const -> std::optional<QString> override;
    auto getInfoURL(QString const& id) const -> std::optional<QString> override;
    auto getVersionsURL(VersionSearchArgs const& args) const -> std::optional<QString> override;
    auto getDependencyURL(DependencySearchArgs const& args) const -> std::optional<QString> override;

    // Override ResourceAPI virtual methods for JS API
    QString apiName() const override { return m_metadata.id; }
    QString metaEntryBase() const override { return m_metadata.id + "Plugins"; }

    QString getApiName() const { return m_apiName; }
    const ApiMetadata& getMetadata() const { return m_metadata; }

    /**
     * 获取所有已注册的 JS API
     */
    static QList<std::shared_ptr<JSResourceAPI>> getRegisteredAPIs();

    /**
     * 根据资源类型获取可用的 JS API
     */
    static QList<std::shared_ptr<JSResourceAPI>> getAPIsForResourceType(ModPlatform::ResourceType type);

private:
    JSResourceAPI(const QString& apiName);

    bool initialize(const QString& jsCode);
    void setupJSBindings();
    void cleanup();
    bool loadMetadata();

    // JavaScript 执行辅助函数
    JSValue callJSFunction(const char* funcName, int argc, JSValueConst* argv) const;
    QJsonObject jsValueToJsonObject(JSValue val) const;
    QJsonArray jsValueToJsonArray(JSValue val) const;
    QString jsValueToString(JSValue val) const;
    JSValue jsonObjectToJSValue(const QJsonObject& obj) const;
    JSValue jsonArrayToJSValue(const QJsonArray& arr) const;

    // QuickJS 运行时和上下文
    JSRuntime* m_runtime = nullptr;
    JSContext* m_context = nullptr;
    JSValue m_apiObject;  // JavaScript API 对象实例

    QString m_apiName;
    ApiMetadata m_metadata;
    mutable QString m_lastError;

    // 全局注册表
    static QList<std::shared_ptr<JSResourceAPI>> s_registeredAPIs;
};
