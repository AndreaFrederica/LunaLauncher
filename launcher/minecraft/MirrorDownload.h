// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2025 TheKodeToad <TheKodeToad@proton.me>
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

#pragma once

#include <QString>
#include <array>

class MirrorDownload {
public:
    enum MirrorType : int {
        Official,  // Use Mojang's official servers
        BMCLAPI,   // Use BMCLAPI mirror
        Custom,    // User-specified URLs
        First = Official,
        Last = Custom
    };

    struct MirrorTypeInfo {
        const QString name;
        const QString defaultMetaUrl;
        const QString defaultAssetsUrl;
        const QString defaultLibrariesUrl;
        const QString defaultFMLLibsUrl;
    };

    static const std::array<MirrorTypeInfo, 3> MirrorTypes;
};
