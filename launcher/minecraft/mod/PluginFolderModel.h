// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
*/

#pragma once

#include <QAbstractListModel>
#include <QDir>
#include <QFileSystemWatcher>
#include <QList>
#include <memory>

#include "Plugin.h"
#include "ResourceFolderModel.h"

class BaseInstance;

/**
 * 插件文件夹模型 - 兼容 ResourceFolderModel 接口
 * 但内部使用独立的 Plugin 数据结构
 */
class PluginFolderModel : public ResourceFolderModel {
    Q_OBJECT
public:
    enum Columns {
        EnableColumn = 0,
        ImageColumn,
        NameColumn,
        VersionColumn,
        SizeColumn,
        NUM_COLUMNS
    };

    explicit PluginFolderModel(const QDir& dir, BaseInstance* instance, bool is_indexed = false, bool create_dir = true, QObject* parent = nullptr);

    // ResourceFolderModel 接口重写
    QString id() const override { return "plugins"; }
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    int columnCount(const QModelIndex& parent) const override;
    // 使用基类的 rowCount implementation

    // 插件特定接口
    Plugin::Ptr pluginAt(int index) const;

protected:
    Task* createParseTask(Resource&) override { return nullptr; }
    Resource* createResource(const QFileInfo& info) override { return new Plugin(info); }

    // RESOURCE_HELPERS(Plugin) // 可选，如果需要 helper 方法

private:
    // 移除 m_plugins 和 loadPluginsFromDirectory
};
