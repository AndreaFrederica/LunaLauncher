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

class SettingsObject;

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

    /// Effective mirror endpoints for the current settings.
    struct ResolvedMirror {
        MirrorType type = Official;

        /// Base for launcher.mojang.com, piston-*.mojang.com, launchermeta.mojang.com
        /// and s3.amazonaws.com/Minecraft.Download (all keep their original path).
        QString mojangBase;

        /// Maven root for libraries.minecraft.net and the mod loader Maven repositories.
        QString librariesBase;

        /// Base for resources.download.minecraft.net.
        QString assetsBase;

        /// Base for meta.fabricmc.net.
        QString fabricMetaBase;

        /// Base for meta.quiltmc.org.
        QString quiltMetaBase;

        /// Base for the legacy FML libraries.
        QString fmlLibsBase;

        /// Base for authlib-injector.yushi.moe.
        QString authlibInjectorBase;

        /// Optional replacement base for cdn.modrinth.com (mod file downloads).
        /// Independent of the Mojang mirror type: it is configured on its own.
        QString modrinthCdnBase;

        bool isOfficial() const { return type == Official; }
        bool isEmpty() const { return mojangBase.isEmpty() && librariesBase.isEmpty() && assetsBase.isEmpty(); }
    };

    static const std::array<MirrorTypeInfo, 3> MirrorTypes;

    /// Build the effective mirror configuration from settings. A null settings object
    /// resolves to the official servers.
    static ResolvedMirror resolve(SettingsObject* settings);

    /// Rewrite a download URL according to the given mirror configuration.
    static QString rewriteUrl(const QString& url, const ResolvedMirror& mirror);

    /// Convenience overload that resolves the mirror from the given settings first.
    static QString rewriteUrl(const QString& url, SettingsObject* settings);
};
