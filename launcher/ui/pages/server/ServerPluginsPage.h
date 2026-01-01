/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
*/
#pragma once
#include "ui/pages/instance/ModFolderPage.h"

class ServerPluginsPage : public ModFolderPage
{
    Q_OBJECT
public:
    explicit ServerPluginsPage(BaseInstance *inst, std::shared_ptr<ModFolderModel> mods, QWidget *parent = nullptr);
    virtual ~ServerPluginsPage() = default;

    QString displayName() const override { return tr("Plugins"); }
    QIcon icon() const override { return QIcon::fromTheme("loadermods"); }
    QString id() const override { return "plugins"; }
    QString helpPage() const override { return "Plugins"; }
};
