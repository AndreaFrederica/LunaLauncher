// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "HangarResourcePages.h"
#include "ui_ResourcePage.h"

#include "HangarResourceModels.h"

#include "modplatform/hangar/HangarAPI.h"
#include "server/ServerInstance.h"

namespace ResourceDownload {

HangarPluginPage::HangarPluginPage(PluginDownloadDialog* dialog, BaseInstance& instance, ResourceAPI* customApi) : PluginPage(dialog, instance)
{
    m_model = new HangarPluginModel(instance);
    m_ui->packView->setModel(m_model);

    addSortings();

    connect(m_ui->packView->selectionModel(), &QItemSelectionModel::currentChanged, this, &HangarPluginPage::onSelectionChanged);
    connect(m_ui->versionSelectionBox, &QComboBox::currentIndexChanged, this, &HangarPluginPage::onVersionSelectionChanged);
    connect(m_ui->resourceSelectionButton, &QPushButton::clicked, this, &HangarPluginPage::onResourceSelected);

    m_ui->packDescription->setMetaEntry(metaEntryBase());
}

QIcon HangarPluginPage::icon() const
{
    // TODO: Add Hangar icon resource
    return QIcon::fromTheme("hangar");
}

bool HangarPluginPage::shouldDisplay() const
{
    // Only display for server instances
    if (!m_baseInstance.traits().contains("server"))
        return false;

    auto serverInst = dynamic_cast<ServerInstance*>(&m_baseInstance);
    if (!serverInst)
        return false;

    // Hangar is for server plugins, always show for server instances
    return true;
}

}  // namespace ResourceDownload
