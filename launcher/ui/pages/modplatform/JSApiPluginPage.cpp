// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "JSApiPluginPage.h"
#include "ui_ResourcePage.h"

#include "ui/pages/modplatform/PluginModel.h"

#include "BaseInstance.h"

namespace ResourceDownload {

JSApiPluginPage::JSApiPluginPage(PluginDownloadDialog* dialog, BaseInstance& instance, ResourceAPI* api) : PluginPage(dialog, instance)
{
    // Store API info for display
    m_apiName = api->apiName();
    m_apiId = api->metaEntryBase().remove("Plugins");  // Remove "Plugins" suffix to get the ID

    // Create model with the provided API
    m_model = new PluginModel(instance, api, api->apiName(), api->metaEntryBase(), false);
    m_ui->packView->setModel(m_model);

    addSortings();

    connect(m_ui->packView->selectionModel(), &QItemSelectionModel::currentChanged, this, &JSApiPluginPage::onSelectionChanged);
    connect(m_ui->versionSelectionBox, &QComboBox::currentIndexChanged, this, &JSApiPluginPage::onVersionSelectionChanged);
    connect(m_ui->resourceSelectionButton, &QPushButton::clicked, this, &JSApiPluginPage::onResourceSelected);

    m_ui->packDescription->setMetaEntry(metaEntryBase());
}

QIcon JSApiPluginPage::icon() const
{
    // Try to load theme icon based on API ID, fallback to generic extension icon
    return QIcon::fromTheme(m_apiId, QIcon::fromTheme("extension"));
}

bool JSApiPluginPage::shouldDisplay() const
{
    // Always display JS API pages
    return true;
}

}  // namespace ResourceDownload
