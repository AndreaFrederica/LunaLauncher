// SPDX-License-Identifier: GPL-3.0-only

#include "LauncherApi.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include <QList>
#include <QSet>
#include <QScopedValueRollback>
#include <QStringList>

#include "cli/OperationService.h"
#include "api/LauncherApiInstances.h"
#include "api/LauncherApiDomains.h"
#include "api/LauncherApiFiles.h"
#include "api/LauncherApiResources.h"
#include "api/LauncherApiServer.h"
#include "api/LauncherApiExports.h"
#include "api/LauncherApiComponents.h"
#include "api/LauncherApiCatalog.h"
#include "api/LauncherApiIntegrations.h"
#include "api/LauncherApiAppearance.h"
#include "api/LauncherApiStreams.h"
#include "tasks/Task.h"

namespace {

QString validateInput(const QJsonValue& value, const QJsonObject& schema, const QString& path = "parameters")
{
    const auto type = schema.value("type").toString();
    if ((!type.isEmpty() && type == "object" && !value.isObject()) ||
        (type == "array" && !value.isArray()) || (type == "string" && !value.isString()) ||
        (type == "boolean" && !value.isBool()) || (type == "number" && !value.isDouble()) ||
        (type == "integer" && (!value.isDouble() || std::floor(value.toDouble()) != value.toDouble())))
        return QStringLiteral("%1 must be %2.").arg(path, type);
    if (value.isDouble() && ((schema.contains("minimum") && value.toDouble() < schema.value("minimum").toDouble()) ||
                             (schema.contains("maximum") && value.toDouble() > schema.value("maximum").toDouble())))
        return QStringLiteral("%1 is outside its permitted range.").arg(path);
    if (value.isObject()) {
        const auto object = value.toObject();
        for (const auto& key : schema.value("required").toArray())
            if (!object.contains(key.toString())) return QStringLiteral("%1.%2 is required.").arg(path, key.toString());
        const auto properties = schema.value("properties").toObject();
        for (auto it = properties.begin(); it != properties.end(); ++it) {
            if (!object.contains(it.key())) continue;
            const auto error = validateInput(object.value(it.key()), it.value().toObject(), path + '.' + it.key());
            if (!error.isEmpty()) return error;
        }
    }
    if (value.isArray() && schema.value("items").isObject()) {
        int i = 0;
        for (const auto& item : value.toArray()) {
            const auto error = validateInput(item, schema.value("items").toObject(), QStringLiteral("%1[%2]").arg(path).arg(i++));
            if (!error.isEmpty()) return error;
        }
    }
    return {};
}

QJsonObject stringProperty(const QString& description)
{
    return { { "type", "string" }, { "description", description } };
}

QJsonObject boolProperty(const QString& description, bool defaultValue = false)
{
    return { { "type", "boolean" }, { "description", description }, { "default", defaultValue } };
}

QJsonObject objectSchema(QJsonObject properties, QJsonArray required = {})
{
    QJsonObject schema{ { "type", "object" }, { "properties", properties }, { "additionalProperties", false } };
    if (!required.isEmpty())
        schema.insert("required", required);
    return schema;
}

QString taskStateName(Task::State state)
{
    switch (state) {
        case Task::State::Inactive:
            return QStringLiteral("inactive");
        case Task::State::Running:
            return QStringLiteral("running");
        case Task::State::Succeeded:
            return QStringLiteral("succeeded");
        case Task::State::Failed:
            return QStringLiteral("failed");
        case Task::State::AbortedByUser:
            return QStringLiteral("aborted");
    }
    return QStringLiteral("unknown");
}

QString taskStepStateName(TaskStepState state)
{
    switch (state) {
        case TaskStepState::Waiting:
            return QStringLiteral("waiting");
        case TaskStepState::Running:
            return QStringLiteral("running");
        case TaskStepState::Failed:
            return QStringLiteral("failed");
        case TaskStepState::Succeeded:
            return QStringLiteral("succeeded");
    }
    return QStringLiteral("unknown");
}

QList<ApiOperation> legacyOperations()
{
    const auto instance = stringProperty("Instance ID, managed name, or display name.");
    const auto account = stringProperty("Account ID or profile name.");
    const auto resource = QJsonObject{ { "instance", instance }, { "kind", stringProperty("Resource kind.") },
                                  { "resource", stringProperty("Resource ID, name, or file name.") } };
    auto removableResource = resource;
    removableResource.insert("confirm", boolProperty("Confirm removal."));
    QList<ApiOperation> operations{ { "instance.list", "List installed instances.", objectSchema({}) },
             { "instance.info", "Read an installed instance.", objectSchema({ { "instance", instance } }, { "instance" }) },
             { "instance.rename", "Rename an installed instance.", objectSchema({ { "instance", instance }, { "name", stringProperty("New name.") } }, { "instance", "name" }) },
             { "instance.group", "Move an instance to a group.", objectSchema({ { "instance", instance }, { "group", stringProperty("Group, or empty to clear.") } }, { "instance", "group" }) },
             { "instance.copy", "Copy an installed instance.", objectSchema({ { "instance", instance }, { "name", stringProperty("Name for the copy.") }, { "group", stringProperty("Optional group.") }, { "icon", stringProperty("Optional icon key.") } }, { "instance", "name" }) },
             { "instance.update", "Run an instance update.", objectSchema({ { "instance", instance } }, { "instance" }) },
             { "instance.delete", "Trash or permanently delete an instance.", objectSchema({ { "instance", instance }, { "confirm", boolProperty("Confirm deletion.") }, { "permanent", boolProperty("Delete permanently.") }, { "force", boolProperty("Ignore linked instances.") } }, { "instance", "confirm" }), {}, true },
             { "instance.undo-delete", "Restore the most recently trashed instance.", objectSchema({}) },
             { "account.list", "List launcher accounts.", objectSchema({}) },
             { "account.login", "Add a launcher account.", objectSchema({ { "type", stringProperty("microsoft, offline, yggdrasil, or unified-pass.") }, { "username", stringProperty("Username.") }, { "password", stringProperty("Password.") }, { "authUrl", stringProperty("Yggdrasil auth URL.") }, { "sessionUrl", stringProperty("Yggdrasil session URL.") } }, { "type" }) },
             { "account.set-default", "Set or clear the default account.", objectSchema({ { "account", account } }, { "account" }) },
             { "account.refresh", "Refresh an account.", objectSchema({ { "account", account } }, { "account" }) },
             { "account.remove", "Remove an account.", objectSchema({ { "account", account }, { "confirm", boolProperty("Confirm removal.") } }, { "account", "confirm" }), {}, true },
             { "instance.import", "Import an instance pack.", objectSchema({ { "source", stringProperty("Local path or URL.") }, { "name", stringProperty("Optional name.") } }, { "source" }) },
             { "instance.launch", "Launch an instance.", objectSchema({ { "instance", instance }, { "profile", stringProperty("Account profile.") }, { "offlineName", stringProperty("Offline player name.") }, { "server", stringProperty("Server to join.") }, { "world", stringProperty("World to join.") }, { "wait", boolProperty("Wait for exit.") } }, { "instance" }) },
             { "resource.list", "List installed resources.", objectSchema({ { "instance", instance }, { "kind", stringProperty("Resource kind.") } }, { "instance", "kind" }) },
             { "resource.install", "Install a resource from a path or direct URL.", objectSchema({ { "instance", instance }, { "kind", stringProperty("Resource kind.") }, { "source", stringProperty("Path or URL.") } }, { "instance", "kind", "source" }) },
             { "resource.enable", "Enable an installed resource.", objectSchema(resource, { "instance", "kind", "resource" }) },
             { "resource.disable", "Disable an installed resource.", objectSchema(resource, { "instance", "kind", "resource" }) },
             { "resource.remove", "Remove an installed resource.", objectSchema(removableResource, { "instance", "kind", "resource", "confirm" }), {}, true },
             { "java.list", "List available Java installations.", objectSchema({}) },
             { "settings.list", "List registered settings.", objectSchema({ { "scope", stringProperty("launcher or instance." ) }, { "instance", instance }, { "filter", stringProperty("Optional key filter.") }, { "reveal", boolProperty("Reveal sensitive values.") } }, { "scope" }) },
             { "settings.get", "Read a registered setting.", objectSchema({ { "scope", stringProperty("launcher or instance." ) }, { "instance", instance }, { "key", stringProperty("Setting ID.") }, { "reveal", boolProperty("Reveal sensitive value.") } }, { "scope", "key" }) },
             { "settings.set", "Set a registered setting.", objectSchema({ { "scope", stringProperty("launcher or instance." ) }, { "instance", instance }, { "key", stringProperty("Setting ID.") }, { "value", QJsonObject{ { "description", "JSON value." } } } }, { "scope", "key", "value" }) },
             { "settings.reset", "Reset a registered setting.", objectSchema({ { "scope", stringProperty("launcher or instance." ) }, { "instance", instance }, { "key", stringProperty("Setting ID.") } }, { "scope", "key" }) },
             { "settings.export", "Export registered settings as a JSON values object.", objectSchema({ { "scope", stringProperty("launcher or instance." ) }, { "instance", instance }, { "reveal", boolProperty("Include sensitive values.") } }, { "scope" }) },
             { "settings.import", "Import a JSON values object into registered settings.", objectSchema({ { "scope", stringProperty("launcher or instance." ) }, { "instance", instance }, { "values", QJsonObject{ { "type", "object" } } }, { "allowSensitive", boolProperty("Allow sensitive values in the import.") } }, { "scope", "values" }) } };
    for (auto& operation : operations) {
        auto properties = operation.inputSchema.value("properties").toObject();
        if (operation.name == "instance.copy") {
            for (const auto key : { "copySaves", "keepPlaytime", "copyMods", "copyResourcePacks", "copyShaderPacks", "copyScreenshots" })
                properties.insert(key, QJsonObject{ { "type", "boolean" } });
        }
        if (operation.name == "account.login" || operation.name == "instance.launch") {
            for (const auto key : { "profileId", "username", "password" })
                properties.insert(key, stringProperty(QString::fromLatin1(key)));
        }
        if (operation.name == "account.login") {
            for (const auto key : { "sourceName", "serverId", "minecraftProfileName" })
                properties.insert(key, stringProperty(QString::fromLatin1(key)));
        }
        if (operation.name.startsWith("settings."))
            properties.insert("reveal", boolProperty("Reveal sensitive values."));
        if (operation.name == "resource.remove")
            properties.insert("preserveMetadata", boolProperty("Keep indexed metadata."));
        operation.inputSchema.insert("properties", properties);
    }
    return operations;

}

}  // namespace

