/*
 *  Luna Launcher - Minecraft Launcher 
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
 */
#pragma once
#include "ResourceFolderModel.h"
#include "SchematicResource.h"

class SchematicsFolderModel : public ResourceFolderModel {
    Q_OBJECT
public:
    enum Columns { NameColumn = 0, FormatColumn, DimensionsColumn, DateColumn, SizeColumn, NUM_COLUMNS };

    explicit SchematicsFolderModel(const QDir& dir, BaseInstance* instance, QObject* parent = nullptr);

    QString id() const override { return "schematics"; }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    int columnCount(const QModelIndex& parent) const override;

protected:
    Resource* createResource(const QFileInfo& file) override { return new SchematicResource(file); }
    Task* createParseTask(Resource&) override { return nullptr; }

    RESOURCE_HELPERS(SchematicResource)
};

