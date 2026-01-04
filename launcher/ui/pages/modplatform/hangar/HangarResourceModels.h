// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica<andreafrederica@outlook.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#pragma once

#include "BaseInstance.h"
#include "modplatform/hangar/HangarAPI.h"
#include "ui/pages/modplatform/PluginModel.h"

namespace ResourceDownload {

class HangarPluginModel : public PluginModel {
    Q_OBJECT

   public:
    HangarPluginModel(BaseInstance& base);
    ~HangarPluginModel() override = default;

    [[nodiscard]] QString debugName() const override { return "Hangar"; }
    [[nodiscard]] inline QString metaEntryBase() const override { return "HangarPlugins"; }
};

}  // namespace ResourceDownload