LauncherApi::LauncherApi(QObject* parent) : QObject(parent), m_legacyService(new OperationService(this))
{
    connect(m_legacyService, &OperationService::taskStarted, this, &LauncherApi::trackTask);
    connect(m_legacyService, &OperationService::taskFinished, this, &LauncherApi::clearTrackedTask);
    for (const auto& operation : legacyOperations()) {
        const auto name = operation.name;
        registerOperation(operation, [this, name](const QJsonObject& parameters, UserInteraction& interaction) {
            return m_legacyService->execute(name, parameters, interaction);
        });
    }
    registerOperation({ "api.describe", "Describe all operations and their input schemas." },
                      [this](const QJsonObject&, UserInteraction&) { return OperationService::success(describe()); });
    registerOperation({ "task.list",
                        "List tasks tracked by the launcher API, including completed task snapshots.",
                        objectSchema({ { "runningOnly", boolProperty("Only return tasks that are currently running.") } }),
                        "tasks" },
                      [this](const QJsonObject& parameters, UserInteraction&) { return taskList(parameters); });
    registerOperation({ "task.status",
                        "Read progress, status, and cancellation capabilities for a tracked task.",
                        objectSchema({ { "taskId", stringProperty("Task UUID. Omit to inspect the current operation task.") } }),
                        "tasks" },
                      [this](const QJsonObject& parameters, UserInteraction&) { return taskStatus(parameters); });
    registerOperation({ "task.cancel",
                        "Request cancellation of a running task.",
                        objectSchema({ { "taskId", stringProperty("Task UUID. Omit to cancel the current operation task.") } }),
                        "tasks",
                        true },
                      [this](const QJsonObject& parameters, UserInteraction&) { return taskCancel(parameters); });
    registerInstanceApiOperations(*this);
    registerLauncherApiDomains(*this);
    registerLauncherApiFiles(*this);
    registerLauncherApiResourceOperations(*this);
    registerLauncherApiServerOperations(*this);
    registerLauncherApiExportOperations(*this);
    registerLauncherApiComponentOperations(*this);
    registerLauncherApiCatalogOperations(*this);
    registerLauncherApiIntegrationOperations(*this);
    registerLauncherApiAppearanceOperations(*this);
    m_streams = std::make_unique<LauncherApiStreams>(*this);
    registerOperation({ "api.batch", "Execute up to 100 operations sequentially; returns individual results, without rollback. Recursive batches are rejected.",
        objectSchema({ { "operations", QJsonObject{ { "type", "array" }, { "items", objectSchema({
            { "operation", stringProperty("Operation name.") }, { "parameters", QJsonObject{ { "type", "object" } } } }, { "operation" }) } } },
            { "stopOnError", boolProperty("Stop at the first failed operation.", true) } }, { "operations" }), "batch", true },
        [this](const QJsonObject& p, UserInteraction& interaction) {
            const auto operations = p.value("operations").toArray();
            if (operations.isEmpty() || operations.size() > 100) return OperationService::failure("Batch size must be 1 to 100.", 2);
            for (const auto& item : operations) {
                const auto name = item.toObject().value("operation").toString();
                if (name == "api.batch" || !m_operations.contains(name)) return OperationService::failure("Unknown operation or recursive batch: " + name, 2);
            }
            QJsonArray results;
            bool complete = true;
            for (const auto& item : operations) {
                if (isCancellationRequested()) { complete = false; break; }
                const auto request = item.toObject();
                const auto result = execute(request.value("operation").toString(), request.value("parameters").toObject(), interaction);
                results.append(result);
                if (!result.value("ok").toBool()) {
                    complete = false;
                    if (p.value("stopOnError").toBool(true)) break;
                }
            }
            return OperationService::success(QJsonObject{ { "results", results }, { "complete", complete },
                { "executed", results.size() }, { "remaining", operations.size() - results.size() }, { "cancelled", isCancellationRequested() } });
        });
}

