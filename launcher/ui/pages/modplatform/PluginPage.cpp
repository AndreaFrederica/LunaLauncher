// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "PluginPage.h"
#include "ui_ResourcePage.h"

#include <QDesktopServices>
#include <QKeyEvent>

#include <memory>

#include "Application.h"
#include "ResourceDownloadTask.h"

#include "server/ServerInstance.h"
#include "minecraft/mod/PluginFolderModel.h"

#include "ui/dialogs/ResourceDownloadDialog.h"

namespace ResourceDownload {

PluginPage::PluginPage(PluginDownloadDialog* dialog, BaseInstance& instance) : ResourcePage(dialog, instance)
{
    // Hide filter button for plugins as we don't support filtering yet
    m_ui->resourceFilterButton->setHidden(true);
}

void PluginPage::triggerSearch()
{
    m_ui->packView->selectionModel()->setCurrentIndex({}, QItemSelectionModel::SelectionFlag::ClearAndSelect);
    m_ui->packView->clearSelection();
    m_ui->packDescription->clear();
    m_ui->versionSelectionBox->clear();
    updateSelectionButton();

    static_cast<PluginModel*>(m_model)->searchWithTerm(getSearchTerm(), m_ui->sortByBox->currentData().toUInt(), false);
}

QMap<QString, QString> PluginPage::urlHandlers() const
{
    // TODO: Add Hangar URL handlers when implementing Hangar support
    return {};
}

void PluginPage::addResourceToPage(ModPlatform::IndexedPack::Ptr pack,
                                   ModPlatform::IndexedVersion& version,
                                   std::shared_ptr<ResourceFolderModel> base_model)
{
    auto pluginList = dynamic_cast<PluginFolderModel*>(base_model.get());
    if (!pluginList) {
        qWarning() << "PluginPage::addResourceToPage: base_model is not a PluginFolderModel";
        return;
    }

    m_parentDialog->addResource(pack, version);
}

}  // namespace ResourceDownload
