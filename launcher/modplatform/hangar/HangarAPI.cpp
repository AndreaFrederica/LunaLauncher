// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "HangarAPI.h"

#include "Application.h"
#include "Json.h"
#include "net/ApiDownload.h"
#include "net/NetJob.h"

#include <QUrl>

namespace {
QString optString(const QJsonObject& obj, const QString& key, const QString& fallback = {})
{
    const auto v = obj.value(key);
    return v.isString() ? v.toString() : fallback;
}

QJsonObject optObject(const QJsonObject& obj, const QString& key)
{
    const auto v = obj.value(key);
    return v.isObject() ? v.toObject() : QJsonObject{};
}

int optInt(const QJsonObject& obj, const QString& key, int fallback = 0)
{
    const auto v = obj.value(key);
    return v.isDouble() ? v.toInt() : fallback;
}

bool optBool(const QJsonObject& obj, const QString& key, bool fallback = false)
{
    const auto v = obj.value(key);
    return v.isBool() ? v.toBool() : fallback;
}
}  // namespace

Task::Ptr HangarAPI::getProjects(QStringList pluginIds, std::shared_ptr<QByteArray> response) const
{
    // Hangar 不支持批量请求，需要多个单独请求
    Q_UNUSED(pluginIds);
    Q_UNUSED(response);
    return nullptr;
}

auto HangarAPI::getSortingMethods() const -> QList<ResourceAPI::SortingMethod>
{
    return { { 0, "stars", QObject::tr("Sort by stars") },
             { 1, "downloads", QObject::tr("Sort by downloads") },
             { 2, "views", QObject::tr("Sort by views") },
             { 3, "newest", QObject::tr("Sort by newest") },
             { 4, "updated", QObject::tr("Sort by recently updated") } };
}

void HangarAPI::loadIndexedPack(ModPlatform::IndexedPack& pack, QJsonObject& obj) const
{
    //qDebug() << "[HangarAPI] loadIndexedPack - Raw JSON:" << QJsonDocument(obj).toJson(QJsonDocument::Compact);

    // Use owner/slug as pack ID for Hangar
    auto namespaceObj = Json::requireObject(obj, "namespace");
    QString owner = Json::requireString(namespaceObj, "owner");
    QString slug = Json::requireString(namespaceObj, "slug");

    pack.addonId = QString("%1/%2").arg(owner, slug);
    pack.provider = ModPlatform::ResourceProvider::HANGAR;
    pack.name = Json::requireString(obj, "name");
    pack.slug = slug;
    pack.description = optString(obj, "description");

    if (!owner.isEmpty()) {
        pack.websiteUrl = QString("https://hangar.papermc.io/%1/%2").arg(owner, slug);
        ModPlatform::ModpackAuthor author;
        author.name = owner;
        author.url = getAuthorURL(owner);
        pack.authors.append(author);
    }

    // Avatar/logo
    pack.logoUrl = optString(obj, "avatarUrl");
    if (pack.logoUrl.isEmpty()) {
        auto settingsObj = optObject(obj, "settings");
        pack.logoUrl = optString(settingsObj, "icon");
    }

    // Set logoName for caching and display
    if (!pack.logoUrl.isEmpty()) {
        pack.logoName = QString("%1.%2").arg(slug, QFileInfo(QUrl(pack.logoUrl).fileName()).suffix());
    }

    // Stats (for reference, not stored in pack)
    auto statsObj = optObject(obj, "stats");
    Q_UNUSED(statsObj);

    // Hangar 的搜索接口已经返回了 mainPageContent，直接加载
    QString mainPageContent = optString(obj, "mainPageContent");
    if (!mainPageContent.isEmpty()) {
        pack.extraData.body = QString("Downloads: %1, Stars: %2\n\n")
                                 .arg(optInt(statsObj, "downloads"))
                                 .arg(optInt(statsObj, "stars"));
        pack.extraData.body = pack.extraData.body + mainPageContent;
        //qDebug() << "[HangarAPI] loadIndexedPack - Loaded mainPageContent:" << mainPageContent.left(100);
    }

    // qDebug() << "[HangarAPI] loadIndexedPack - Converted pack:";
    // qDebug() << "  name:" << pack.name;
    // qDebug() << "  slug:" << pack.slug;
    // qDebug() << "  addonId:" << pack.addonId;
    // qDebug() << "  logoUrl:" << pack.logoUrl;
    // qDebug() << "  description:" << pack.description.left(100);
    // qDebug() << "  body length:" << pack.extraData.body.length();
}

