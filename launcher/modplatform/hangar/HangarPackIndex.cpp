// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "HangarPackIndex.h"

#include "Json.h"

namespace Hangar {

void loadIndexedPack(ModPlatform::IndexedPack& pack, QJsonObject& obj)
{
    pack.addonId = Json::requireString(obj, "name");
    pack.provider = ModPlatform::ResourceProvider::HANGAR;
    pack.name = Json::requireString(obj, "name");
    pack.slug = obj["slug"].toString("");

    if (!pack.slug.isEmpty())
        pack.websiteUrl = "https://hangar.papermc.io/" + pack.slug;
    else
        pack.websiteUrl = "";

    pack.description = obj["description"].toString("");

    if (obj.contains("owner")) {
        ModPlatform::ModpackAuthor author;
        author.name = obj["owner"].toObject()["name"].toString();
        author.url = "https://hangar.papermc.io/" + author.name;
        pack.authors = { author };
    }

    pack.logoUrl = obj["avatarUrl"].toString("");
    pack.logoName = QString("%1.png").arg(pack.slug);

    pack.extraDataLoaded = false;
}

void loadExtraPackInfo(ModPlatform::IndexedPack& pack, QJsonObject& obj)
{
    if (obj.contains("settings")) {
        auto settings = obj["settings"].toObject();

        pack.extraData.sourceUrl = settings["sources"].toString("");
        if (pack.extraData.sourceUrl.endsWith('/'))
            pack.extraData.sourceUrl.chop(1);

        pack.extraData.issuesUrl = settings["issues"].toString("");
        if (pack.extraData.issuesUrl.endsWith('/'))
            pack.extraData.issuesUrl.chop(1);

        pack.extraData.wikiUrl = settings["wiki"].toString("");
        if (pack.extraData.wikiUrl.endsWith('/'))
            pack.extraData.wikiUrl.chop(1);
    }

    pack.extraDataLoaded = true;
}

ModPlatform::IndexedVersion loadIndexedPackVersion(QJsonObject& obj)
{
    ModPlatform::IndexedVersion version;

    version.version = Json::requireString(obj, "name");
    version.version_number = version.version;

    if (obj.contains("downloads")) {
        auto downloads = obj["downloads"].toObject();
        for (auto platformKey : downloads.keys()) {
            auto platformDownload = downloads[platformKey].toObject();
            if (platformDownload.contains("downloadUrl")) {
                version.downloadUrl = Json::requireString(platformDownload, "downloadUrl");
                version.fileName = version.downloadUrl.split('/').last();
                break;
            }
        }
    }

    if (obj.contains("changelog"))
        version.changelog = obj["changelog"].toString();

    if (obj.contains("platformDependencies")) {
        auto deps = obj["platformDependencies"].toObject();
        for (auto platform : deps.keys()) {
            auto versions = deps[platform].toArray();
            for (auto ver : versions) {
                version.mcVersion.push_back(ver.toString());
            }
        }
    }

    version.date = Json::requireString(obj, "createdAt");
    version.version_type = ModPlatform::IndexedVersionType::Release;

    return version;
}

}  // namespace Hangar
