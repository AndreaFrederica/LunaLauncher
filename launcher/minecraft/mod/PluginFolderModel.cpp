// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica<andreafrederica@outlook.com>
*/

#include "PluginFolderModel.h"
#include "FileSystem.h"

#include <QDebug>
#include <QFileInfo>

PluginFolderModel::PluginFolderModel(const QDir& dir, BaseInstance* instance, bool is_indexed, bool create_dir, QObject* parent)
    : ResourceFolderModel(dir, instance, is_indexed, create_dir, parent)
{
    // 配置目录过滤器
    m_dir.setFilter(QDir::Readable | QDir::NoDotAndDotDot | QDir::Files);
    m_dir.setNameFilters({"*.jar", "*.jar.disabled"});
    m_dir.setSorting(QDir::Name | QDir::IgnoreCase);

    // 触发初始加载
    update();
}

int PluginFolderModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return NUM_COLUMNS;
}

QVariant PluginFolderModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_resources.size())
        return QVariant();

    auto resource = m_resources[index.row()];
    auto plugin = resource.dynamicCast<Plugin>();
    if (!plugin)
        return QVariant();

    int column = index.column();

    if (role == Qt::DisplayRole) {
        switch (column) {
            case NameColumn:
                return plugin->name();
            case VersionColumn:
                return plugin->version();
            case SizeColumn:
                return plugin->sizeStr();
            default:
                return QVariant();
        }
    } else if (role == Qt::CheckStateRole) {
        if (column == EnableColumn) {
            return plugin->enabled() ? Qt::Checked : Qt::Unchecked;
        }
    } else if (role == Qt::ToolTipRole) {
        QStringList tooltip;
        tooltip << tr("Name: %1").arg(plugin->name());
        if (!plugin->version().isEmpty() && plugin->version() != "Unknown")
            tooltip << tr("Version: %1").arg(plugin->version());
        if (!plugin->description().isEmpty())
            tooltip << tr("Description: %1").arg(plugin->description());
        tooltip << tr("File: %1").arg(plugin->fileinfo().fileName());
        tooltip << tr("Size: %1").arg(plugin->sizeStr());
        return tooltip.join("\n");
    }

    return QVariant();
}

QVariant PluginFolderModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
        return QVariant();

    switch (section) {
        case EnableColumn:
            return QString();
        case ImageColumn:
            return QString();
        case NameColumn:
            return tr("Name");
        case VersionColumn:
            return tr("Version");
        case SizeColumn:
            return tr("Size");
        default:
            return QVariant();
    }
}

Plugin::Ptr PluginFolderModel::pluginAt(int index) const
{
    if (index < 0 || index >= m_resources.size())
        return nullptr;
    return shared_qobject_ptr<Plugin>(m_resources[index].dynamicCast<Plugin>());
}