LauncherApi::~LauncherApi() = default;

QJsonObject LauncherApi::execute(const QString& operation, const QJsonObject& parameters, UserInteraction& interaction)
{
    if (!m_executeDepth) m_cancelRequested = false;
    QScopedValueRollback<int> depth(m_executeDepth, m_executeDepth + 1);
    const auto it = m_operations.constFind(operation);
    if (it == m_operations.constEnd()) {
        auto result = OperationService::failure(QStringLiteral("Unknown API operation: %1").arg(operation), 2);
        result.insert("apiVersion", 1);
        result.insert("operation", operation);
        return result;
    }
    const auto validation = validateInput(parameters, it->metadata.inputSchema);
    auto result = validation.isEmpty() ? it->handler(parameters, interaction) : OperationService::failure(validation, 2);
    if (!result.contains("apiVersion"))
        result.insert("apiVersion", 1);
    result.insert("operation", operation);
    return result;
}

QJsonArray LauncherApi::describe() const
{
    QStringList names = m_operations.keys();
    std::sort(names.begin(), names.end());

    QJsonArray result;
    for (const auto& name : names)
        result.append(m_operations.value(name).metadata.toJson());
    return result;
}

void LauncherApi::registerOperation(ApiOperation operation, Handler handler)
{
    if (operation.name.isEmpty() || !handler)
        return;
    // Copy the key before moving the metadata.  The key and value are passed
    // to QHash::insert in one expression, whose argument evaluation order is
    // unspecified; moving operation first could otherwise clear operation.name
    // before QHash copied it, leaving every registration under an empty key.
    const auto key = operation.name;
    RegisteredOperation registered{ std::move(operation), std::move(handler) };
    m_operations.insert(key, std::move(registered));
}

