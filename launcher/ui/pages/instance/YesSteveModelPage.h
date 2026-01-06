/*
 *  Luna Launcher - Minecraft Launcher 
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
 */
#pragma once
#include "ExternalResourcesPage.h"
#include "minecraft/mod/YesSteveModelFolderModel.h"

class YesSteveModelPage : public ExternalResourcesPage {
    Q_OBJECT
public:
    explicit YesSteveModelPage(MinecraftInstance* instance, std::shared_ptr<YesSteveModelFolderModel> model, QWidget* parent = nullptr);
    virtual ~YesSteveModelPage() = default;

    QString displayName() const override { return tr("Yes Steve Models"); }
    QIcon icon() const override { return QIcon::fromTheme("resourcepacks"); } 
    QString id() const override { return "yesstevemodel"; }
    QString helpPage() const override { return "Yes-Steve-Model"; }

    bool shouldDisplay() const override { return true; }

protected slots:
    void renameItem();

private:
    std::shared_ptr<YesSteveModelFolderModel> m_model;
};
