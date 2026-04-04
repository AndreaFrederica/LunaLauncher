/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
*/
#include "ServerPluginsPage.h"
#include "ui_ExternalResourcesPage.h"
#include "minecraft/mod/PluginFolderModel.h"

ServerPluginsPage::ServerPluginsPage(BaseInstance *inst, std::shared_ptr<PluginFolderModel> plugins, QWidget *parent)
    : ExternalResourcesPage(inst, plugins.get(), parent)
{
    setFilter("%1 (*.jar)");
}