ModPlatform::IndexedVersion HangarAPI::loadIndexedPackVersion(QJsonObject& obj, ModPlatform::ResourceType) const
{
    //qDebug() << "[HangarAPI] loadIndexedPackVersion - Raw JSON:" << QJsonDocument(obj).toJson(QJsonDocument::Compact).left(500);

    ModPlatform::IndexedVersion ver;

    ver.version = Json::requireString(obj, "name");
    ver.version_number = ver.version;
    ver.date = optString(obj, "createdAt");
    ver.changelog = optString(obj, "description");

    // Use version ID as fileId so it passes validation
    ver.fileId = optInt(obj, "id");

    // Parse channel for version type
    auto channelObj = optObject(obj, "channel");
    QString channelName = optString(channelObj, "name");
    if (channelName.contains("Release", Qt::CaseInsensitive)) {
        ver.version_type = ModPlatform::IndexedVersionType::Release;
    } else if (channelName.contains("Beta", Qt::CaseInsensitive) || channelName.contains("Snapshot", Qt::CaseInsensitive)) {
        ver.version_type = ModPlatform::IndexedVersionType::Beta;
    } else if (channelName.contains("Alpha", Qt::CaseInsensitive)) {
        ver.version_type = ModPlatform::IndexedVersionType::Alpha;
    }

    // Parse platform dependencies (MC versions)
    auto platformDeps = optObject(obj, "platformDependencies");
    QStringList mcVersions;

    for (auto it = platformDeps.begin(); it != platformDeps.end(); ++it) {
        QJsonArray versions = it.value().toArray();
        for (const auto& v : versions) {
            QString verStr = v.toString();
            if (!mcVersions.contains(verStr)) {
                mcVersions.append(verStr);
            }
        }
    }
    ver.mcVersion = mcVersions;

    // Store all available download platforms for later selection based on user choice
    auto downloads = optObject(obj, "downloads");
    if (!downloads.isEmpty()) {
        QStringList availablePlatforms = downloads.keys();
        for (const auto& platform : availablePlatforms) {
            if (platform == "PAPER")
                ver.pluginLoaders |= ModPlatform::Paper;
            else if (platform == "SPIGOT")
                ver.pluginLoaders |= ModPlatform::Spigot;
            else if (platform == "BUKKIT")
                ver.pluginLoaders |= ModPlatform::Bukkit;
            else if (platform == "PURPUR")
                ver.pluginLoaders |= ModPlatform::Purpur;
            else if (platform == "SPONGE")
                ver.pluginLoaders |= ModPlatform::Sponge;
            else if (platform == "VELOCITY")
                ver.pluginLoaders |= ModPlatform::Velocity;
            else if (platform == "WATERFALL")
                ver.pluginLoaders |= ModPlatform::Waterfall;
            else if (platform == "BUNGEECORD")
                ver.pluginLoaders |= ModPlatform::BungeeCord;
        }

        // Pick first platform as fallback (will be refined in getProjectVersions)
        auto firstPlatform = availablePlatforms.first();
        auto platformDownload = optObject(downloads, firstPlatform);
        if (!platformDownload.isEmpty()) {
            // Most plugins use externalUrl (Modrinth CDN), not Hangar-hosted downloadUrl
            ver.downloadUrl = optString(platformDownload, "externalUrl");
            if (ver.downloadUrl.isEmpty()) {
                ver.downloadUrl = optString(platformDownload, "downloadUrl");
            }

            auto fileInfo = optObject(platformDownload, "fileInfo");
            if (!fileInfo.isEmpty()) {
                ver.fileName = optString(fileInfo, "name");
                ver.hash = optString(fileInfo, "sha256Hash");
                ver.hash_type = "sha256";
            } else if (!ver.downloadUrl.isEmpty()) {
                QUrl url(ver.downloadUrl);
                ver.fileName = url.fileName();
                if (ver.fileName.isEmpty()) {
                    ver.fileName = QString("%1.jar").arg(ver.version);
                }
            }
        }
    }

    // qDebug() << "[HangarAPI] loadIndexedPackVersion - Converted version:";
    // qDebug() << "  version:" << ver.version;
    // qDebug() << "  fileId:" << ver.fileId;
    // qDebug() << "  downloadUrl:" << ver.downloadUrl;
    // qDebug() << "  fileName:" << ver.fileName;
    // qDebug() << "  pluginLoaders:" << ver.pluginLoaders;
    // qDebug() << "  mcVersion:" << ver.mcVersion;

    return ver;
}

