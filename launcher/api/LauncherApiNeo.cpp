// SPDX-License-Identifier: GPL-3.0-only

#include "LauncherApiNeo.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include <utility>

#include "LauncherApi.h"
#include "LauncherApiSupport.h"
#include "AsyncTaskRuntime.h"

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
        objectSchema({ { "operation", stringProperty("instance.import, instance.create, instance.component.set-version, instance.special-loader.install, instance.update, java.install, modpack.install, resource.install, resource.install-version, resource.install-with-dependencies, account.refresh, account.login, instance.launch, instance.export, or server.start.") },
                       { "parameters", QJsonObject{ { "type", "object" }, { "description", "Operation parameters." } } },
                       { "priority", stringProperty("normal or high.") } }, { "operation", "parameters" }),
        "neo", true },
        [](const QJsonObject& p, UserInteraction& interaction) {
            auto parameters = p.value("parameters").toObject();
            if (p.contains("idempotencyKey") && !parameters.contains("idempotencyKey"))
                parameters.insert("idempotencyKey", p.value("idempotencyKey"));
            if (p.contains("priority") && !parameters.contains("priority"))
                parameters.insert("priority", p.value("priority"));
            return AsyncTaskRuntime::instance().start(p.value("operation").toString(), parameters, interaction);
        });

    api.registerOperation({ "neo.task.status", "Read one asynchronous Neo task snapshot.", objectSchema({ { "taskId", stringProperty("Neo task id.") } }, { "taskId" }), "neo" },
        [](const QJsonObject& p, UserInteraction&) { return AsyncTaskRuntime::instance().status(p.value("taskId").toString()); });

    api.registerOperation({ "neo.task.list", "List recent asynchronous Neo tasks.", objectSchema({}), "neo" },
        [](const QJsonObject&, UserInteraction&) { return AsyncTaskRuntime::instance().list(); });

    api.registerOperation({ "neo.task.cancel", "Cancel one asynchronous Neo task.", objectSchema({ { "taskId", stringProperty("Neo task id.") } }, { "taskId" }), "neo", true },
        [](const QJsonObject& p, UserInteraction&) { return AsyncTaskRuntime::instance().cancel(p.value("taskId").toString()); });

    api.registerOperation({ "neo.task.respond", "Answer a pending task interaction.",
        objectSchema({ { "taskId", stringProperty("Neo task id.") }, { "interactionId", stringProperty("Pending interaction id.") },
                       { "value", QJsonObject{} }, { "cancel", QJsonObject{ { "type", "boolean" } } } },
                     { "taskId", "interactionId" }), "neo", true },
        [](const QJsonObject& p, UserInteraction&) { return AsyncTaskRuntime::instance().respond(p); });

    api.registerOperation({ "neo.task.capabilities", "Describe the asynchronous Neo task API and conflict policy.", objectSchema({}), "neo" },
        [](const QJsonObject&, UserInteraction&) {
            return QJsonObject{
                { "ok", true },
                { "data", QJsonObject{
                    { "parallel", true },
                    { "maxActive", AsyncTaskRuntime::maxActive },
                    { "operations", QJsonArray{ "instance.import", "instance.create", "instance.component.set-version", "instance.special-loader.install", "instance.update", "java.install", "modpack.install", "resource.install", "resource.install-version", "resource.install-with-dependencies", "account.refresh", "account.login", "instance.launch", "instance.export", "server.start" } },
                    { "conflictPolicy", "instance.import/instance.create/modpack.install use the global 'instances' key; resource.install, resource.install-version, resource.install-with-dependencies, instance.update and component updates use 'instance:<id>'; java.install uses 'java'; account.refresh uses 'account:<id>'; account.login uses 'accounts'; instance.launch, instance.export and server.start use 'instance:<id>'. instance.import, modpack.install, resource.install-version, resource.install-with-dependencies and instance.special-loader.install also share the 'interactive-install' key to isolate their interaction channels. Tasks without shared keys run concurrently." },
                } }
            };
        });
}
