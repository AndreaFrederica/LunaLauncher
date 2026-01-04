// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#pragma once

#include "ui/pages/modplatform/PluginPage.h"

#include "modplatform/jsapi/JSAPIManager.h"

namespace ResourceDownload {
namespace Modrinth {

class ModrinthPluginPage : public ResourceDownload::PluginPage {
    Q_OBJECT

   public:
    static ModrinthPluginPage* create(PluginDownloadDialog* dialog, BaseInstance& instance)
    {
        auto page = new ModrinthPluginPage(dialog, instance);
        auto model = static_cast<PluginModel*>(page->getModel());

        connect(model, &ResourceModel::versionListUpdated, page, &ResourcePage::versionListUpdated);
        connect(model, &ResourceModel::projectInfoUpdated, page, &ResourcePage::updateUi);
        connect(model, &QAbstractListModel::modelReset, page, &ResourcePage::modelReset);

        return page;
    }

    [[nodiscard]] QString displayName() const override { return tr("Modrinth"); }
    [[nodiscard]] QIcon icon() const override { return Modrinth::icon(); }

    [[nodiscard]] bool shouldDisplay() const override;

   protected:
    ModrinthPluginPage(PluginDownloadDialog* dialog, BaseInstance& instance);
};

}  // namespace Modrinth
}  // namespace ResourceDownload