void LauncherApi::trackTask(Task* task)
{
    if (!task)
        return;
    const auto taskId = task->getUid().toString(QUuid::WithoutBraces);
    auto& record = m_tasks[taskId];
    if (!record.task) {
        record.task = task;
        m_taskOrder.append(taskId);
        connect(task, &Task::finished, this, [this, taskId] {
            const auto it = m_tasks.find(taskId);
            if (it != m_tasks.end() && it->task)
                it->snapshot = snapshotTask(taskId, it->task);
        });
    }
    record.snapshot = snapshotTask(taskId, task);
    m_externalTask = task;
    // Do not retain a QHash reference while removing entries, or evict a task
    // which still needs to receive cancellation and completion updates.
    for (qsizetype i = 0; m_taskOrder.size() > 64 && i < m_taskOrder.size();) {
        const auto expired = m_taskOrder.at(i);
        const auto tracked = m_tasks.value(expired).task;
        if (expired != taskId && (!tracked || tracked->isFinished())) {
            m_tasks.remove(expired);
            m_taskOrder.removeAt(i);
        } else {
            ++i;
        }
    }
}

void LauncherApi::clearTrackedTask(Task* task)
{
    if (task) {
        const auto taskId = task->getUid().toString(QUuid::WithoutBraces);
        const auto it = m_tasks.find(taskId);
        if (it != m_tasks.end())
            it->snapshot = snapshotTask(taskId, task);
    }
    if (m_externalTask == task)
        m_externalTask.clear();
}

