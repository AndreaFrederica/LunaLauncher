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

#include "modplatform/ResourceAPI.h"

#include "ui/pages/modplatform/PluginPage.h"

namespace ResourceDownload {

namespace Hangar {
// TODO: Add Hangar namespace identifier
static inline QString id() { return "hangar"; }
}  // namespace Hangar

class PluginDownloadDialog;

class HangarPluginPage : public PluginPage {
    Q_OBJECT

   public:
    static HangarPluginPage* create(PluginDownloadDialog* dialog, BaseInstance& instance, ResourceAPI* customApi = nullptr)
    {
        return PluginPage::create<HangarPluginPage>(dialog, instance, customApi);
    }

    HangarPluginPage(PluginDownloadDialog* dialog, BaseInstance& instance, ResourceAPI* customApi = nullptr);
    ~HangarPluginPage() override = default;

    QString displayName() const override { return "Hangar"; }
    QString debugName() const override { return "Hangar"; }
    QIcon icon() const override;
    QString id() const override { return Hangar::id(); }
    QString helpPage() const override { return "Hangar"; }
    QString metaEntryBase() const override { return "HangarPlugins"; }

    bool shouldDisplay() const override;
};

}  // namespace ResourceDownload