QJsonArray HangarAPI::documentToArray(QJsonDocument& doc) const
{
    return Json::requireObject(doc)["result"].toArray();
}

void HangarAPI::loadExtraPackInfo(ModPlatform::IndexedPack& pack, QJsonObject& obj) const
{
    //qDebug() << "[HangarAPI] loadExtraPackInfo - Pack:" << pack.name;

    // 加载详细介绍 (mainPageContent)
    QString mainPageContent = optString(obj, "mainPageContent");
    if (!mainPageContent.isEmpty()) {
        pack.extraData.body = mainPageContent;
        //qDebug() << "[HangarAPI] loadExtraPackInfo - Loaded body:" << mainPageContent.left(100);
    }

    // 确保 logoUrl 被设置
    QString avatarUrl = optString(obj, "avatarUrl");
    if (!avatarUrl.isEmpty() && pack.logoUrl.isEmpty()) {
        pack.logoUrl = avatarUrl;
        qDebug() << "[HangarAPI] loadExtraPackInfo - Set logoUrl:" << avatarUrl;
    }

    // 加载额外的项目信息
    auto settingsObj = optObject(obj, "settings");

    // 链接信息
    auto linksArray = settingsObj.value("links").toArray();
    if (!linksArray.isEmpty()) {
        // Hangar links structure: [{ "type": "top", "links": [...] }]
        for (const auto& linkGroupVal : linksArray) {
            auto linkGroup = linkGroupVal.toObject();
            auto linksInGroup = linkGroup.value("links").toArray();
            for (const auto& linkVal : linksInGroup) {
                auto link = linkVal.toObject();
                QString linkName = optString(link, "name").toLower();
                QString linkUrl = optString(link, "url");

                if (linkName.contains("github") || linkName.contains("source")) {
                    pack.extraData.sourceUrl = linkUrl;
                } else if (linkName.contains("issue")) {
                    pack.extraData.issuesUrl = linkUrl;
                } else if (linkName.contains("wiki")) {
                    pack.extraData.wikiUrl = linkUrl;
                } else if (linkName.contains("discord")) {
                    pack.extraData.discordUrl = linkUrl;
                }
            }
        }
    }

    // 捐赠信息
    auto donateObj = optObject(settingsObj, "donation");
    if (!donateObj.isEmpty()) {
        bool enabled = optBool(donateObj, "enable", false);
        if (enabled) {
            ModPlatform::DonationData donation;
            donation.platform = "custom";
            donation.url = optString(donateObj, "url");
            if (!donation.url.isEmpty()) {
                pack.extraData.donate.append(donation);
            }
        }
    }

    // 许可信息
    auto licenseObj = optObject(settingsObj, "license");
    if (!licenseObj.isEmpty()) {
        QString licenseName = optString(licenseObj, "name");
        QString licenseType = optString(licenseObj, "type");
        if (!licenseName.isEmpty() || !licenseType.isEmpty()) {
            QString licenseInfo;
            if (!licenseName.isEmpty()) {
                licenseInfo = licenseName;
            } else if (!licenseType.isEmpty()) {
                licenseInfo = licenseType;
            }
            // Append to body if we have mainPageContent
            if (!pack.extraData.body.isEmpty()) {
                pack.extraData.body += QString("\n\n---\n**License:** %1").arg(licenseInfo);
            }
        }
    }

    //qDebug() << "[HangarAPI] loadExtraPackInfo - Final logoUrl:" << pack.logoUrl;
    //qDebug() << "[HangarAPI] loadExtraPackInfo - Body length:" << pack.extraData.body.length();
}

