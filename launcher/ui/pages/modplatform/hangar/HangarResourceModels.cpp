// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "HangarResourceModels.h"

#include "Json.h"
#include "modplatform/ModIndex.h"
#include "modplatform/hangar/HangarAPI.h"
#include "server/ServerInstance.h"

namespace ResourceDownload {

HangarPluginModel::HangarPluginModel(BaseInstance& base)
    : PluginModel(base, new HangarAPI(), "Hangar", "HangarPlugins")
{
}

}  // namespace ResourceDownload
