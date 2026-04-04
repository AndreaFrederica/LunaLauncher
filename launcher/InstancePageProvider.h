/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
 */
#pragma once
#include <FileSystem.h>
#include <ui/pages/instance/DataPackPage.h>
#include "minecraft/MinecraftInstance.h"
#include "ui/pages/BasePage.h"
#include "ui/pages/BasePageProvider.h"
#include "ui/pages/instance/InstanceSettingsPage.h"
#include "ui/pages/instance/LogPage.h"
#include "ui/pages/instance/ManagedPackPage.h"
#include "ui/pages/instance/ModFolderPage.h"
#include "ui/pages/instance/PluginFolderPage.h"
#include "ui/pages/instance/NotesPage.h"
#include "ui/pages/instance/OtherLogsPage.h"
#include "ui/pages/instance/ResourcePackPage.h"
#include "ui/pages/instance/ScreenshotsPage.h"
#include "ui/pages/instance/ServersPage.h"
#include "ui/pages/instance/ShaderPackPage.h"
#include "ui/pages/instance/TexturePackPage.h"
#include "ui/pages/instance/VersionPage.h"
#include "ui/pages/instance/WorldListPage.h"
#include "ui/pages/instance/YesSteveModelPage.h"
#include "ui/pages/instance/CustomPlayerModelPage.h"
#include "ui/pages/instance/CustomUIPanelPage.h"
#include "ui/pages/instance/SchematicsPage.h"
#include "server/ServerInstance.h"
#include "ui/pages/server/ServerConsolePage.h"
#include "ui/pages/server/ServerSettingsPage.h"
#include "ui/pages/server/ServerEulaPage.h"
#include "ui/pages/server/ServerOpsPage.h"
#include "ui/pages/server/ServerPlayerListPage.h"
#include "ui/pages/server/ServerYamlEditorPage.h"
#include "ui/pages/server/ServerPropertyPage.h"
#include "ui/pages/server/ServerJavaPage.h"
#include "ui/pages/server/ServerModLoaderPage.h"

class InstancePageProvider : protected QObject, public BasePageProvider {
    Q_OBJECT
   public:
    explicit InstancePageProvider(BaseInstance* parent) { inst = parent; }

    virtual ~InstancePageProvider() = default;
    virtual QList<BasePage*> getPages() override
    {
        QList<BasePage*> values;

        if (auto serverInst = dynamic_cast<ServerInstance*>(inst)) {
            values.append(new ServerConsolePage(serverInst));

            // Mods page
            auto modsPage = new ModFolderPage(serverInst, serverInst->loaderModList().get());
            modsPage->setFilter("%1 (*.jar *.zip)");
            values.append(modsPage);

            // Plugins page
            auto pluginsPage = new PluginFolderPage(serverInst, serverInst->pluginList());
            pluginsPage->setFilter("%1 (*.jar)");
            values.append(pluginsPage);

            values.append(new NotesPage(serverInst));
            values.append(new ServerModLoaderPage(serverInst));
            values.append(new ServerSettingsPage(serverInst));
            values.append(new ServerJavaPage(serverInst));
            values.append(new ServerEulaPage(serverInst));
            values.append(new ServerPropertyPage(serverInst));
            values.append(new ServerYamlEditorPage(serverInst, ServerYamlEditorPage::Bukkit));
            values.append(new ServerYamlEditorPage(serverInst, ServerYamlEditorPage::Spigot));
            values.append(new ServerOpsPage(serverInst));
            values.append(new ServerPlayerListPage(serverInst, ServerPlayerListPage::Whitelist));
            values.append(new ServerPlayerListPage(serverInst, ServerPlayerListPage::BannedPlayers));
            values.append(new ServerPlayerListPage(serverInst, ServerPlayerListPage::BannedIPs));
            values.append(new OtherLogsPage("logs", tr("Logs"), "Other-Logs", inst));
            return values;
        }

        values.append(new LogPage(inst));
        auto onesix = dynamic_cast<MinecraftInstance*>(inst);
        if (!onesix) {
            return values;
        }
        values.append(new VersionPage(onesix));
        values.append(ManagedPackPage::createPage(onesix));
        auto modsPage = new ModFolderPage(onesix, onesix->loaderModList().get());
        modsPage->setFilter("%1 (*.zip *.jar *.litemod *.nilmod)");
        values.append(modsPage);
        values.append(new CoreModFolderPage(onesix, onesix->coreModList().get()));
        values.append(new NilModFolderPage(onesix, onesix->nilModList().get()));
        values.append(new ResourcePackPage(onesix, onesix->resourcePackList().get()));
        values.append(new GlobalDataPackPage(onesix));
        values.append(new TexturePackPage(onesix, onesix->texturePackList().get()));
        values.append(new ShaderPackPage(onesix, onesix->shaderPackList().get()));
        values.append(new YesSteveModelPage(onesix, onesix->yesSteveModelList()));
        values.append(new CustomPlayerModelPage(onesix, onesix->customPlayerModelList()));
        values.append(new SchematicsPage(onesix, onesix->schematicsList()));
        values.append(new NotesPage(onesix));
        values.append(new WorldListPage(onesix, onesix->worldList().get()));
        values.append(new ServersPage(onesix));
        values.append(new ScreenshotsPage(FS::PathCombine(onesix->gameRoot(), "screenshots")));
        values.append(new CustomUIPanelPage(onesix));
        values.append(new InstanceSettingsPage(onesix));
        values.append(new OtherLogsPage("logs", tr("Other Logs"), "Other-Logs", inst));
        return values;
    }

    virtual QString dialogTitle() override { return tr("Edit Instance (%1)").arg(inst->name()); }

   protected:
    BaseInstance* inst;
};