auto HangarAPI::getPluginLoaderAsString(ModPlatform::PluginLoaderType loader) -> QString
{
    switch (loader) {
        case ModPlatform::Paper:
            return "paper";
        case ModPlatform::Spigot:
            return "spigot";
        case ModPlatform::Bukkit:
            return "bukkit";
        case ModPlatform::Purpur:
            return "purpur";
        case ModPlatform::Sponge:
            return "sponge";
        case ModPlatform::Velocity:
            return "velocity";
        case ModPlatform::Waterfall:
            return "waterfall";
        case ModPlatform::BungeeCord:
            return "bungeecord";
        default:
            return "";
    }
}

QStringList HangarAPI::getPluginLoaderStrings(ModPlatform::PluginLoaderTypes types)
{
    QStringList l;
    for (auto loader : { ModPlatform::Paper, ModPlatform::Spigot, ModPlatform::Bukkit,
                         ModPlatform::Purpur, ModPlatform::Sponge, ModPlatform::Velocity,
                         ModPlatform::Waterfall, ModPlatform::BungeeCord }) {
        if (types & loader) {
            l << getPluginLoaderAsString(loader);
        }
    }
    return l;
}

QString HangarAPI::getAuthorURL(const QString& author) const
{
    return QString("https://hangar.papermc.io/%1").arg(author);
}

QString HangarAPI::selectBestPlatform(const QJsonObject& versionObj, ModPlatform::PluginLoaderTypes requestedLoaders) const
{
    auto downloads = optObject(versionObj, "downloads");
    if (downloads.isEmpty())
        return {};

    QStringList availablePlatforms = downloads.keys();

    // Platform compatibility (child → parent): Purpur → Paper → Spigot → Bukkit
    // Waterfall → BungeeCord
    auto isCompatible = [](const QString& available, ModPlatform::PluginLoaderType requested) -> bool {
        if (available == "PURPUR")
            return requested & (ModPlatform::Purpur | ModPlatform::Paper | ModPlatform::Spigot | ModPlatform::Bukkit);
        if (available == "PAPER")
            return requested & (ModPlatform::Paper | ModPlatform::Spigot | ModPlatform::Bukkit);
        if (available == "SPIGOT")
            return requested & (ModPlatform::Spigot | ModPlatform::Bukkit);
        if (available == "BUKKIT")
            return requested & ModPlatform::Bukkit;
        if (available == "WATERFALL")
            return requested & (ModPlatform::Waterfall | ModPlatform::BungeeCord);
        if (available == "BUNGEECORD")
            return requested & ModPlatform::BungeeCord;
        if (available == "VELOCITY")
            return requested & ModPlatform::Velocity;
        if (available == "SPONGE")
            return requested & ModPlatform::Sponge;
        return false;
    };

    // Priority: most specific first (Purpur > Paper > Spigot > Bukkit)
    QList<ModPlatform::PluginLoaderType> priority = {
        ModPlatform::Purpur, ModPlatform::Paper, ModPlatform::Spigot, ModPlatform::Bukkit,
        ModPlatform::Velocity, ModPlatform::Waterfall, ModPlatform::BungeeCord, ModPlatform::Sponge
    };

    for (auto requested : priority) {
        if (!(requestedLoaders & requested))
            continue;

        // Exact match first
        QString requestedStr = getPluginLoaderAsString(requested).toUpper();
        if (availablePlatforms.contains(requestedStr))
            return requestedStr;

        // Then compatible platforms (prefer more specific)
        for (const auto& available : availablePlatforms) {
            if (isCompatible(available, requested))
                return available;
        }
    }

    // Fallback
    return availablePlatforms.isEmpty() ? QString() : availablePlatforms.first();
}