void LauncherApi::cancelCurrent()
{
    m_cancelRequested = true;
    const auto legacyTask = m_legacyService ? m_legacyService->currentTask() : nullptr;
    if (m_externalTask && m_externalTask != legacyTask)
        m_externalTask->abort();
    if (m_legacyService)
        m_legacyService->cancelCurrent();
}

Task* LauncherApi::currentTask() const
{
    if (m_externalTask)
        return m_externalTask;
    return m_legacyService ? m_legacyService->currentTask() : nullptr;
}

QJsonObject LauncherApi::snapshotTask(const QString& taskId, Task* task) const
{
    if (!task)
        return QJsonObject{ { "id", taskId } };

    const auto state = task->getState();
    QJsonArray warnings;
    for (const auto& warning : task->warnings())
        warnings.append(warning);
    QJsonArray steps;
    for (const auto& progress : task->getStepProgress()) {
        if (!progress)
            continue;
        steps.append(QJsonObject{ { "id", progress->uid.toString(QUuid::WithoutBraces) },
                                  { "current", progress->current },
                                  { "total", progress->total },
                                  { "oldCurrent", progress->old_current },
                                  { "oldTotal", progress->old_total },
                                  { "status", progress->status },
                                  { "details", progress->details },
                                  { "state", taskStepStateName(progress->state) },
                                  { "finished", progress->isDone() } });
    }
    return QJsonObject{ { "id", taskId },
                        { "type", QString::fromLatin1(task->metaObject()->className()) },
                        { "name", task->objectName() },
                        { "state", taskStateName(state) },
                        { "running", task->isRunning() },
                        { "finished", task->isFinished() },
                        { "successful", task->wasSuccessful() },
                        { "canAbort", task->canAbort() },
                        { "status", task->getStatus() },
                        { "details", task->getDetails() },
                        { "progress", task->getProgress() },
                        { "totalProgress", task->getTotalProgress() },
                        { "transferRate", task->getTransferRate() },
                        { "failReason", task->failReason() },
                        { "warnings", warnings },
                        { "steps", steps } };
}

