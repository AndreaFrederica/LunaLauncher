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

#include <QJsonObject>
#include "modplatform/ModIndex.h"

namespace Hangar {

void loadIndexedPack(ModPlatform::IndexedPack& m, QJsonObject& obj);
void loadExtraPackInfo(ModPlatform::IndexedPack& m, QJsonObject& obj);
ModPlatform::IndexedVersion loadIndexedPackVersion(QJsonObject& obj);

}  // namespace Hangar
