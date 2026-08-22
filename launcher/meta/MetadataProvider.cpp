// SPDX-License-Identifier: GPL-3.0-only

#include "MetadataProvider.h"

#include "CleanroomMetadataProvider.h"
#include "Application.h"
#include "meta/Index.h"
#include "meta/VersionList.h"

namespace Meta {

static QList<std::shared_ptr<MetadataProvider>> s_providers;
static QList<Task::Ptr> s_activeProviderTasks;

void registerBuiltinProviders()
{
    s_activeProviderTasks.clear();
    s_providers.clear();
    s_providers.push_back(std::make_shared<CleanroomMetadataProvider>());

    // Pre-register provider UIDs and load their lists so component controls
    // can become available without querying the default Prism metadata API.
    auto* index = APPLICATION->metadataIndex();
    for (const auto& provider : s_providers) {
        for (const auto& uid : provider->knownUids()) {
            auto list = index->get(uid);
            if (!list->isLoaded()) {
                auto task = provider->loadVersionListTask(list.get());
                auto* taskPtr = task.get();
                s_activeProviderTasks.append(task);
                QObject::connect(taskPtr, &Task::finished, taskPtr, [taskPtr] {
                    s_activeProviderTasks.removeIf([taskPtr](const Task::Ptr& activeTask) { return activeTask.get() == taskPtr; });
                });
                task->start();
            }
        }
    }
}

const QList<std::shared_ptr<MetadataProvider>>& builtinProviders()
{
    return s_providers;
}

MetadataProvider* findProviderForUid(const QString& uid)
{
    for (const auto& provider : s_providers) {
        if (provider->handlesUid(uid)) {
            return provider.get();
        }
    }
    return nullptr;
}

}  // namespace Meta
