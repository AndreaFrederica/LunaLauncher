/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
*/
#include "ServerPluginsPage.h"

ServerPluginsPage::ServerPluginsPage(BaseInstance *inst, std::shared_ptr<ModFolderModel> mods, QWidget *parent)
    : ModFolderPage(inst, mods, parent)
{
    setFilter("%1 (*.jar)");
}
