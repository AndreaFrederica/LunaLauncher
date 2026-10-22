// SPDX-License-Identifier: GPL-3.0-only

#include "LauncherApiNeo.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include <utility>

#include "LauncherApi.h"
#include "LauncherApiSupport.h"
#include "NeoTaskService.h"

namespace {

QJsonObject stringProperty(const QString& description)
{
    return ApiSupport::string(description);
}

QJsonObject objectSchema(QJsonObject properties, QJsonArray required = {})
{
    return ApiSupport::schema(std::move(properties), std::move(required));
}

}  // namespace

void registerLauncherApiNeoOperations(LauncherApi& api)
{
    api.registerOperation({ "neo.task.start",
        "Start an asynchronous Neo UI task immediately. Independent instance keys run in parallel; conflicting keys queue.",
        objectSchema({ { "operation", stringProperty("instance.import, instance.create, or resource.install.") },
                       { "parameters", QJsonObject{ { "type", "object" }, { "description", "Operation parameters." } } },
                       { "priority", stringProperty("normal or high.") } }, { "operation", "parameters" }),
        "neo", true },
        [](const QJsonObject& p, UserInteraction& interaction) {
            return NeoTaskService::instance().start(p.value("operation").toString(), p.value("parameters").toObject(), interaction);
        });

    api.registerOperation({ "neo.task.status", "Read one asynchronous Neo task snapshot.", objectSchema({ { "taskId", stringProperty("Neo task id.") } }, { "taskId" }), "neo" },
        [](const QJsonObject& p, UserInteraction&) { return NeoTaskService::instance().status(p.value("taskId").toString()); });

    api.registerOperation({ "neo.task.list", "List recent asynchronous Neo tasks.", objectSchema({}), "neo" },
        [](const QJsonObject&, UserInteraction&) { return NeoTaskService::instance().list(); });

    api.registerOperation({ "neo.task.cancel", "Cancel one asynchronous Neo task.", objectSchema({ { "taskId", stringProperty("Neo task id.") } }, { "taskId" }), "neo", true },
        [](const QJsonObject& p, UserInteraction&) { return NeoTaskService::instance().cancel(p.value("taskId").toString()); });

    api.registerOperation({ "neo.task.capabilities", "Describe the asynchronous Neo task API and conflict policy.", objectSchema({}), "neo" },
        [](const QJsonObject&, UserInteraction&) {
            return QJsonObject{
                { "ok", true },
                { "data", QJsonObject{
                    { "parallel", true },
                    { "maxActive", 3 },
                    { "operations", QJsonArray{ "instance.import", "instance.create", "resource.install" } },
                    { "conflictPolicy", "instance.import/instance.create use the global 'instances' key; resource.install uses 'instance:<id>'. Different keys run concurrently." },
                } }
            };
        });
}