Task::Ptr HangarAPI::getProjectVersions(VersionSearchArgs&& args, Callback<QVector<ModPlatform::IndexedVersion>>&& callbacks) const
{
    auto versions_url_optional = getVersionsURL(args);
    if (!versions_url_optional.has_value())
        return nullptr;

    auto versions_url = versions_url_optional.value();

    auto netJob = makeShared<NetJob>(QString("%1::Versions").arg(args.pack->name), APPLICATION->network());
    auto [action, response] = Net::ApiDownload::makeByteArray(QUrl(versions_url));
    netJob->addNetAction(action);

    QObject::connect(netJob.get(), &NetJob::succeeded, [this, response, callbacks, args] {
        QJsonParseError parse_error{};
        QJsonDocument doc = QJsonDocument::fromJson(*response, &parse_error);
        if (parse_error.error != QJsonParseError::NoError) {
            qWarning() << "Error while parsing JSON response for getting versions at " << parse_error.offset
                       << " reason: " << parse_error.errorString();
            qWarning() << *response;
            return;
        }

        QVector<ModPlatform::IndexedVersion> unsortedVersions;
        try {
            //qDebug() << "[HangarAPI] getProjectVersions - Raw response:" << QString::fromUtf8(*response).left(500);

            // Hangar returns {pagination, result: [...]}, extract result array
            QJsonArray arr;
            if (doc.isObject()) {
                auto obj = doc.object();
                //qDebug() << "[HangarAPI] getProjectVersions - Response keys:" << obj.keys();
                if (obj.contains("result")) {
                    arr = obj["result"].toArray();
                    //qDebug() << "[HangarAPI] getProjectVersions - Found" << arr.size() << "versions in result array";
                } else {
                    //qDebug() << "[HangarAPI] getProjectVersions - No 'result' key found!";
                }
            }

            for (auto versionIter : arr) {
                auto obj = versionIter.toObject();

                auto file = loadIndexedPackVersion(obj, args.resourceType);
                if (!file.addonId.isValid())
                    file.addonId = args.pack->addonId;

                // Select best platform based on user's plugin loaders + compatibility
                if (file.fileId.isValid() && args.pluginLoaders.has_value()) {
                    auto selectedPlatform = selectBestPlatform(obj, args.pluginLoaders.value());
                    if (!selectedPlatform.isEmpty()) {
                        auto downloads = optObject(obj, "downloads");
                        auto platformDownload = optObject(downloads, selectedPlatform);
                        if (!platformDownload.isEmpty()) {
                            // Most plugins use externalUrl (Modrinth/external CDN)
                            file.downloadUrl = optString(platformDownload, "externalUrl");
                            if (file.downloadUrl.isEmpty()) {
                                file.downloadUrl = optString(platformDownload, "downloadUrl");
                            }

                            auto fileInfo = optObject(platformDownload, "fileInfo");
                            if (!fileInfo.isEmpty()) {
                                file.fileName = optString(fileInfo, "name");
                                file.hash = optString(fileInfo, "sha256Hash");
                                file.hash_type = "sha256";
                            } else if (!file.downloadUrl.isEmpty()) {
                                QUrl url(file.downloadUrl);
                                file.fileName = url.fileName();
                                if (file.fileName.isEmpty()) {
                                    file.fileName = QString("%1.jar").arg(file.version);
                                }
                            }
                        }
                    }
                }

                if (file.fileId.isValid() && !file.downloadUrl.isEmpty())
                    unsortedVersions.append(file);
            }

            auto orderSortPredicate = [](const ModPlatform::IndexedVersion& a, const ModPlatform::IndexedVersion& b) -> bool {
                return a.date > b.date;
            };
            std::sort(unsortedVersions.begin(), unsortedVersions.end(), orderSortPredicate);
        } catch (const JSONValidationError& e) {
            qDebug() << doc;
            qWarning() << "Error while reading Hangar resource version: " << e.cause();
        }

        callbacks.on_succeed(unsortedVersions);
    });

    auto weak = netJob.toWeakRef();
    QObject::connect(netJob.get(), &NetJob::failed, [weak, callbacks](const QString& reason) {
        int network_error_code = -1;
        if (auto netJob = weak.lock()) {
            if (auto* failed_action = netJob->getFailedActions().at(0); failed_action)
                network_error_code = failed_action->replyStatusCode();
        }
        callbacks.on_fail(reason, network_error_code);
    });
    QObject::connect(netJob.get(), &NetJob::aborted, [callbacks] {
        if (callbacks.on_abort != nullptr)
            callbacks.on_abort();
    });

    return netJob;
}