QJsonObject LauncherApi::taskStatus(const QJsonObject& parameters) const
{
    const auto requestedId = parameters.value("taskId").toString().trimmed();
    const auto active = currentTask();
    const auto activeId = active ? active->getUid().toString(QUuid::WithoutBraces) : QString();
    const auto taskId = requestedId.isEmpty() ? activeId : requestedId;

    if (taskId.isEmpty())
        return OperationService::failure(QObject::tr("There is no current task."), 2);

    const auto it = m_tasks.constFind(taskId);
    if (it != m_tasks.constEnd()) {
        if (it->task)
            return OperationService::success(snapshotTask(taskId, it->task));
        return OperationService::success(it->snapshot);
    }
    if (active && activeId == taskId)
        return OperationService::success(snapshotTask(taskId, active));
    return OperationService::failure(QObject::tr("Task not found: %1").arg(taskId), 2);
}

QJsonObject LauncherApi::taskList(const QJsonObject& parameters) const
{
    const bool runningOnly = parameters.value("runningOnly").toBool(false);
    QJsonArray tasks;
    QSet<QString> seen;
    for (const auto& taskId : m_taskOrder) {
        const auto it = m_tasks.constFind(taskId);
        if (it == m_tasks.constEnd())
            continue;
        const auto item = it->task ? snapshotTask(taskId, it->task) : it->snapshot;
        if (runningOnly && !item.value("running").toBool())
            continue;
        tasks.append(item);
        seen.insert(taskId);
    }
    if (const auto active = currentTask()) {
        const auto taskId = active->getUid().toString(QUuid::WithoutBraces);
        if (!seen.contains(taskId) && (!runningOnly || active->isRunning()))
            tasks.append(snapshotTask(taskId, active));
    }
    return OperationService::success(tasks);
}

QJsonObject LauncherApi::taskCancel(const QJsonObject& parameters)
{
    const auto requestedId = parameters.value("taskId").toString().trimmed();
    Task* task = nullptr;
    QString taskId = requestedId;
    if (requestedId.isEmpty()) {
        task = currentTask();
        if (task)
            taskId = task->getUid().toString(QUuid::WithoutBraces);
    } else {
        const auto it = m_tasks.find(requestedId);
        if (it != m_tasks.end())
            task = it->task;
        if (!task) {
            const auto active = currentTask();
            if (active && active->getUid().toString(QUuid::WithoutBraces) == requestedId)
                task = active;
        }
    }
    if (!task)
        return OperationService::failure(taskId.isEmpty() ? QObject::tr("There is no current task.")
                                                           : QObject::tr("Task not found: %1").arg(taskId),
                                         2);
    if (!task->isRunning())
        return OperationService::success(QJsonObject{ { "cancelled", false }, { "alreadyFinished", true },
                                                      { "task", snapshotTask(taskId, task) } });
    if (!task->canAbort())
        return OperationService::failure(QObject::tr("Task cannot be cancelled: %1").arg(taskId), 2);
    const bool cancelled = task->abort();
    if (cancelled) m_cancelRequested = true;
    return OperationService::success(QJsonObject{ { "cancelled", cancelled }, { "task", snapshotTask(taskId, task) } });
}

QJsonArray LauncherApi::streamNotifications()
{
    return m_streams->notifications();
}
