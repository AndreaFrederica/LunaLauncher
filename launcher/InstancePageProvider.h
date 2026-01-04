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
    explicit InstancePageProvider(InstancePtr parent) { inst = parent; }

    virtual ~InstancePageProvider() = default;
    virtual QList<BasePage*> getPages() override
    {
        QList<BasePage*> values;

        if (auto serverInst = std::dynamic_pointer_cast<ServerInstance>(inst)) {
            values.append(new ServerConsolePage(serverInst.get()));

            // Mods page
            auto modsPage = new ModFolderPage(serverInst.get(), serverInst->loaderModList());
            modsPage->setFilter("%1 (*.jar *.zip)");
            values.append(modsPage);

            // Plugins page
            auto pluginsPage = new PluginFolderPage(serverInst.get(), serverInst->pluginList());
            pluginsPage->setFilter("%1 (*.jar)");
            values.append(pluginsPage);

            values.append(new NotesPage(serverInst.get()));
            values.append(new ServerModLoaderPage(serverInst.get()));
            values.append(new ServerSettingsPage(serverInst.get()));
            values.append(new ServerJavaPage(serverInst.get()));
            values.append(new ServerEulaPage(serverInst.get()));
            values.append(new ServerPropertyPage(serverInst.get()));
            values.append(new ServerYamlEditorPage(serverInst.get(), ServerYamlEditorPage::Bukkit));
            values.append(new ServerYamlEditorPage(serverInst.get(), ServerYamlEditorPage::Spigot));
            values.append(new ServerOpsPage(serverInst.get()));
            values.append(new ServerPlayerListPage(serverInst.get(), ServerPlayerListPage::Whitelist));
            values.append(new ServerPlayerListPage(serverInst.get(), ServerPlayerListPage::BannedPlayers));
            values.append(new ServerPlayerListPage(serverInst.get(), ServerPlayerListPage::BannedIPs));
            values.append(new OtherLogsPage("logs", tr("Logs"), "Other-Logs", inst));
            return values;
        }

        values.append(new LogPage(inst));
        std::shared_ptr<MinecraftInstance> onesix = std::dynamic_pointer_cast<MinecraftInstance>(inst);
        if (!onesix) {
            return values;
        }
        values.append(new VersionPage(onesix.get()));
        values.append(ManagedPackPage::createPage(onesix.get()));
        auto modsPage = new ModFolderPage(onesix.get(), onesix->loaderModList());
        modsPage->setFilter("%1 (*.zip *.jar *.litemod *.nilmod)");
        values.append(modsPage);
        values.append(new CoreModFolderPage(onesix.get(), onesix->coreModList()));
        values.append(new NilModFolderPage(onesix.get(), onesix->nilModList()));
        values.append(new ResourcePackPage(onesix.get(), onesix->resourcePackList()));
        values.append(new GlobalDataPackPage(onesix.get()));
        values.append(new TexturePackPage(onesix.get(), onesix->texturePackList()));
        values.append(new ShaderPackPage(onesix.get(), onesix->shaderPackList()));
        values.append(new YesSteveModelPage(onesix.get(), onesix->yesSteveModelList()));
        values.append(new CustomPlayerModelPage(onesix.get(), onesix->customPlayerModelList()));
        values.append(new SchematicsPage(onesix.get(), onesix->schematicsList()));
        values.append(new NotesPage(onesix.get()));
        values.append(new WorldListPage(onesix, onesix->worldList()));
        values.append(new ServersPage(onesix));
        values.append(new ScreenshotsPage(FS::PathCombine(onesix->gameRoot(), "screenshots")));
        values.append(new InstanceSettingsPage(onesix));
        values.append(new OtherLogsPage("logs", tr("Other Logs"), "Other-Logs", inst));
        return values;
    }

    virtual QString dialogTitle() override { return tr("Edit Instance (%1)").arg(inst->name()); }

   protected:
    InstancePtr inst;
};
