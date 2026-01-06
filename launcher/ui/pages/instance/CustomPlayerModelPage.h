/*
 *  Luna Launcher - Minecraft Launcher 
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
 */
#pragma once
#include "ExternalResourcesPage.h"
#include "minecraft/mod/CustomPlayerModelFolderModel.h"

class CustomPlayerModelPage : public ExternalResourcesPage {
    Q_OBJECT
public:
    explicit CustomPlayerModelPage(MinecraftInstance* instance, std::shared_ptr<CustomPlayerModelFolderModel> model, QWidget* parent = nullptr);
    virtual ~CustomPlayerModelPage() = default;

    QString displayName() const override { return tr("Custom Player Models"); }
    QIcon icon() const override { return QIcon::fromTheme("resourcepacks"); }
    QString id() const override { return "customplayermodel"; }
    QString helpPage() const override { return "Custom-Player-Model"; }

    bool shouldDisplay() const override { return true; }

protected slots:
    void renameItem();

private:
    std::shared_ptr<CustomPlayerModelFolderModel> m_model;
};
