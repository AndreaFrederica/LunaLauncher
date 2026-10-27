// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
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
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *      Copyright 2013-2021 MultiMC Contributors
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#pragma once

#include "InstanceCreationTask.h"

#include <optional>

#include "minecraft/MinecraftInstance.h"

#include "modplatform/BlockedMod.h"
#include "modplatform/flame/FileResolvingTask.h"

#include "net/NetJob.h"

class FlameCreationTask final : public InstanceCreationTask {
    Q_OBJECT

   public:
    FlameCreationTask(const QString& staging_path,
                      SettingsObject* global_settings,
                      QWidget* parent,
                      QString id,
                      QString version_id,
                      QString original_instance_id = {})
        : InstanceCreationTask(), m_parent(parent), m_managedId(std::move(id)), m_managedVersionId(std::move(version_id))
    {
        setStagingPath(staging_path);
        setParentSettings(global_settings);

        m_original_instance_id = std::move(original_instance_id);
    }

    bool abort() override;
    QList<Task*> getSubTasks() const override;

    bool updateInstance() override;
    std::unique_ptr<BaseInstance> createInstance() override;

   private slots:
    void idResolverSucceeded(QEventLoop&);
    void setupDownloadJob(QEventLoop&);
    void copyBlockedMods(const QList<BlockedMod>& blocked_mods);
    void validateOtherResources(QEventLoop& loop);
    QString getVersionForLoader(QString uid, QString loaderType, QString version, QString mcVersion);

   private:
    /// True when the file already exists in the staging area with the expected size.
    bool isFileAlreadyPresent(const Flame::File& file, const QString& path) const;
    /// Looks for an already downloaded copy of a Modrinth-fallback file in the
    /// user's download / central mods folders. Returns an empty string if none.
    QString findLocalCopy(const Flame::File& file) const;
    /// Offers the failed downloads as blocked mods (manual download) and retries once.
    void onDownloadFailed(const QString& reason, QEventLoop& loop);

   private:
    QWidget* m_parent = nullptr;

    shared_qobject_ptr<Flame::FileResolvingTask> m_modIdResolver;
    Flame::Manifest m_pack;

    // Handle to allow aborting
    Task::Ptr m_processUpdateFileInfoJob = nullptr;
    NetJob::Ptr m_filesJob = nullptr;

    QString m_managedId, m_managedVersionId;

    QList<std::pair<QString, QString>> m_otherResources;

    std::optional<BaseInstance*> m_instance;

    QStringList m_selectedOptionalMods;

    /// Set once the failed downloads were handed to the user, so a second
    /// failure fails the import instead of showing the dialog again.
    bool m_blockedFallbackUsed = false;
};
