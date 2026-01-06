// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "PluginModel.h"

#include "Application.h"
#include "minecraft/PackProfile.h"
#include "minecraft/mod/PluginFolderModel.h"
#include "server/ServerInstance.h"

namespace ResourceDownload {

PluginModel::PluginModel(BaseInstance& base, ResourceAPI* api, QString debugName, QString metaEntryBase, bool api_owned)
    : ResourceModel(api, api_owned), m_base_instance(base), m_debugName(debugName), m_metaEntryBase(metaEntryBase)
{
}

void PluginModel::searchWithTerm(const QString& term, unsigned int sort, bool filter_changed)
{
    if (m_search_term == term && m_search_term.isNull() == term.isNull() && m_current_sort_index == sort && !filter_changed) {
        return;
    }

    setSearchTerm(term);
    m_current_sort_index = sort;

    refresh();
}

ResourceAPI::SearchArgs PluginModel::createSearchArguments()
{
    ResourceAPI::SearchArgs args;

    args.type = ModPlatform::ResourceType::Plugin;
    args.offset = m_next_search_offset;
    args.search = m_search_term;
    args.sorting = getCurrentSortingMethodByIndex();

    if (m_base_instance.traits().contains("server")) {
        if (auto serverInst = dynamic_cast<ServerInstance*>(&m_base_instance)) {
            auto loaders = serverInst->getPluginLoaderTypes();
            if (loaders != 0)
                args.pluginLoaders = loaders;
            auto mcVer = serverInst->getMinecraftVersion();
            if (!mcVer.isEmpty()) {
                std::list<Version> versions;
                versions.emplace_back(mcVer);
                args.versions = versions;
            }
        }
    }

    return args;
}

ResourceAPI::VersionSearchArgs PluginModel::createVersionsArguments(const QModelIndex& index)
{
    auto& pack = m_packs[index.row()];
    ResourceAPI::VersionSearchArgs args;

    args.pack = pack;
    args.resourceType = ModPlatform::ResourceType::Plugin;

    if (m_base_instance.traits().contains("server")) {
        if (auto serverInst = dynamic_cast<ServerInstance*>(&m_base_instance)) {
            auto loaders = serverInst->getPluginLoaderTypes();
            if (loaders != 0)
                args.pluginLoaders = loaders;
            auto mcVer = serverInst->getMinecraftVersion();
            if (!mcVer.isEmpty()) {
                std::list<Version> versions;
                versions.emplace_back(mcVer);
                args.mcVersions = versions;
            }
        }
    }

    return args;
}

ResourceAPI::ProjectInfoArgs PluginModel::createInfoArguments(const QModelIndex& index)
{
    ResourceAPI::ProjectInfoArgs args;

    args.pack = m_packs[index.row()];

    return args;
}

bool PluginModel::isPackInstalled(ModPlatform::IndexedPack::Ptr pack) const
{
    if (!m_base_instance.traits().contains("server"))
        return false;

    auto serverInst = dynamic_cast<ServerInstance*>(&m_base_instance);
    if (!serverInst)
        return false;

    auto pluginList = serverInst->pluginList();
    if (!pluginList)
        return false;

    for (int i = 0; i < pluginList->size(); i++) {
        auto plugin = pluginList->pluginAt(i);
        if (!plugin)
            continue;

        if (QString::compare(plugin->name(), pack->name, Qt::CaseInsensitive) == 0)
            return true;
        if (!pack->slug.isEmpty() && QString::compare(plugin->name(), pack->slug, Qt::CaseInsensitive) == 0)
            return true;
    }

    return false;
}

QVariant PluginModel::getInstalledPackVersion(ModPlatform::IndexedPack::Ptr pack) const
{
    if (!m_base_instance.traits().contains("server"))
        return {};

    auto serverInst = dynamic_cast<ServerInstance*>(&m_base_instance);
    if (!serverInst)
        return {};

    auto pluginList = serverInst->pluginList();
    if (!pluginList)
        return {};

    for (int i = 0; i < pluginList->size(); i++) {
        auto plugin = pluginList->pluginAt(i);
        if (!plugin)
            continue;

        bool matches = (QString::compare(plugin->name(), pack->name, Qt::CaseInsensitive) == 0);
        if (!matches && !pack->slug.isEmpty())
            matches = (QString::compare(plugin->name(), pack->slug, Qt::CaseInsensitive) == 0);

        if (matches)
            return plugin->version();
    }

    return {};
}

bool PluginModel::checkVersionFilters(const ModPlatform::IndexedVersion& v)
{
    if (m_base_instance.traits().contains("server")) {
        if (auto serverInst = dynamic_cast<ServerInstance*>(&m_base_instance)) {
            auto loaders = serverInst->getPluginLoaderTypes();
            if (loaders != 0) {
                if (!(v.pluginLoaders & loaders))
                    return false;
            }
            auto mcVer = serverInst->getMinecraftVersion();
            if (!mcVer.isEmpty()) {
                auto mm = mcVer.split('.');
                QString prefix = mcVer;
                if (mm.size() >= 2) {
                    prefix = mm[0] + "." + mm[1];
                }
                bool match = false;
                for (const auto& verStr : v.mcVersion) {
                    if (verStr == mcVer || verStr.startsWith(prefix))
                        { match = true; break; }
                }
                if (!match)
                    return false;
            }
        }
    }
    return !optedOut(v);
}

}  // namespace ResourceDownload
