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

#include "MirrorDownload.h"

const std::array<MirrorDownload::MirrorTypeInfo, 3> MirrorDownload::MirrorTypes = {
    {
        // Official - Empty strings mean use BuildConfig defaults
        {
            QT_TRANSLATE_NOOP("APIPage", "Official"),
            "",  // MetaURLOverride - use BuildConfig.META_URL
            "",  // ResourceURL - use BuildConfig.DEFAULT_RESOURCE_BASE
            "",  // LibrariesURL - use BuildConfig.LIBRARY_BASE
            ""   // FMLLibsURL - use BuildConfig.FMLLIBS_BASE_URL
        },
        // BMCLAPI - https://bmclapi2.bangbang93.com
        {
            QT_TRANSLATE_NOOP("APIPage", "BMCLAPI"),
            "https://bmclapi2.bangbang93.com/",
            "https://bmclapi2.bangbang93.com/assets/",
            "https://bmclapi2.bangbang93.com/maven/",
            "https://bmclapi2.bangbang93.com/maven/"  // FML Libraries
        },
        // Custom - User-provided URLs
        {
            QT_TRANSLATE_NOOP("APIPage", "Custom"),
            "",  // User-provided
            "",  // User-provided
            "",  // User-provided
            ""   // User-provided
        }
    }
};
