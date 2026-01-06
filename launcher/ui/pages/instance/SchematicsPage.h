/*
 *  Luna Launcher - Minecraft Launcher 
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
 */
#pragma once
#include "ExternalResourcesPage.h"
#include "minecraft/mod/SchematicsFolderModel.h"

class SchematicsPage : public ExternalResourcesPage {
    Q_OBJECT
public:
    SchematicsPage(MinecraftInstance* instance, std::shared_ptr<SchematicsFolderModel> model, QWidget* parent = nullptr);

    QString id() const override { return "Schematics"; }
    QString displayName() const override { return tr("Schematics"); }
    QString helpPage() const override { return "Schematics"; }
    QIcon icon() const override { return QIcon::fromTheme("resourcepacks"); }
    bool shouldDisplay() const override { return true; }

private slots:
    void renameItem();

private:
    std::shared_ptr<SchematicsFolderModel> m_model;
};
