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

#include "modplatform/ModIndex.h"
#include "modplatform/ResourceAPI.h"

#include <QDebug>

// HangarAPI - 服务器插件平台 API
class HangarAPI : public ResourceAPI {
public:
    // ResourceAPI 接口实现
    Task::Ptr getProjects(QStringList addonIds, std::shared_ptr<QByteArray> response) const override;
    void loadIndexedPack(ModPlatform::IndexedPack& pack, QJsonObject& obj) const override;
    ModPlatform::IndexedVersion loadIndexedPackVersion(QJsonObject& obj, ModPlatform::ResourceType type = ModPlatform::ResourceType::Plugin) const override;
    QJsonArray documentToArray(QJsonDocument& doc) const override;
    void loadExtraPackInfo(ModPlatform::IndexedPack& pack, QJsonObject& obj) const override;
    auto getSortingMethods() const -> QList<ResourceAPI::SortingMethod> override;

    // Handle Hangar's {pagination, result} response format
    // Note: This shadows the non-virtual base class method
    Task::Ptr getProjectVersions(VersionSearchArgs&& args, Callback<QVector<ModPlatform::IndexedVersion>>&& callbacks) const override;

    static auto getPluginLoaderAsString(ModPlatform::PluginLoaderType loader) -> QString;
    static QStringList getPluginLoaderStrings(ModPlatform::PluginLoaderTypes types);

    // Select the best download platform based on user's plugin loaders and compatibility rules
    QString selectBestPlatform(const QJsonObject& versionObj, ModPlatform::PluginLoaderTypes requestedLoaders) const;

    // Helper to get author URL
    QString getAuthorURL(const QString& author) const;

    static auto validatePluginLoaders(ModPlatform::PluginLoaderTypes loaders) -> bool
    {
        return loaders == 0 || loaders & (ModPlatform::Paper | ModPlatform::Spigot |
                                          ModPlatform::Bukkit | ModPlatform::Purpur |
                                          ModPlatform::Sponge | ModPlatform::Velocity |
                                          ModPlatform::Waterfall | ModPlatform::BungeeCord);
    }

    inline auto getSearchURL(SearchArgs const& args) const -> std::optional<QString> override
    {
        QStringList get_arguments;
        get_arguments.append(QString("offset=%1").arg(args.offset));
        get_arguments.append(QString("limit=25"));

        if (args.search.has_value() && !args.search.value().isEmpty())
            get_arguments.append(QString("q=%1").arg(args.search.value()));

        if (args.sorting.has_value())
            get_arguments.append(QString("sort=%1").arg(args.sorting.value().name));

        // Hangar /projects only supports a single platform filter.
        // If multiple loaders are selected, prefer PAPER (or the first selected loader).
        if (args.pluginLoaders.has_value()) {
            auto loaderStrings = getPluginLoaderStrings(args.pluginLoaders.value());
            if (!loaderStrings.isEmpty()) {
                QString chosen = loaderStrings.contains("paper") ? "paper" : loaderStrings.first();
                get_arguments.append(QString("platform=%1").arg(chosen));
            }
        }

        // Hangar /projects does not support Minecraft version filtering.

        return QString("https://hangar.papermc.io/api/v1/projects?%1").arg(get_arguments.join('&'));
    }

    inline auto getInfoURL(QString const& id) const -> std::optional<QString> override
    {
        return QString("https://hangar.papermc.io/api/v1/projects/%1").arg(id);
    }

    inline auto getVersionsURL(VersionSearchArgs const& args) const -> std::optional<QString> override
    {
        // Hangar /versions endpoint does not accept platform/version filters.
        // Filtering (Minecraft version / platform) must be done client-side.
        QString addonIdStr = args.pack->addonId.value<QString>();
        qDebug() << "[HangarAPI] getVersionsURL - addonId:" << addonIdStr;
        QString url = QString("https://hangar.papermc.io/api/v1/projects/%1/versions").arg(addonIdStr);
        qDebug() << "[HangarAPI] getVersionsURL - Generated URL:" << url;
        return url;
    }

    inline auto getDependencyURL(DependencySearchArgs const& args) const -> std::optional<QString> override
    {
        // Hangar API 的依赖信息包含在版本信息中
        Q_UNUSED(args);
        return {};
    }
};
