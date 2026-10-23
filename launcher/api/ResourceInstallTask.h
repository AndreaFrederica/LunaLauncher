// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QQueue>
#include <QSet>
#include <QStringList>
#include <memory>

#include "tasks/Task.h"
#include "modplatform/ModIndex.h"
#include "modplatform/ResourceAPI.h"
#include "modplatform/ResourceType.h"

class BaseInstance;
class MinecraftInstance;
class ResourceAPI;
class ResourceFolderModel;

/**
 * Resolves and installs a resource plus its required/optional dependency graph
 * using the provider's asynchronous metadata API. The actual downloads are
 * executed by ResourceDownloadTask inside a SequentialTask.
 */
class ResourceInstallTask final : public Task {
    Q_OBJECT

   public:
    ResourceInstallTask(BaseInstance* instance,
                        QString kind,
                        QString provider,
                        QString projectId,
                        QString versionId,
                        bool includeOptional,
                        QString minecraftVersion,
                        QStringList loaders);
    ~ResourceInstallTask() override = default;

    void executeTask() override;
    bool abort() override;
    bool canAbort() const override { return true; }

   private:
    void fetchProject();
    void fetchVersions();
    void resolveNextDependency();
    void installVersions();
    void fail(const QString& message);
    ModPlatform::IndexedPack::Ptr makePack(const QVariant& addonId, const QString& name = {}) const;
    ModPlatform::ResourceType resourceType() const;
    ModPlatform::ModLoaderTypes loaderFlags() const;

    BaseInstance* m_instance = nullptr;
    MinecraftInstance* m_minecraft = nullptr;
    ResourceFolderModel* m_model = nullptr;
    QString m_kind;
    QString m_provider;
    QString m_projectId;
    QString m_versionId;
    QString m_minecraftVersion;
    QStringList m_loaders;
    bool m_includeOptional = false;
    bool m_aborted = false;

    std::unique_ptr<ResourceAPI> m_api;
    ModPlatform::IndexedPack::Ptr m_rootPack;
    ModPlatform::IndexedVersion m_rootVersion;
    QQueue<ModPlatform::Dependency> m_dependencies;
    QSet<QString> m_visited;
    QList<QPair<ModPlatform::IndexedPack::Ptr, ModPlatform::IndexedVersion>> m_install;
    Task::Ptr m_current;
};
