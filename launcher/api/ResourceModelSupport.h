// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "minecraft/MinecraftInstance.h"
#include "minecraft/mod/DataPackFolderModel.h"
#include <QFileInfo>

namespace ApiSupport {
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
