#pragma once

#include "modplatform/CheckUpdateTask.h"
#include "modplatform/flame/FlameAPI.h"

class FlameCheckUpdate : public CheckUpdateTask {
    Q_OBJECT

   public:
    FlameCheckUpdate(QList<Resource*>& resources,
                     std::list<Version>& mcVersions,
                     QList<ModPlatform::ModLoaderType> loadersList,
                     std::shared_ptr<ResourceFolderModel> resourceModel,
                     ModPlatform::ResourceProvider provider = ModPlatform::ResourceProvider::FLAME);

   public slots:
    bool abort() override;

   protected slots:
    void executeTask() override;
   private slots:
    void getLatestVersionCallback(Resource* resource, std::shared_ptr<QByteArray> response);
    void collectBlockedMods();

   private:
    Task::Ptr m_task = nullptr;
    ModPlatform::ResourceProvider m_provider;
    FlameAPI m_api;

    QHash<Resource*, QString> m_blocked;
};
