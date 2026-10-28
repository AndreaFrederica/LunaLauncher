// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2026 KasuganoMeow <kasugano1966@proton.me>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "DeveloperMode.h"

#include <QDir>
#include <QFileInfo>

namespace {
constexpr const char* toString(DeveloperMode::DebugMarker marker) {
    switch (marker) {
        case DeveloperMode::Marker0: return "/debug/__debug_type_0__";
    }
    return "unknown";
}
}

bool DeveloperMode::isEnabled()
{
#ifdef LAUNCHER_DEVELOPER_MODE_BUILD
    return true;
#else
    return false;
#endif
}

bool DeveloperMode::hasDebugMarker(DebugMarker marker)
{
    return QFileInfo::exists(QDir::currentPath() + toString(marker));
}
