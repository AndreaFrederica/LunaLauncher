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

#include <QCoreApplication>

#include "settings/SettingsObject.h"

namespace {

QString normalizeBase(QString base)
{
    base = base.trimmed();
    if (base.isEmpty())
        return {};
    if (!base.endsWith('/'))
        base += '/';
    return base;
}

QString joinBase(const QString& base, const QString& path)
{
    if (base.isEmpty())
        return {};
    return base + path;
}

/**
 * Replace a host (and optionally a path prefix below it) with another base.
 *
 * The original URL keeps everything after the matched part, including the query
 * string. `pathPrefix` must not start with a slash, because a single leading
 * slash after the host is always consumed.
 */
bool replaceHost(QString& url, const QString& host, const QString& base, const QString& pathPrefix = {})
{
    if (base.isEmpty() || url.isEmpty())
        return false;

    for (const auto& scheme : { QStringLiteral("https://"), QStringLiteral("http://") }) {
        const QString prefix = scheme + host;
        if (!url.startsWith(prefix, Qt::CaseInsensitive))
            continue;

        QString rest = url.mid(prefix.size());
        // Guard against matching a longer host name (e.g. "example.com.evil").
        if (!rest.isEmpty() && !rest.startsWith('/'))
            continue;

        while (rest.startsWith('/'))
            rest = rest.mid(1);

        if (!pathPrefix.isEmpty()) {
            if (!rest.startsWith(pathPrefix, Qt::CaseInsensitive))
                continue;
            rest = rest.mid(pathPrefix.size());
        }

        url = base + rest;
        return true;
    }

    return false;
}

}  // namespace

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

MirrorDownload::ResolvedMirror MirrorDownload::resolve(SettingsObject* settings)
{
    ResolvedMirror mirror;
    if (!settings)
        return mirror;

    // Third-party mod file CDNs have their own override and must also work when
    // the Mojang downloads are served officially.
    mirror.modrinthCdnBase = normalizeBase(settings->get("ModrinthDownloadMirrorURL").toString());

    const int type = settings->get("DownloadMirrorType").toInt();
    if (type < First || type > Last)
        return mirror;

    mirror.type = static_cast<MirrorType>(type);

    if (mirror.type == Official)
        return mirror;

    if (mirror.type == BMCLAPI) {
        const QString base = normalizeBase(QStringLiteral("https://bmclapi2.bangbang93.com"));
        mirror.mojangBase = base;
        mirror.librariesBase = joinBase(base, QStringLiteral("maven/"));
        mirror.assetsBase = joinBase(base, QStringLiteral("assets/"));
        mirror.fabricMetaBase = joinBase(base, QStringLiteral("fabric-meta/"));
        mirror.quiltMetaBase = joinBase(base, QStringLiteral("quilt-meta/"));
        mirror.fmlLibsBase = mirror.librariesBase;
        mirror.authlibInjectorBase = joinBase(base, QStringLiteral("mirrors/authlib-injector/"));
        return mirror;
    }

    // Custom: every endpoint is optional. If only the Mojang downloads base is
    // configured, assume a BMCLAPI-compatible layout for the well-known subtrees
    // so a single custom API URL is enough for the common case.
    mirror.mojangBase = normalizeBase(settings->get("MojangDownloadsMirrorURL").toString());
    mirror.librariesBase = normalizeBase(settings->get("LibrariesURL").toString());
    mirror.assetsBase = normalizeBase(settings->get("ResourceURLOverride").toString());
    mirror.fmlLibsBase = normalizeBase(settings->get("LegacyFMLLibsURLOverride").toString());

    if (!mirror.mojangBase.isEmpty()) {
        if (mirror.librariesBase.isEmpty())
            mirror.librariesBase = joinBase(mirror.mojangBase, QStringLiteral("maven/"));
        if (mirror.assetsBase.isEmpty())
            mirror.assetsBase = joinBase(mirror.mojangBase, QStringLiteral("assets/"));
        mirror.fabricMetaBase = joinBase(mirror.mojangBase, QStringLiteral("fabric-meta/"));
        mirror.quiltMetaBase = joinBase(mirror.mojangBase, QStringLiteral("quilt-meta/"));
        mirror.authlibInjectorBase = joinBase(mirror.mojangBase, QStringLiteral("mirrors/authlib-injector/"));
    }

    if (mirror.fmlLibsBase.isEmpty())
        mirror.fmlLibsBase = mirror.librariesBase;

    return mirror;
}

QString MirrorDownload::rewriteUrl(const QString& url, const ResolvedMirror& mirror)
{
    if (url.isEmpty())
        return url;

    QString rewritten = url;

    // Mod file CDN override (e.g. the Modrinth fallback for blocked CurseForge
    // mods). Configured independently of the Mojang mirror selection.
    replaceHost(rewritten, QStringLiteral("cdn.modrinth.com"), mirror.modrinthCdnBase);

    if (mirror.isOfficial())
        return rewritten;

    // Assets objects. AssetUpdateTask also supplies the assets base directly, but
    // absolute asset URLs can appear in other version metadata as well.
    replaceHost(rewritten, QStringLiteral("resources.download.minecraft.net"), mirror.assetsBase);

    // Maven repositories. They all share a single Maven root and only differ in
    // the path prefix that the official URL uses.
    replaceHost(rewritten, QStringLiteral("libraries.minecraft.net"), mirror.librariesBase);
    replaceHost(rewritten, QStringLiteral("files.minecraftforge.net"), mirror.librariesBase, QStringLiteral("maven/"));
    replaceHost(rewritten, QStringLiteral("maven.minecraftforge.net"), mirror.librariesBase);
    replaceHost(rewritten, QStringLiteral("maven.fabricmc.net"), mirror.librariesBase);
    replaceHost(rewritten, QStringLiteral("maven.neoforged.net"), mirror.librariesBase, QStringLiteral("releases/"));
    replaceHost(rewritten, QStringLiteral("maven.quiltmc.org"), mirror.librariesBase, QStringLiteral("repository/release/"));

    // Mod loader metadata.
    replaceHost(rewritten, QStringLiteral("meta.fabricmc.net"), mirror.fabricMetaBase);
    replaceHost(rewritten, QStringLiteral("meta.quiltmc.org"), mirror.quiltMetaBase);

    // LiteLoader is served from BMCLAPI's Maven tree under a fixed path.
    replaceHost(rewritten, QStringLiteral("dl.liteloader.com"), joinBase(mirror.librariesBase, QStringLiteral("com/mumfrey/liteloader/")),
                QStringLiteral("versions/"));

    // Misc Mojang-hosted files.
    replaceHost(rewritten, QStringLiteral("authlib-injector.yushi.moe"), mirror.authlibInjectorBase);

    // Mojang's download CDNs keep their path below the mirror root.
    replaceHost(rewritten, QStringLiteral("launcher.mojang.com"), mirror.mojangBase);
    replaceHost(rewritten, QStringLiteral("launchermeta.mojang.com"), mirror.mojangBase);
    replaceHost(rewritten, QStringLiteral("piston-data.mojang.com"), mirror.mojangBase);
    replaceHost(rewritten, QStringLiteral("piston-meta.mojang.com"), mirror.mojangBase);
    replaceHost(rewritten, QStringLiteral("s3.amazonaws.com"), mirror.mojangBase, QStringLiteral("Minecraft.Download/"));

    return rewritten;
}

QString MirrorDownload::rewriteUrl(const QString& url, SettingsObject* settings)
{
    return rewriteUrl(url, resolve(settings));
}
