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
#include <QJsonObject>

#include "modplatform/jsapi/JSAPIManager.h"
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

    QJsonObject context{
        { QStringLiteral("mirrorType"), mirror.isOfficial() ? QStringLiteral("official") :
          (mirror.type == BMCLAPI ? QStringLiteral("bmclapi") : QStringLiteral("custom")) },
        { QStringLiteral("mirrorTypeId"), static_cast<int>(mirror.type) },
        { QStringLiteral("mojangBase"), mirror.mojangBase },
        { QStringLiteral("librariesBase"), mirror.librariesBase },
        { QStringLiteral("assetsBase"), mirror.assetsBase },
        { QStringLiteral("fabricMetaBase"), mirror.fabricMetaBase },
        { QStringLiteral("quiltMetaBase"), mirror.quiltMetaBase },
        { QStringLiteral("fmlLibsBase"), mirror.fmlLibsBase },
        { QStringLiteral("authlibInjectorBase"), mirror.authlibInjectorBase },
        { QStringLiteral("modrinthCdnBase"), mirror.modrinthCdnBase },
    };
    auto& jsApis = JSAPIManager::instance();
    // Tests and small command-line tools can use MirrorDownload without going
    // through Application::initialize(). Load the packaged/user JS hooks on
    // first use in that case.
    if (jsApis.getAllAPIs().isEmpty())
        jsApis.initialize();
    return jsApis.rewriteUrl(url, context);
}

QString MirrorDownload::rewriteUrl(const QString& url, SettingsObject* settings)
{
    return rewriteUrl(url, resolve(settings));
}
