// SPDX-License-Identifier: GPL-3.0-only

#include "LauncherApiCustomUi.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include <utility>

#include "Application.h"
#include "BaseInstance.h"
#include "InstanceList.h"
#include "LauncherApi.h"
#include "LauncherApiSupport.h"
#include "CustomUiRuntime.h"
#include "cli/OperationService.h"
#include "minecraft/MinecraftInstance.h"

namespace {

QJsonObject stringProperty(const QString& description)
{
    return ApiSupport::string(description);
}

QJsonObject objectSchema(QJsonObject properties, QJsonArray required = {})
{
    return ApiSupport::schema(std::move(properties), std::move(required));
}

MinecraftInstance* findMinecraftInstance(const QString& reference)
{
    auto list = APPLICATION->instances();
    auto instance = list->getInstanceById(reference);
    if (!instance) instance = list->getInstanceByManagedName(reference);
    if (!instance) {
        for (int i = 0; i < list->count(); ++i) {
            if (list->at(i)->name().compare(reference, Qt::CaseInsensitive) == 0) {
                instance = list->at(i);
                break;
            }
        }
    }
    return dynamic_cast<MinecraftInstance*>(instance);
}

}  // namespace

void registerLauncherApiCustomUiOperations(LauncherApi& api)
{
    const auto instance = stringProperty("Minecraft instance id, managed name, or display name.");
    const auto control = QJsonObject{ { "type", "object" }, { "description", "Control object from snapshot.tabs[].controls[]." } };
    const auto value = QJsonObject{ { "description", "New control value (string, number, boolean, or array)." } };

    api.registerOperation({ "instance.custom-ui.snapshot",
        "Load the instance custom UI JSON/JS panel and return tabs, controls, state and errors.",
        objectSchema({ { "instance", instance } }, { "instance" }), "instances" },
        [](const QJsonObject& p, UserInteraction&) {
            auto target = findMinecraftInstance(p.value("instance").toString());
            if (!target) return OperationService::failure("The Minecraft instance does not exist.", 2);
            CustomUiRuntime runtime(target);
            const bool loaded = runtime.load();
            auto result = runtime.snapshot();
            result["loaded"] = loaded;
            return OperationService::success(result);
        });

    api.registerOperation({ "instance.custom-ui.trigger",
        "Apply one custom UI control value; actions and handlers run inside the instance JS runtime.",
        objectSchema({ { "instance", instance }, { "control", control }, { "value", value } }, { "instance", "control" }), "instances", true },
        [](const QJsonObject& p, UserInteraction&) {
            auto target = findMinecraftInstance(p.value("instance").toString());
            if (!target) return OperationService::failure("The Minecraft instance does not exist.", 2);
            if (!p.value("control").isObject()) return OperationService::failure("control must be an object.", 2);
            CustomUiRuntime runtime(target);
            runtime.load();
            const auto triggered = runtime.trigger(p.value("control").toObject(), p.value("value"));
            auto result = runtime.snapshot();
            result["triggered"] = triggered;
            return OperationService::success(result);
        });

    api.registerOperation({ "instance.custom-ui.save",
        "Persist the current custom UI state file.",
        objectSchema({ { "instance", instance } }, { "instance" }), "instances", true },
        [](const QJsonObject& p, UserInteraction&) {
            auto target = findMinecraftInstance(p.value("instance").toString());
            if (!target) return OperationService::failure("The Minecraft instance does not exist.", 2);
            CustomUiRuntime runtime(target);
            runtime.load();
            const auto saved = runtime.persist();
            auto result = runtime.snapshot();
            result["saved"] = saved;
            return OperationService::success(result);
        });

    api.registerOperation({ "instance.custom-ui.reload",
        "Reload custom UI tabs from disk.",
        objectSchema({ { "instance", instance } }, { "instance" }), "instances" },
        [](const QJsonObject& p, UserInteraction&) {
            auto target = findMinecraftInstance(p.value("instance").toString());
            if (!target) return OperationService::failure("The Minecraft instance does not exist.", 2);
            CustomUiRuntime runtime(target);
            const bool loaded = runtime.load();
            auto result = runtime.snapshot();
            result["loaded"] = loaded;
            return OperationService::success(result);
        });
}
