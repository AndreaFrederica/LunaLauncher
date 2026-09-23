// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "minecraft/MinecraftInstance.h"
#include "minecraft/mod/DataPackFolderModel.h"
#include "minecraft/mod/Mod.h"
#include "api/LauncherApiObjects.h"
#include <QBuffer>
#include <QEventLoop>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QPixmap>
#include <QTimer>

namespace ApiSupport {
inline QString resourceTypeName(ResourceType type)
{
    switch (type) {
        case ResourceType::ZIPFILE: return "archive";
        case ResourceType::SINGLEFILE: return "file";
        case ResourceType::FOLDER: return "folder";
        case ResourceType::LITEMOD: return "litemod";
        case ResourceType::UNKNOWN: return "unknown";
    }
    return "unknown";
}

inline QString resourceStatusName(ResourceStatus status)
{
    switch (status) {
        case ResourceStatus::INSTALLED: return "installed";
        case ResourceStatus::NOT_INSTALLED: return "not-installed";
        case ResourceStatus::NO_METADATA: return "no-metadata";
        case ResourceStatus::UNKNOWN: return "unknown";
    }
    return "unknown";
}

// Keep the transport shape shared by resource.list and resource.inspect.  The
// Qt models already parse mod loader manifests and indexed metadata; using the
// model objects here avoids a second, incomplete archive parser in the API.
inline QJsonObject resourceSnapshot(const Resource& resource)
{
    auto result = LauncherApiModel::ResourceObject(const_cast<Resource*>(&resource)).snapshot();
    result.insert("type", resourceTypeName(resource.type()));
    result.insert("status", resourceStatusName(resource.status()));
    QJsonArray issues;
    for (const auto& issue : resource.issues()) issues.append(issue);
    result.insert("issues", issues);

    if (const auto metadata = resource.metadata()) {
        QJsonObject metadataObject{ { "slug", metadata->slug }, { "name", metadata->name },
                                    { "filename", metadata->filename }, { "side", static_cast<int>(metadata->side) },
                                    { "mode", metadata->mode }, { "url", metadata->url.toString() },
                                    { "hashFormat", metadata->hash_format }, { "hash", metadata->hash },
                                    { "provider", static_cast<int>(metadata->provider) },
                                    { "projectId", QJsonValue::fromVariant(metadata->project_id) },
                                    { "fileId", QJsonValue::fromVariant(metadata->file_id) },
                                    { "version", metadata->version_number }, { "releaseType", metadata->releaseType.toString() } };
        QJsonArray versions;
        for (const auto& version : metadata->mcVersions) versions.append(version);
        metadataObject.insert("minecraftVersions", versions);
        QJsonArray dependencies;
        for (const auto& dependency : metadata->dependencies)
            dependencies.append(QJsonObject{ { "addonId", QJsonValue::fromVariant(dependency.addonId) },
                                             { "type", static_cast<int>(dependency.type) }, { "version", dependency.version } });
        metadataObject.insert("dependencies", dependencies);
        result.insert("metadata", metadataObject);
    }

    if (const auto* mod = dynamic_cast<const Mod*>(&resource)) {
        result.insert("modId", mod->mod_id());
        result.insert("version", mod->version());
        result.insert("description", mod->description());
        result.insert("authors", QJsonArray::fromStringList(mod->authors()));
        result.insert("homepage", mod->homepage());
        result.insert("issueTracker", mod->issueTracker());
        result.insert("side", mod->side());
        result.insert("loaders", mod->loaders());
        result.insert("minecraftVersions", mod->mcVersions());
        result.insert("releaseType", mod->releaseType());
        result.insert("dependencies", QJsonArray::fromStringList(mod->dependencies()));
        result.insert("resolved", mod->isResolved());
        result.insert("resolving", mod->isResolving());
        const auto icon = mod->icon({ 96, 96 }, Qt::KeepAspectRatio);
        if (!icon.isNull()) {
            QByteArray bytes;
            QBuffer buffer(&bytes);
            if (buffer.open(QIODevice::WriteOnly) && icon.toImage().save(&buffer, "PNG"))
                result.insert("icon", QStringLiteral("data:image/png;base64,") + QString::fromLatin1(bytes.toBase64()));
        }
    }
    return result;
}

inline void ensureResourceModelReady(const std::shared_ptr<ResourceFolderModel>& model)
{
    if (!model) return;
    QEventLoop updateLoop;
    QObject::connect(model.get(), &ResourceFolderModel::updateFinished, &updateLoop, &QEventLoop::quit);
    if (model->update()) updateLoop.exec();
    if (model->hasPendingParseTasks()) {
        QEventLoop parseLoop;
        QTimer timeout;
        timeout.setSingleShot(true);
        QObject::connect(&timeout, &QTimer::timeout, &parseLoop, &QEventLoop::quit);
        QObject::connect(model.get(), &ResourceFolderModel::parseFinished, &parseLoop, [&parseLoop, model] {
            if (!model->hasPendingParseTasks()) parseLoop.quit();
        });
        timeout.start(3000);
        if (model->hasPendingParseTasks()) parseLoop.exec();
    }
}

// World folders are names returned by instance.world.list, never arbitrary paths.
inline std::shared_ptr<ResourceFolderModel> worldDataPacks(BaseInstance* instance, const QString& kind, const QString& world)
{
    if (kind != "datapacks" || world.isEmpty() || world == "." || world == ".." ||
        world.contains('/') || world.contains('\\') || world.contains(':') || !dynamic_cast<MinecraftInstance*>(instance)) return {};
    const QFileInfo saves(QDir(instance->gameRoot()).filePath("saves"));
    const QFileInfo folder(QDir(saves.absoluteFilePath()).filePath(world));
    if (!saves.isDir() || saves.isSymLink() || !folder.isDir() || folder.isSymLink() ||
        QFileInfo(folder.canonicalFilePath()).absolutePath() != saves.canonicalFilePath()) return {};
    const QFileInfo packs(QDir(folder.absoluteFilePath()).filePath("datapacks"));
    if (packs.isSymLink() || (packs.exists() && !packs.isDir())) return {};
    return std::make_shared<DataPackFolderModel>(packs.absoluteFilePath(), instance, true, true);
}
}
