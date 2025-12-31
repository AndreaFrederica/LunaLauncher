/*
 *  Luna Launcher - Minecraft Launcher 
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
 */
#include "SchematicsFolderModel.h"
#include <QIcon>

SchematicsFolderModel::SchematicsFolderModel(const QDir& dir, BaseInstance* instance, QObject* parent)
    : ResourceFolderModel(dir, instance, false, true, parent)
{
    m_column_names = QStringList({ tr("Name"), tr("Format"), tr("Dimensions"), tr("Last Modified"), tr("Size") });
    m_column_names_translated = QStringList({ tr("Name"), tr("Format"), tr("Dimensions"), tr("Last Modified"), tr("Size") });
    m_column_sort_keys = { SortType::NAME, SortType::NAME, SortType::SIZE, SortType::DATE, SortType::SIZE };
    m_column_resize_modes = { QHeaderView::Stretch, QHeaderView::ResizeToContents, QHeaderView::ResizeToContents, QHeaderView::Interactive, QHeaderView::Interactive };
    m_columnsHideable = { false, false, false, true, true };
}

QVariant SchematicsFolderModel::data(const QModelIndex& index, int role) const
{
    if (!validateIndex(index))
        return {};

    int row = index.row();
    int column = index.column();
    auto resource = qobject_cast<SchematicResource*>(m_resources[row].data());
    if(!resource) return {};

    switch (role) {
        case Qt::DisplayRole:
            switch (column) {
                case NameColumn:
                    return resource->name();
                case FormatColumn:
                    return resource->formatString();
                case DimensionsColumn:
                    return resource->dimensionsString();
                case DateColumn:
                    return resource->dateTimeChanged();
                case SizeColumn:
                    return resource->sizeStr();
                default:
                    return {};
            }
        case Qt::DecorationRole: {
            if (column == NameColumn) {
                // optional icon
            }
            return {};
        }
        case Qt::ToolTipRole: {
            if (column == NameColumn) {
                return resource->internal_id();
            }
            return {};
        }
        default:
            return {};
    }
}

QVariant SchematicsFolderModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        if (section >= 0 && section < m_column_names_translated.size())
            return m_column_names_translated[section];
    }
    return ResourceFolderModel::headerData(section, orientation, role);
}

int SchematicsFolderModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : NUM_COLUMNS;
}

