// SPDX-License-Identifier: GPL-3.0-only

#include "AsyncTaskRuntime.h"

#include <QDateTime>
#include <QEventLoop>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonDocument>
#include <QPointer>
#include <QSaveFile>
#include <QTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QUrl>

#include <limits>
#include <memory>
#include <utility>

#include "Application.h"
#include "BaseInstance.h"
#include "FileSystem.h"
#include "InstanceImportTask.h"
#include "meta/Index.h"
#include "minecraft/VanillaInstanceCreationTask.h"
#include "minecraft/PackProfile.h"
#include "net/Mode.h"
#include "java/download/ManifestDownloadTask.h"
#include "java/download/ArchiveDownloadTask.h"
#include "tasks/SequentialTask.h"
#include "tasks/TaskProgressSnapshot.h"
#include "api/ResourceInstallTask.h"
#include "api/InstanceExportTask.h"
#include "api/LauncherApi.h"
#include "InstanceList.h"
#include "minecraft/auth/AccountList.h"
#include "minecraft/auth/MinecraftAccount.h"
#include "minecraft/auth/AuthFlow.h"
#include "minecraft/auth/LocalYggdrasil.h"
#include "LaunchController.h"
#include "DesktopServices.h"
#include "minecraft/launch/MinecraftTarget.h"
#include "api/LauncherApiSupport.h"
#include "api/ResourceModelSupport.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/mod/ModFolderModel.h"
#include "minecraft/mod/PluginFolderModel.h"
#include "minecraft/mod/ResourceFolderModel.h"
#include "meta/CleanroomMeta.h"
#include "net/Download.h"
#include "net/NetJob.h"
#include "server/ServerInstance.h"

namespace {

BaseInstance* findInstance(const QString& reference)
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
    return instance;
}

// The UI uses short loader names, while PackProfile and metadata providers
// use the component UID. Keep the async API on the same canonical identifiers
// as LauncherApiInstances and the legacy installer.
QString loaderUid(QString loader)
{
    loader = loader.trimmed().toLower();
    static const QHash<QString, QString> aliases{ { "minecraft", "net.minecraft" },
                                                  { "fabric", "net.fabricmc.fabric-loader" },
                                                  { "quilt", "org.quiltmc.quilt-loader" },
                                                  { "forge", "net.minecraftforge" },
                                                  { "neoforge", "net.neoforged" },
                                                  { "cleanroom", "com.cleanroommc.cleanroom" },
                                                  { "liteloader", "com.mumfrey.liteloader" } };
    return aliases.value(loader, loader);
}

std::shared_ptr<ResourceFolderModel> findResourceModel(BaseInstance* instance, QString kind, const QString& world = {})
{
    kind = kind.toLower().remove('-').remove('_');
    if (!world.isEmpty()) return ApiSupport::worldDataPacks(instance, kind, world);
    if (auto server = dynamic_cast<ServerInstance*>(instance)) {
        if (kind == "mods") return std::static_pointer_cast<ResourceFolderModel>(server->loaderModList());
        if (kind == "plugins") return std::static_pointer_cast<ResourceFolderModel>(server->pluginList());
        return nullptr;
    }
    static const QMap<QString, int> indexes{ { "mods", 0 },           { "coremods", 1 },           { "nilmods", 2 },
                                             { "resourcepacks", 3 },  { "texturepacks", 4 },       { "shaderpacks", 5 },
                                             { "yesstevemodels", 6 }, { "customplayermodels", 7 }, { "schematics", 8 },
                                             { "datapacks", 9 } };
    if (!indexes.contains(kind)) return nullptr;
    const auto client = dynamic_cast<MinecraftInstance*>(instance);
    if (!client) return nullptr;
    const auto lists = client->resourceLists();
    const auto index = indexes.value(kind);
    return index < lists.size() ? lists.at(index) : nullptr;
}

MinecraftAccountPtr findAccount(const QString& reference)
{
    const auto accounts = APPLICATION->accounts();
    for (int i = 0; i < accounts->count(); ++i) {
        const auto account = accounts->at(i);
        if (account->profileId() == reference || account->internalId() == reference ||
            account->profileName().compare(reference, Qt::CaseInsensitive) == 0) {
            return account;
        }
    }
    return nullptr;
}

/**
 * Adapt the synchronous headless API operations to the event-driven Neo task
 * runtime.  The pack APIs already own the provider-specific install tasks and
 * cancellation logic; keeping them behind this proxy means every provider
 * follows the same task lifecycle as instance imports and resource installs.
 */
class ApiOperationTask final : public Task {
   public:
    ApiOperationTask(QString operation, QJsonObject parameters) : m_operation(std::move(operation)), m_parameters(std::move(parameters))
    {
        setAbortable(true);
    }

    bool abort() override
    {
        m_cancelled = true;
        if (m_inputLoop) m_inputLoop->quit();
        m_api.cancelCurrent();
        return true;
    }

    QJsonObject pendingInteraction() const { return m_pendingInput; }

    QJsonObject respond(const QJsonObject& parameters)
    {
        if (!m_inputLoop || parameters.value("interactionId").toString() != m_pendingInput.value("interactionId").toString())
            return { { "ok", false }, { "error", "No matching pending task interaction." } };
        const auto answer = parameters.value("value");
        if (!parameters.value("cancel").toBool()) {
            const auto choices = m_pendingInput.value("choices");
            if ((!choices.isArray() && !answer.isString()) ||
                (choices.isArray() && (!answer.isDouble() || answer.toDouble() != answer.toInt(-1) ||
                 answer.toInt(-1) < 0 || answer.toInt() >= choices.toArray().size())))
                return { { "ok", false }, { "error", "Expected text or a valid choice index." } };
        }
        m_answer = parameters.value("cancel").toBool() ? QJsonValue() : answer;
        m_inputLoop->quit();
        m_inputLoop = nullptr;
        return { { "ok", true }, { "data", QJsonObject{ { "accepted", true } } } };
    }

    QJsonValue ask(QJsonObject context)
    {
        if (m_cancelled) return {};
        QEventLoop loop;
        m_answer = QJsonValue();
        context.insert("interactionId", QUuid::createUuid().toString(QUuid::WithoutBraces));
        m_pendingInput = context;
        m_inputLoop = &loop;
        setStatus(context.value("type").toString() == "restricted-downloads" ?
                  tr("Waiting for restricted downloads and confirmation") : tr("Waiting for input"));
        loop.exec();
        m_inputLoop = nullptr;
        m_pendingInput = {};
        if (m_cancelled) return {};
        return std::exchange(m_answer, QJsonValue());
    }

    QList<Task*> getSubTasks() const override
    {
        return m_child ? QList<Task*>{ m_child.data() } : QList<Task*>{};
    }

   protected:
    void executeTask() override
    {
        class Interaction final : public UserInteraction {
           public:
            explicit Interaction(ApiOperationTask* owner) : m_owner(owner) {}
            void status(const QString& message) override { m_owner->setStatus(message); }
            void deviceCode(const QString& url, const QString& code, int expiresIn) override
            {
                m_owner->setDetails(url.isEmpty() ? code : url + " " + code);
                Q_UNUSED(expiresIn);
            }
            std::optional<QString> input(const QString& prompt, bool secret) override
            {
                const auto answer = m_owner->ask({ { "type", "input" }, { "prompt", prompt }, { "secret", secret } });
                return answer.isString() ? std::optional<QString>(answer.toString()) : std::nullopt;
            }
            std::optional<QString> inputWithContext(const QString& prompt, const QJsonObject& context) override
            {
                auto request = context;
                request.insert("prompt", prompt);
                const auto answer = m_owner->ask(request);
                return answer.isString() ? std::optional<QString>(answer.toString()) : std::nullopt;
            }
            std::optional<int> select(const QString& prompt, const QJsonArray& choices) override
            {
                const auto answer = m_owner->ask({ { "type", "select" }, { "prompt", prompt }, { "choices", choices } });
                return answer.isDouble() ? std::optional<int>(answer.toInt()) : std::nullopt;
            }

           private:
            ApiOperationTask* m_owner = nullptr;
        } interaction(this);

        QTimer bridge;
        bridge.setInterval(100);
        connect(&bridge, &QTimer::timeout, this, [this] {
            auto* child = m_api.currentTask();
            if (!child || child == this || child == m_child.data())
                return;
            if (m_child) disconnect(m_child, nullptr, this, nullptr);
            m_step_progress.clear();
            m_child = child;
            setStatus(child->getStatus());
            setDetails(child->getDetails());
            setProgress(child->getProgress(), child->getTotalProgress());
            setTransferRate(child->getTransferRate());
            connect(child, &Task::status, this, &Task::setStatus, Qt::UniqueConnection);
            connect(child, &Task::details, this, &Task::setDetails, Qt::UniqueConnection);
            connect(child, &Task::progress, this, &Task::setProgress, Qt::UniqueConnection);
            connect(child, &Task::transferRate, this, &Task::setTransferRate, Qt::UniqueConnection);
            connect(child, &Task::stepProgress, this, &Task::propagateStepProgress, Qt::UniqueConnection);
        });
        bridge.start();
        const auto result = m_api.execute(m_operation, m_parameters, interaction);
        bridge.stop();
        if (m_api.isCancellationRequested()) {
            emitAborted();
        } else if (result.value("ok").toBool() &&
                   !(m_operation == "resource.install-with-dependencies" && !result.value("data").toObject().value("complete").toBool())) {
            setProgress(1, 1);
            emitSucceeded();
        } else {
            QString reason = result.value("error").toString();
            for (const auto& item : result.value("data").toObject().value("items").toArray()) {
                if (reason.isEmpty() && !item.toObject().value("ok").toBool()) reason = item.toObject().value("error").toString();
            }
            emitFailed(reason.isEmpty() ? QObject::tr("操作未全部完成。") : reason);
        }
    }

   private:
    QString m_operation;
    QJsonObject m_parameters;
    LauncherApi m_api;
    QPointer<Task> m_child;
    QPointer<QEventLoop> m_inputLoop;
    QJsonObject m_pendingInput;
    QJsonValue m_answer;
    bool m_cancelled = false;
};

}  // namespace

AsyncTaskRuntime& AsyncTaskRuntime::instance()
{
    static AsyncTaskRuntime service;
    return service;
}

AsyncTaskRuntime::AsyncTaskRuntime(QObject* parent) : QObject(parent), m_temp()
{
    loadJournal();
    m_tick = new QTimer(this);
    m_tick->setInterval(250);
    connect(m_tick, &QTimer::timeout, this, [this] {
        for (const auto& id : m_order) {
            if (!m_records.contains(id)) continue;
            const auto& record = m_records.value(id);
            if (!record.finished && (record.state == "running" || record.state == "queued")) emitChanged(record);
        }
    });
    m_tick->start();
}

QString AsyncTaskRuntime::conflictKeyFor(const QString& operation, const QJsonObject& parameters) const
{
    if (operation == "instance.import" || operation == "instance.create" || operation == "modpack.install")
        return "instances";
    if (operation == "resource.install" || operation == "resource.install-version" || operation == "instance.component.set-version" || operation == "instance.special-loader.install")
        return "instance:" + parameters.value("instance").toString();
    return "global";
}

QStringList resourceKeysFor(const QString& operation, const QJsonObject& parameters)
{
    QStringList keys;
    if (operation == "instance.import" || operation == "instance.create" || operation == "modpack.install")
        keys << "instances";
    else if (operation == "resource.install" || operation == "resource.install-version" || operation == "resource.install-with-dependencies" || operation == "instance.component.set-version" || operation == "instance.special-loader.install" || operation == "instance.update" || operation == "instance.launch" || operation == "instance.export" || operation == "server.start")
        keys << "instance:" + parameters.value("instance").toString();
    else if (operation == "java.install")
        keys << "java";
    else if (operation == "account.refresh")
        keys << "account:" + parameters.value("account").toString();
    else if (operation == "account.login")
        keys << "accounts";
    else
        keys << "global";
    return keys;
}

void AsyncTaskRuntime::addResourceKeys(Record& record) const
{
    const auto keys = resourceKeysFor(record.operation, record.parameters);
    if (record.operation == "instance.component.set-version" || record.operation == "resource.install") {
        record.writes = QSet<QString>(keys.begin(), keys.end());
    } else {
        record.writes = QSet<QString>(keys.begin(), keys.end());
    }
    record.conflictKey = keys.value(0);
    // These compatibility operations own a ScopedUserInteraction across a
    // nested Qt event loop. Do not nest two such installs: a callback from the
    // outer task must not consume the inner task's interaction channel.
    if (QStringList{ "instance.import", "modpack.install", "resource.install-version",
                     "resource.install-with-dependencies", "instance.special-loader.install" }.contains(record.operation))
        record.writes.insert("interactive-install");
}

int AsyncTaskRuntime::deadlineFor(const QString& operation) const
{
    if (operation == "modpack.install") return 30 * 60 * 1000;
    if (operation == "instance.create" || operation == "instance.import") return 30 * 60 * 1000;
    if (operation == "resource.install" || operation == "resource.install-version") return 15 * 60 * 1000;
    if (operation == "instance.component.set-version") return 10 * 60 * 1000;
    if (operation == "instance.special-loader.install") return 30 * 60 * 1000;
    if (operation == "java.install") return 15 * 60 * 1000;
    if (operation == "resource.install-with-dependencies") return 20 * 60 * 1000;
    if (operation == "account.refresh") return 5 * 60 * 1000;
    if (operation == "account.login") return 10 * 60 * 1000;
    if (operation == "instance.launch") return 10 * 60 * 1000;
    if (operation == "instance.export") return 30 * 60 * 1000;
    if (operation == "server.start") return 5 * 60 * 1000;
    if (operation == "instance.update") return 20 * 60 * 1000;
    return 15 * 60 * 1000;
}

QJsonObject AsyncTaskRuntime::start(const QString& operation, const QJsonObject& parameters, UserInteraction& interaction)
{
    Q_UNUSED(interaction);
    if (!QStringList{ "instance.import", "instance.create", "instance.component.set-version", "instance.special-loader.install", "instance.update", "instance.launch", "java.install", "modpack.install", "resource.install", "resource.install-version", "resource.install-with-dependencies", "account.refresh", "account.login", "instance.export", "server.start" }.contains(operation))
        return { { "ok", false }, { "error", QStringLiteral("Unsupported async task operation: %1").arg(operation) } };

    const auto idempotencyKey = parameters.value("idempotencyKey").toString();
    if (!idempotencyKey.isEmpty()) {
        for (const auto& id : m_order) {
            const auto& old = m_records.value(id);
            if (old.idempotencyKey == idempotencyKey && old.idempotencyKey == idempotencyKey)
                return { { "ok", true }, { "data", snapshot(old) }, { "deduplicated", true } };
        }
    }

    pruneRecords();
    const auto id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    Record record;
    record.id = id;
    record.operation = operation;
    record.parameters = parameters;
    record.idempotencyKey = idempotencyKey;
    record.priority = parameters.value("priority").toString() == "high" ? 1 : 0;
    record.queuedAt = QDateTime::currentMSecsSinceEpoch();
    record.deadlineMs = deadlineFor(operation);
    addResourceKeys(record);
    if (operation == "instance.import" || operation == "instance.create" || operation == "modpack.install") {
        for (int i = 0; i < APPLICATION->instances()->count(); ++i)
            record.instancesBefore.insert(APPLICATION->instances()->at(i)->id());
    }
    m_records.insert(id, record);
    m_order.append(id);
    m_pending.enqueue({ id });
    saveJournal();
    emitChanged(m_records[id]);
    pump();
    return { { "ok", true }, { "data", snapshot(m_records.value(id)) } };
}

bool AsyncTaskRuntime::canAcquire(const Record& record) const
{
    for (const auto& key : record.reads)
        if (m_writers.contains(key)) return false;
    for (const auto& key : record.writes) {
        if (m_writers.contains(key)) return false;
        if (m_readerCounts.value(key) > 0) return false;
    }
    return true;
}

void AsyncTaskRuntime::acquire(const Record& record)
{
    for (const auto& key : record.reads) m_readerCounts[key] += 1;
    for (const auto& key : record.writes) m_writers.insert(key);
}

void AsyncTaskRuntime::release(const Record& record)
{
    for (const auto& key : record.reads) {
        const auto count = m_readerCounts.value(key) - 1;
        if (count <= 0) m_readerCounts.remove(key);
        else m_readerCounts[key] = count;
    }
    for (const auto& key : record.writes) m_writers.remove(key);
}

void AsyncTaskRuntime::pump()
{
    while (m_active < maxActive && !m_pending.isEmpty()) {
        int best = -1;
        qint64 bestScore = std::numeric_limits<qint64>::min();
        const auto now = QDateTime::currentMSecsSinceEpoch();
        for (int i = 0; i < m_pending.size(); ++i) {
            const auto& candidate = m_records[m_pending.at(i).id];
            if (candidate.finished || !canAcquire(candidate)) continue;
            const qint64 age = now - candidate.queuedAt;
            const qint64 score = static_cast<qint64>(candidate.priority) * 1000000000LL + age;
            if (score > bestScore) { best = i; bestScore = score; }
        }
        if (best < 0) break;
        const auto pending = m_pending.takeAt(best);
        auto& record = m_records[pending.id];
        if (record.finished) continue;
        acquire(record);
        record.state = "running";
        record.startedAt = now;
        ++m_active;
        armDeadline(record.id);
        saveJournal();
        emitChanged(record);
        // Start on the next event-loop turn. Most task implementations return
        // immediately from start(), but provider-backed operations adapt the
        // synchronous compatibility API with a nested event loop. Deferring
        // the call lets neo.task.start return its task id first, so status,
        // progress and cancellation requests can be processed while that
        // provider task is running.
        QTimer::singleShot(0, this, [this, id = record.id] {
            if (!m_records.contains(id) || m_records[id].finished || m_records[id].state != "running") return;
            startRecord(m_records[id]);
        });
    }
}

void AsyncTaskRuntime::armDeadline(const QString& id)
{
    auto& record = m_records[id];
    if (record.deadlineMs <= 0) return;
    record.deadline = new QTimer(this);
    record.deadline->setSingleShot(true);
    connect(record.deadline, &QTimer::timeout, this, [this, id]() {
        if (!m_records.contains(id) || m_records[id].finished) return;
        cancel(id);
        if (m_records.contains(id)) {
            m_records[id].error = "任务超时";
            m_records[id].state = "failed";
            m_records[id].finished = true;
            saveJournal();
            emitChanged(m_records[id]);
        }
    });
    record.deadline->start(record.deadlineMs);
}

void AsyncTaskRuntime::finish(const QString& taskId, bool success, const QString& error, const QString& instanceId)
{
    if (!m_records.contains(taskId)) return;
    auto& record = m_records[taskId];
    if (record.finished) return;
    record.finished = true;
    record.state = record.cancelRequested ? "cancelled" : (success ? "succeeded" : "failed");
    record.error = error;
    record.finishedAt = QDateTime::currentMSecsSinceEpoch();
    if (!instanceId.isEmpty()) record.instanceId = instanceId;
    if (record.deadline) {
        record.deadline->stop();
        record.deadline->deleteLater();
        record.deadline = nullptr;
    }
    if (record.state != "queued") release(record);
    m_active = qMax(0, m_active - 1);
    saveJournal();
    emitChanged(record);
    pump();
}

void AsyncTaskRuntime::emitChanged(const Record& record)
{
    emit taskChanged(snapshot(record));
}

QJsonObject AsyncTaskRuntime::snapshot(const Record& record) const
{
    QJsonArray reads, writes, children;
    for (const auto& key : record.reads) reads.append(key);
    for (const auto& key : record.writes) writes.append(key);
    QJsonObject taskProgress;
    qint64 transferRate = 0;
    QString details;
    if (record.task) {
        transferRate = record.task->getTransferRate();
        details = record.task->getDetails();
        taskProgress = TaskProgressSnapshot::snapshot(record.task.data());
        children = taskProgress.value("children").toArray();
    }
    return QJsonObject{
        { "taskId", record.id },
        { "operation", record.operation },
        { "state", record.state },
        { "status", record.status },
        { "name", record.parameters.value("name").toString() },
        { "error", record.error },
        { "progress", static_cast<double>(record.progress) },
        { "total", static_cast<double>(record.total) },
        { "totalProgress", static_cast<double>(record.total) },
        { "transferRate", static_cast<double>(transferRate) },
        { "details", details },
        { "interaction", dynamic_cast<ApiOperationTask*>(record.task.data()) ?
                         static_cast<ApiOperationTask*>(record.task.data())->pendingInteraction() : QJsonObject{} },
        { "children", children },
        { "summary", taskProgress.value("summary") },
        { "childrenTruncated", taskProgress.value("childrenTruncated") },
        { "steps", children },
        { "finished", record.finished },
        { "cancelRequested", record.cancelRequested },
        { "conflictKey", record.conflictKey },
        { "reads", reads },
        { "writes", writes },
        { "createdAt", static_cast<double>(record.queuedAt) },
        { "startedAt", static_cast<double>(record.startedAt) },
        { "finishedAt", static_cast<double>(record.finishedAt) },
        { "instanceId", record.instanceId },
        { "createdInstanceId", record.createdInstanceId },
        { "pid", static_cast<double>(record.pid) },
        { "deviceCode", record.deviceCode },
        { "deviceUrl", record.deviceUrl },
        { "deviceExpiresIn", record.deviceExpiresIn },
        { "idempotencyKey", record.idempotencyKey },
    };
}

QJsonObject AsyncTaskRuntime::status(const QString& taskId) const
{
    if (!m_records.contains(taskId)) return { { "ok", false }, { "error", "Unknown async task." } };
    return { { "ok", true }, { "data", snapshot(m_records.value(taskId)) } };
}

QJsonObject AsyncTaskRuntime::list() const
{
    QJsonArray tasks;
    for (auto it = m_order.crbegin(); it != m_order.crend(); ++it) {
        if (m_records.contains(*it)) tasks.append(snapshot(m_records.value(*it)));
    }
    return { { "ok", true }, { "data", QJsonObject{ { "tasks", tasks } } } };
}

QJsonObject AsyncTaskRuntime::respond(const QJsonObject& parameters)
{
    const auto id = parameters.value("taskId").toString();
    if (!m_records.contains(id) || m_records[id].finished)
        return { { "ok", false }, { "error", "Unknown or finished task." } };
    auto* task = dynamic_cast<ApiOperationTask*>(m_records[id].task.data());
    if (!task) return { { "ok", false }, { "error", "Task is not awaiting input." } };
    return task->respond(parameters);
}

QJsonObject AsyncTaskRuntime::cancel(const QString& taskId)
{
    if (!m_records.contains(taskId)) return { { "ok", false }, { "error", "Unknown async task." } };
    auto& record = m_records[taskId];
    if (record.finished) return { { "ok", true }, { "data", snapshot(record) } };
    const bool wasRunning = record.state == "running";
    record.cancelRequested = true;
    record.finished = true;
    record.state = "cancelled";
    record.error = "cancelled";
    record.status = "已取消";
    record.finishedAt = QDateTime::currentMSecsSinceEpoch();
    if (record.deadline) {
        record.deadline->stop();
        record.deadline->deleteLater();
        record.deadline = nullptr;
    }
    for (int i = m_pending.size() - 1; i >= 0; --i) {
        if (m_pending.at(i).id == taskId) m_pending.removeAt(i);
    }
    if (wasRunning) {
        release(record);
        m_active = qMax(0, m_active - 1);
    }
    if (record.task) record.task->abort();
    saveJournal();
    emitChanged(record);
    pump();
    return { { "ok", true }, { "data", snapshot(record) } };
}

void AsyncTaskRuntime::startRecord(const Record& record)
{
    const auto id = record.id;
    if (record.operation == "instance.update") {
        const auto instance = findInstance(record.parameters.value("instance").toString());
        if (!instance) { finish(id, false, "Instance not found."); return; }
        if (instance->isRunning()) { finish(id, false, "A running instance cannot be updated."); return; }
        const auto updateTasks = instance->createUpdateTask();
        if (updateTasks.isEmpty()) { finish(id, true, {}, instance->id()); return; }
        auto task = makeShared<SequentialTask>(QObject::tr("Update instance %1").arg(instance->name()));
        for (const auto& updateTask : updateTasks) task->addTask(updateTask);
        auto& stored = m_records[id];
        stored.task = task.get();
        stored.holder = task;
        stored.status = "正在更新实例";
        connect(task.get(), &Task::status, this, [this, id](const QString& status) { if (m_records.contains(id)) m_records[id].status = status; });
        connect(task.get(), &Task::progress, this, [this, id](qint64 current, qint64 total) {
            if (!m_records.contains(id)) return;
            m_records[id].progress = current;
            m_records[id].total = total;
        });
        connect(task.get(), &Task::finished, this, [this, id, task, instanceId = instance->id()]() {
            if (!m_records.contains(id) || m_records[id].finished) return;
            finish(id, task->wasSuccessful(), task->wasSuccessful() ? QString() : task->failReason(), instanceId);
        });
        task->start();
        return;
    }

    if (record.operation == "java.install") {
        const auto parameters = record.parameters;
        const auto metadata = parameters.value("metadata").toObject();
        const auto url = QUrl(parameters.value("url").toString(metadata.value("url").toString()));
        const auto name = parameters.value("name").toString(metadata.value("name").toString());
        const auto type = parameters.value("downloadType").toString(metadata.value("downloadType").toString());
        const auto checksumType = parameters.value("checksumType").toString(metadata.value("checksumType").toString());
        const auto checksumHash = parameters.value("checksumHash").toString(metadata.value("checksumHash").toString());
        if (!url.isValid() || (url.scheme() != "http" && url.scheme() != "https")) { finish(id, false, "Java installation requires an HTTP(S) URL."); return; }
        if (name.isEmpty() || name == "." || name == ".." || QDir::isAbsolutePath(name) || name.contains('/') || name.contains('\\') || name.contains("..")) { finish(id, false, "Java installation name is invalid."); return; }
        const auto root = APPLICATION->javaPath().trimmed();
        if (root.isEmpty()) { finish(id, false, "The managed Java directory is not configured."); return; }
        const auto finalPath = QDir(root).filePath(name);
        if (QFileInfo::exists(finalPath)) {
            if (!parameters.value("replace").toBool()) { finish(id, false, "Java installation already exists: " + name); return; }
            if (!FS::deletePath(finalPath)) { finish(id, false, "Could not replace existing Java installation: " + name); return; }
        }
        Task::Ptr task;
        if (type.compare("manifest", Qt::CaseInsensitive) == 0)
            task = makeShared<Java::ManifestDownloadTask>(url, finalPath, checksumType, checksumHash);
        else if (type.compare("archive", Qt::CaseInsensitive) == 0)
            task = makeShared<Java::ArchiveDownloadTask>(url, finalPath, checksumType, checksumHash);
        else { finish(id, false, "Unsupported Java download type: " + type); return; }
        auto& stored = m_records[id];
        stored.task = task.get();
        stored.holder = task;
        stored.status = "正在下载 Java 运行时";
        connect(task.get(), &Task::status, this, [this, id](const QString& status) { if (m_records.contains(id)) m_records[id].status = status; });
        connect(task.get(), &Task::progress, this, [this, id](qint64 current, qint64 total) {
            if (!m_records.contains(id)) return;
            m_records[id].progress = current;
            m_records[id].total = total;
        });
        connect(task.get(), &Task::finished, this, [this, id, task, finalPath]() {
            if (!m_records.contains(id) || m_records[id].finished) return;
            if (!task->wasSuccessful()) {
                FS::deletePath(finalPath);
                finish(id, false, task->failReason());
            } else {
                finish(id, true);
            }
        });
        task->start();
        return;
    }

    if (record.operation == "instance.component.set-version") {
        const auto parameters = record.parameters;
        const auto instance = dynamic_cast<MinecraftInstance*>(findInstance(parameters.value("instance").toString()));
        if (!instance) { finish(id, false, "Minecraft instance not found."); return; }
        if (instance->isRunning()) { finish(id, false, "Components cannot be modified while the instance is running."); return; }
        const auto requestedUid = parameters.value("component").toString().trimmed();
        const auto version = parameters.value("version").toString().trimmed();
        if (requestedUid.isEmpty() || version.isEmpty()) { finish(id, false, "A component id and version are required."); return; }
        const auto uid = loaderUid(requestedUid);
        auto profile = instance->getPackProfile();
        if (!profile->isLoaded()) {
            const auto loaded = profile->reload(Net::Mode::Offline);
            if (!loaded) { finish(id, false, loaded.error); return; }
        }

        // Match VersionPage: Minecraft is the important root component and a
        // version switch clears an instance-specific Java override when the
        // automatic Java switch settings are enabled.
        auto important = parameters.value("important").toBool(false);
        if (uid == "net.minecraft") {
            important = true;
            if (APPLICATION->settings()->get("AutomaticJavaSwitch").toBool() && instance->settings()->get("AutomaticJava").toBool() &&
                instance->settings()->get("OverrideJavaLocation").toBool()) {
                instance->settings()->set("OverrideJavaLocation", false);
                instance->settings()->set("JavaPath", "");
            }
        }

        // Cleanroom updates replace the instance package and patch files. A
        // plain PackProfile version edit leaves the old package in place, so
        // use the same dedicated task as the legacy VersionPage.
        if (Meta::Cleanroom::isUid(uid)) {
            profile->saveNow();
            const auto cleanroomTask = Meta::Cleanroom::updateInstanceTask(instance->instanceRoot(), version);
            if (!cleanroomTask) { finish(id, false, "The Cleanroom update task could not be created."); return; }
            auto& stored = m_records[id];
            stored.task = cleanroomTask.get();
            stored.holder = cleanroomTask;
            stored.status = "正在更新 Cleanroom " + version;
            connect(cleanroomTask.get(), &Task::status, this, [this, id](const QString& status) {
                if (m_records.contains(id)) m_records[id].status = status;
            });
            connect(cleanroomTask.get(), &Task::progress, this, [this, id](qint64 current, qint64 total) {
                if (!m_records.contains(id)) return;
                m_records[id].progress = current;
                m_records[id].total = total;
            });
            connect(cleanroomTask.get(), &Task::finished, this, [this, id, profile, cleanroomTask, instanceId = instance->id()]() {
                if (!m_records.contains(id) || m_records[id].finished) return;
                if (!cleanroomTask->wasSuccessful()) {
                    finish(id, false, cleanroomTask->failReason(), instanceId);
                    return;
                }
                const auto reloaded = profile->reload(Net::Mode::Online);
                if (!reloaded) {
                    finish(id, false, reloaded.error, instanceId);
                    return;
                }
                const auto resolveTask = profile->getCurrentTask();
                if (!resolveTask) {
                    profile->saveNow();
                    finish(id, true, {}, instanceId);
                    return;
                }
                auto& record = m_records[id];
                record.task = resolveTask.get();
                record.holder = resolveTask;
                record.status = "正在解析 Cleanroom 组件依赖";
                connect(resolveTask.get(), &Task::status, this, [this, id](const QString& status) {
                    if (m_records.contains(id)) m_records[id].status = status;
                });
                connect(resolveTask.get(), &Task::progress, this, [this, id](qint64 current, qint64 total) {
                    if (!m_records.contains(id)) return;
                    m_records[id].progress = current;
                    m_records[id].total = total;
                });
                connect(resolveTask.get(), &Task::finished, this, [this, id, profile, resolveTask, instanceId]() {
                    if (!m_records.contains(id) || m_records[id].finished) return;
                    const bool success = resolveTask->wasSuccessful();
                    if (success) profile->saveNow();
                    finish(id, success, success ? QString() : resolveTask->failReason(), instanceId);
                });
            });
            cleanroomTask->start();
            return;
        }

        if (!profile->setComponentVersion(uid, version, important)) {
            finish(id, false, "The component version could not be changed.");
            return;
        }
        profile->resolve(Net::Mode::Online);
        const auto task = profile->getCurrentTask();
        if (!task) {
            profile->saveNow();
            finish(id, true, {}, instance->id());
            return;
        }
        auto& stored = m_records[id];
        stored.task = task.get();
        stored.holder = task;
        stored.status = "正在解析组件依赖";
        connect(task.get(), &Task::status, this, [this, id](const QString& status) { if (m_records.contains(id)) m_records[id].status = status; });
        connect(task.get(), &Task::progress, this, [this, id](qint64 current, qint64 total) {
            if (!m_records.contains(id)) return;
            m_records[id].progress = current;
            m_records[id].total = total;
        });
        connect(task.get(), &Task::finished, this, [this, id, profile, task, instanceId = instance->id()]() {
            if (!m_records.contains(id) || m_records[id].finished) return;
            const bool success = task ? task->wasSuccessful() : true;
            if (success) profile->saveNow();
            finish(id, success, success ? QString() : (task ? task->failReason() : "Component resolution failed."), instanceId);
        });
        task->start();
        return;
    }

    if (record.operation == "instance.import" || record.operation == "resource.install-with-dependencies" || record.operation == "modpack.install" || record.operation == "resource.install-version" || record.operation == "instance.special-loader.install" || record.operation == "instance.launch") {
        QJsonObject parameters;
        if (record.operation == "instance.launch") {
            // Route launches through the same compatibility API adapter as
            // installs. This preserves the UserInteraction channel used by
            // headless launch checks (for example, the low-memory prompt),
            // allowing Neo UI to answer instead of treating it as cancelled.
            parameters = record.parameters;
        } else if (record.operation == "modpack.install") {
            for (const auto key : { "provider", "projectId", "versionId", "name", "group", "icon", "optionalMods", "privateCode", "offline" })
                if (record.parameters.contains(key)) parameters.insert(key, record.parameters.value(key));
        } else if (record.operation == "resource.install-version") {
            for (const auto key : { "provider", "kind", "projectId", "versionId", "instance", "world", "replace", "minecraftVersion", "loaders", "pluginLoaders", "includeChangelog" })
                if (record.parameters.contains(key)) parameters.insert(key, record.parameters.value(key));
        } else if (record.operation == "instance.import") {
            for (const auto key : { "source", "name", "group", "icon", "optionalMods" })
                if (record.parameters.contains(key)) parameters.insert(key, record.parameters.value(key));
        } else if (record.operation == "resource.install-with-dependencies") {
            parameters = record.parameters;
        } else {
            for (const auto key : { "instance", "loader", "minecraftVersion", "loaderVersion", "commitRef" })
                if (record.parameters.contains(key)) parameters.insert(key, record.parameters.value(key));
        }
        auto task = makeShared<ApiOperationTask>(record.operation, parameters);
        auto& stored = m_records[id];
        stored.task = task.get();
        stored.holder = task;
        connect(task.get(), &Task::status, this, [this, id](const QString& status) { if (m_records.contains(id)) m_records[id].status = status; });
        connect(task.get(), &Task::progress, this, [this, id](qint64 current, qint64 total) {
            if (!m_records.contains(id)) return;
            m_records[id].progress = current;
            m_records[id].total = total;
        });
        connect(task.get(), &Task::finished, this, [this, id, task, instanceId = parameters.value("instance").toString()]() {
            if (!m_records.contains(id)) return;
            const bool success = task->wasSuccessful();
            if ((m_records[id].operation == "modpack.install" || m_records[id].operation == "instance.import") && success) {
                APPLICATION->instances()->saveNow();
                const auto& before = m_records[id].instancesBefore;
                for (int i = 0; i < APPLICATION->instances()->count(); ++i) {
                    const auto candidate = APPLICATION->instances()->at(i);
                    if (!before.contains(candidate->id())) {
                        m_records[id].createdInstanceId = candidate->id();
                        break;
                    }
                }
            }
            finish(id, success, success ? QString() : task->failReason(), instanceId);
        });
        task->start();
        return;
    }

    if (record.operation == "instance.create") {
        const auto parameters = record.parameters;
        const auto type = parameters.value("type").toString().trimmed().toLower();
        if (!type.isEmpty() && type != "vanilla") {
            finish(id, false, "Async instance.create currently supports vanilla and loader instances only.");
            return;
        }
        const auto versionName = parameters.value("version").toString().trimmed();
        if (versionName.isEmpty()) { finish(id, false, "A Minecraft version is required."); return; }
        if (!ApiSupport::identifier(versionName)) { finish(id, false, "Invalid Minecraft version."); return; }
        const auto requestedLoader = parameters.value("loader").toString().trimmed().toLower();
        const auto loaderVersionName = parameters.value("loaderVersion").toString().trimmed();
        if (!requestedLoader.isEmpty() && !ApiSupport::identifier(loaderVersionName)) {
            finish(id, false, "A valid loader version is required.");
            return;
        }
        const auto name = parameters.value("name").toString().trimmed();
        const auto group = parameters.value("group").toString();
        const auto icon = parameters.value("icon").toString("default");
        const auto loader = loaderUid(requestedLoader);

        auto& stored = m_records[id];
        stored.status = "正在加载 Minecraft 版本 " + versionName;
        const auto index = APPLICATION->metadataIndex();
        auto loadMinecraft = index->loadVersion("net.minecraft", versionName);
        stored.holder = loadMinecraft;
        stored.task = loadMinecraft.get();
        connect(loadMinecraft.get(), &Task::status, this, [this, id](const QString& status) {
            if (m_records.contains(id)) m_records[id].status = status;
        });
        connect(loadMinecraft.get(), &Task::finished, this, [this, id, index, versionName, loader, loaderVersionName, name, group, icon]() {
            if (!m_records.contains(id) || m_records[id].finished) return;
            const auto minecraft = index->get("net.minecraft", versionName);
            if (!minecraft || !minecraft->isLoaded()) {
                finish(id, false, "Minecraft version not found: " + versionName);
                return;
            }
            auto createWrapped = [this, id, minecraft, loader, name, group, icon](BaseVersion::Ptr loaderVersion) {
                if (!m_records.contains(id) || m_records[id].finished) return;
                InstanceTask* raw = nullptr;
                if (Meta::Cleanroom::isUid(loader) && loaderVersion) {
                    // Cleanroom's legacy installer downloads a complete package
                    // and normalizes its patches. A plain VanillaCreationTask
                    // only adds the metadata component and leaves the package
                    // files missing, so mirror the established installer path.
                    raw = new InstanceImportTask(QUrl(Meta::Cleanroom::packageUrl(loaderVersion->descriptor())), nullptr);
                } else {
                    raw = loaderVersion ? new VanillaCreationTask(minecraft, loader, loaderVersion)
                                         : new VanillaCreationTask(minecraft);
                }
                if (!name.isEmpty()) raw->setName(name);
                raw->setGroup(group);
                raw->setIcon(icon);
                auto* task = APPLICATION->instances()->wrapInstanceTask(raw);
                if (!task) { finish(id, false, "The instance creation task could not be created."); return; }
                auto& record = m_records[id];
                record.task = task;
                record.status = "正在创建实例";
                connect(task, &Task::status, this, [this, id](const QString& status) { if (m_records.contains(id)) m_records[id].status = status; });
                connect(task, &Task::progress, this, [this, id](qint64 current, qint64 total) {
                    if (!m_records.contains(id)) return;
                    m_records[id].progress = current;
                    m_records[id].total = total;
                });
                connect(task, &Task::finished, this, [this, id, task]() {
                    if (!m_records.contains(id)) return;
                    const bool success = task->wasSuccessful();
                    QString created;
                    if (success) {
                        APPLICATION->instances()->saveNow();
                        const auto& before = m_records[id].instancesBefore;
                        for (int i = 0; i < APPLICATION->instances()->count(); ++i) {
                            const auto candidate = APPLICATION->instances()->at(i);
                            if (!before.contains(candidate->id())) { created = candidate->id(); break; }
                        }
                    }
                    if (!created.isEmpty()) m_records[id].createdInstanceId = created;
                    finish(id, success, success ? QString() : task->failReason());
                });
                task->start();
            };
            if (loader.isEmpty()) { createWrapped({}); return; }
            auto& record = m_records[id];
            record.status = "正在加载加载器 " + loader + " " + loaderVersionName;
            auto loadLoader = index->loadVersion(loader, loaderVersionName);
            record.holder = loadLoader;
            record.task = loadLoader.get();
            connect(loadLoader.get(), &Task::status, this, [this, id](const QString& status) { if (m_records.contains(id)) m_records[id].status = status; });
            connect(loadLoader.get(), &Task::finished, this, [this, id, index, loader, loaderVersionName, createWrapped]() {
                if (!m_records.contains(id) || m_records[id].finished) return;
                const auto loaderVersion = index->get(loader, loaderVersionName);
                if (!loaderVersion || !loaderVersion->isLoaded()) {
                    finish(id, false, "Loader version not found: " + loader + ":" + loaderVersionName);
                    return;
                }
                createWrapped(loaderVersion);
            });
            loadLoader->start();
        });
        loadMinecraft->start();
        return;
    }

    if (record.operation == "server.start") {
        auto server = dynamic_cast<ServerInstance*>(findInstance(record.parameters.value("instance").toString()));
        if (!server) { finish(id, false, "Server instance not found."); return; }
        if (!server->isRunning() && !server->startServer()) { finish(id, false, "The server could not be started."); return; }
        finish(id, true, {}, server->id());
        return;
    }

    if (record.operation == "instance.export") {
        auto instance = findInstance(record.parameters.value("instance").toString());
        if (!instance) { finish(id, false, "Instance not found."); return; }
        auto* task = new InstanceExportTask(instance, record.parameters);
        auto& stored = m_records[id];
        stored.task = task;
        stored.status = "正在导出实例";
        connect(task, &Task::status, this, [this, id](const QString& status) { if (m_records.contains(id)) m_records[id].status = status; });
        connect(task, &Task::progress, this, [this, id](qint64 current, qint64 total) {
            if (!m_records.contains(id)) return;
            m_records[id].progress = current;
            m_records[id].total = total;
        });
        connect(task, &Task::finished, this, [this, id, task, instanceId = instance->id()]() {
            if (!m_records.contains(id) || m_records[id].finished) return;
            finish(id, task->wasSuccessful(), task->wasSuccessful() ? QString() : task->failReason(), instanceId);
        });
        task->start();
        return;
    }

    if (record.operation == "account.login") {
        const auto parameters = record.parameters;
        auto type = parameters.value("type").toString().trimmed().toLower();
        if (type == "msa" || type == "microsoft-account") type = "microsoft";
        const auto username = parameters.value("username").toString().trimmed();
        if (type == "offline") {
            if (!APPLICATION->accounts()->canAddOfflineAccount()) {
                finish(id, false, "You must add a Microsoft account that owns Minecraft before adding an offline account.");
                return;
            }
            if (username.isEmpty()) { finish(id, false, "A username is required."); return; }
            auto account = MinecraftAccount::createOffline(username);
            const auto uuidMode = parameters.value("offlineUuidMode").toString("default").trimmed().toLower();
            if (uuidMode == "random") account->accountData()->minecraftProfile.id = QUuid::createUuid().toString(QUuid::Id128);
            else if (uuidMode == "custom") {
                const auto custom = QUuid::fromString(parameters.value("offlineUuid").toString());
                if (custom.isNull()) { finish(id, false, "A valid custom offline UUID is required."); return; }
                account->accountData()->minecraftProfile.id = custom.toString(QUuid::Id128);
            } else if (uuidMode != "default" && !uuidMode.isEmpty()) { finish(id, false, "Unsupported offline UUID mode."); return; }
            auto accounts = APPLICATION->accounts();
            accounts->addAccount(account);
            if (!accounts->defaultAccount()) accounts->setDefaultAccount(account);
            accounts->saveList();
            finish(id, true);
            return;
        }
        MinecraftAccountPtr account;
        bool useDeviceCode = parameters.value("useDeviceCode").toBool(true);
        if (type == "microsoft") {
            account = MinecraftAccount::createBlankMSA();
        } else if (type == "yggdrasil" || type == "unified-pass") {
            const auto password = parameters.value("password").toString();
            if (password.isEmpty()) { finish(id, false, "A password is required."); return; }
            if (type == "yggdrasil") {
                const auto authUrl = parameters.value("authUrl").toString();
                const auto sessionUrl = parameters.value("sessionUrl").toString();
                const auto managedLocal = parameters.value("managedLocal").toBool(false);
                const auto isHttpUrl = [](const QString& value) {
                    const QUrl url(value, QUrl::TolerantMode);
                    return url.isValid() && !url.host().isEmpty() && (url.scheme() == "http" || url.scheme() == "https");
                };
                if (!managedLocal && (authUrl.isEmpty() || sessionUrl.isEmpty())) { finish(id, false, "Yggdrasil login requires authUrl and sessionUrl."); return; }
                if (!managedLocal && (!isHttpUrl(authUrl) || !isHttpUrl(sessionUrl))) { finish(id, false, "Yggdrasil authUrl and sessionUrl must be HTTP(S) URLs."); return; }
                account = MinecraftAccount::createYggdrasil(username, password, authUrl, sessionUrl, parameters.value("sourceName").toString(),
                    parameters.value("refreshEndpoint").toString(), parameters.value("validateEndpoint").toString(),
                    parameters.value("authenticateEndpoint").toString(), parameters.value("profileEndpoint").toString(),
                    parameters.value("oauthTokenEndpoint").toString(),
                    parameters.value("tokenType").toString().compare("OAuth", Qt::CaseInsensitive) == 0 ? YggdrasilTokenType::OAuth : YggdrasilTokenType::Standard);
                account->accountData()->yggdrasilConfig.managedLocal = managedLocal;
                if (managedLocal) {
                    QJsonObject profile;
                    QString localError;
                    if (!LocalYggdrasil::instance().ensureProfile(username, password, profile, localError)) {
                        finish(id, false, localError.isEmpty() ? "Could not provision the local verification account." : localError);
                        return;
                    }
                    account->accountData()->minecraftProfile.id = profile.value("id").toString();
                    account->accountData()->minecraftProfile.name = profile.value("name").toString(username);
                }
            } else {
                const auto serverId = parameters.value("serverId").toString();
                if (serverId.isEmpty()) { finish(id, false, "UnifiedPass login requires serverId."); return; }
                const auto baseUrl = parameters.value("baseUrl").toString().trimmed();
                if (!baseUrl.isEmpty()) {
                    const QUrl url(baseUrl, QUrl::TolerantMode);
                    if (!url.isValid() || url.host().isEmpty() || (url.scheme() != "http" && url.scheme() != "https")) { finish(id, false, "UnifiedPass baseUrl must be an HTTP(S) URL."); return; }
                }
                account = MinecraftAccount::createUnifiedPass(username, password, serverId, baseUrl);
            }
        } else {
            finish(id, false, "Unsupported account type: " + type);
            return;
        }
        const auto requestedProfile = parameters.value("profileId").toString();
        auto selector = [requestedProfile](const QJsonArray& profiles) -> std::optional<int> {
            if (profiles.isEmpty()) return std::nullopt;
            if (!requestedProfile.isEmpty()) {
                for (int i = 0; i < profiles.size(); ++i)
                    if (profiles.at(i).toObject().value("id").toString() == requestedProfile) return i;
                return std::nullopt;
            }
            return 0;
        };
        auto task = account->login(useDeviceCode, selector);
        if (!task) { finish(id, false, "Could not start the Microsoft login flow."); return; }
        auto& stored = m_records[id];
        stored.task = task.get();
        stored.holder = task;
        stored.status = "正在等待 Microsoft 登录";
        connect(task.get(), &AuthFlow::authorizeWithBrowserWithExtra, this,
                [this, id](const QString& url, const QString& code, int expiresIn) {
                    if (!m_records.contains(id)) return;
                    auto& record = m_records[id];
                    record.deviceUrl = url;
                    record.deviceCode = code;
                    record.deviceExpiresIn = expiresIn;
                    record.status = "请在浏览器中完成 Microsoft 登录";
                    emitChanged(record);
                });
        connect(task.get(), &AuthFlow::authorizeWithBrowser, this,
                [this, id, useDeviceCode](const QUrl& url) {
                    if (!m_records.contains(id)) return;
                    auto& record = m_records[id];
                    record.deviceUrl = url.toString();
                    record.deviceExpiresIn = 0;
                    record.status = useDeviceCode ? "请在浏览器中完成 Microsoft 登录" : "正在浏览器中完成 Microsoft 授权";
                    emitChanged(record);
                    if (!useDeviceCode) DesktopServices::openUrl(url);
                });
        connect(task.get(), &Task::status, this, [this, id](const QString& status) { if (m_records.contains(id)) m_records[id].status = status; });
        connect(task.get(), &Task::progress, this, [this, id](qint64 current, qint64 total) {
            if (!m_records.contains(id)) return;
            m_records[id].progress = current;
            m_records[id].total = total;
        });
        connect(task.get(), &Task::finished, this, [this, id, account, task]() {
            if (!m_records.contains(id) || m_records[id].finished) return;
            if (!task->wasSuccessful()) { finish(id, false, task->failReason()); return; }
            if (!account->hasProfile()) { finish(id, false, "This Microsoft account has no Minecraft Java profile."); return; }
            auto accounts = APPLICATION->accounts();
            accounts->addAccount(account);
            if (!accounts->defaultAccount()) accounts->setDefaultAccount(account);
            accounts->saveList();
            finish(id, true);
        });
        task->start();
        return;
    }

    if (record.operation == "account.refresh") {
        const auto account = findAccount(record.parameters.value("account").toString());
        if (!account) { finish(id, false, "Account not found."); return; }
        auto task = account->refresh();
        if (!task) { finish(id, true); return; }
        auto& stored = m_records[id];
        stored.task = task.get();
        stored.holder = task;
        stored.status = "正在刷新账户";
        connect(task.get(), &Task::status, this, [this, id](const QString& status) { if (m_records.contains(id)) m_records[id].status = status; });
        connect(task.get(), &Task::progress, this, [this, id](qint64 current, qint64 total) {
            if (!m_records.contains(id)) return;
            m_records[id].progress = current;
            m_records[id].total = total;
        });
        connect(task.get(), &Task::finished, this, [this, id, task]() {
            if (!m_records.contains(id) || m_records[id].finished) return;
            if (task->wasSuccessful()) APPLICATION->accounts()->saveList();
            finish(id, task->wasSuccessful(), task->wasSuccessful() ? QString() : task->failReason());
        });
        task->start();
        return;
    }

    if (record.operation == "resource.install") {
        const auto parameters = record.parameters;
        const auto instance = findInstance(parameters.value("instance").toString());
        if (!instance) { finish(id, false, "Minecraft instance not found."); return; }
        if (instance->isRunning()) { finish(id, false, "Resources cannot be installed while the instance is running."); return; }
        const auto model = findResourceModel(instance, parameters.value("kind").toString(), parameters.value("world").toString());
        if (!model) { finish(id, false, "Unknown or unavailable resource kind."); return; }
        ApiSupport::ensureResourceModelReady(model);
        const auto source = parameters.value("source").toString().trimmed();
        const QUrl url(source, QUrl::TolerantMode);
        QString localPath;
        if (QFileInfo::exists(source)) localPath = QFileInfo(source).absoluteFilePath();
        else if (url.isLocalFile()) localPath = url.toLocalFile();
        if (!localPath.isEmpty()) {
            if (!model->installResource(localPath)) finish(id, false, "The resource could not be installed.");
            else finish(id, true, {}, instance->id());
            return;
        }
        if (!url.isValid() || (url.scheme() != "http" && url.scheme() != "https")) {
            finish(id, false, "Resource source is neither a local file nor an HTTP(S) URL.");
            return;
        }
        const auto fileName = QFileInfo(url.path()).fileName();
        if (fileName.isEmpty()) { finish(id, false, "The resource URL does not contain a file name."); return; }
        localPath = FS::getUniqueResourceName(QDir(m_temp.path()).filePath(fileName));
        auto job = makeShared<NetJob>("NeoTask::DownloadResource", APPLICATION->network());
        job->addNetAction(Net::Download::makeFile(url, localPath));
        auto& stored = m_records[id];
        stored.task = job.get();
        stored.holder = job;
        connect(job.get(), &Task::status, this, [this, id](const QString& status) { if (m_records.contains(id)) m_records[id].status = status; });
        connect(job.get(), &Task::progress, this, [this, id](qint64 current, qint64 total) {
            if (!m_records.contains(id)) return;
            m_records[id].progress = current;
            m_records[id].total = total;
        });
        connect(job.get(), &Task::finished, this, [this, id, job, model, localPath, instanceId = instance->id()]() {
            if (!m_records.contains(id)) return;
            if (!job->wasSuccessful()) { finish(id, false, job->failReason()); return; }
            if (!model->installResource(localPath)) finish(id, false, "The resource could not be installed.");
            else finish(id, true, {}, instanceId);
        });
        job->start();
        return;
    }

    finish(id, false, "Unsupported neo task operation.");
}

void AsyncTaskRuntime::pruneRecords()
{
    while (m_order.size() > 200) {
        bool removed = false;
        for (int i = 0; i < m_order.size(); ++i) {
            const auto id = m_order.at(i);
            if (!m_records.contains(id) || !m_records.value(id).finished) continue;
            m_records.remove(id);
            m_order.removeAt(i);
            removed = true;
            break;
        }
        if (!removed) break;
    }
}

QString journalPath()
{
    return QDir(APPLICATION->dataRoot()).filePath("neo-tasks.json");
}

void AsyncTaskRuntime::loadJournal()
{
    QFile file(journalPath());
    if (!file.open(QIODevice::ReadOnly)) return;
    const auto document = QJsonDocument::fromJson(file.readAll());
    if (!document.isArray()) return;
    for (const auto& value : document.array()) {
        const auto object = value.toObject();
        const auto id = object.value("taskId").toString();
        if (id.isEmpty() || m_records.contains(id)) continue;
        Record record;
        record.id = id;
        record.operation = object.value("operation").toString();
        record.parameters = object.value("parameters").toObject();
        record.state = object.value("state").toString("interrupted");
        record.status = object.value("status").toString();
        record.error = object.value("error").toString();
        record.instanceId = object.value("instanceId").toString();
        record.createdInstanceId = object.value("createdInstanceId").toString();
        record.pid = static_cast<qint64>(object.value("pid").toDouble());
        record.deviceCode = object.value("deviceCode").toString();
        record.deviceUrl = object.value("deviceUrl").toString();
        record.deviceExpiresIn = object.value("deviceExpiresIn").toInt();
        record.conflictKey = object.value("conflictKey").toString();
        record.startedAt = static_cast<qint64>(object.value("startedAt").toDouble());
        for (const auto& key : object.value("reads").toArray()) record.reads.insert(key.toString());
        for (const auto& key : object.value("writes").toArray()) record.writes.insert(key.toString());
        record.idempotencyKey = object.value("idempotencyKey").toString();
        record.progress = static_cast<qint64>(object.value("progress").toDouble());
        record.total = static_cast<qint64>(object.value("total").toDouble());
        record.queuedAt = static_cast<qint64>(object.value("createdAt").toDouble());
        record.finishedAt = static_cast<qint64>(object.value("finishedAt").toDouble());
        record.finished = true;
        if (!QStringList{ "succeeded", "failed", "cancelled", "interrupted" }.contains(record.state)) {
            record.state = "interrupted";
            record.error = "启动器已重启，任务状态未知";
        }
        m_records.insert(id, record);
        m_order.append(id);
    }
}

void AsyncTaskRuntime::saveJournal() const
{
    QJsonArray tasks;
    const int start = qMax(0, m_order.size() - 100);
    for (int i = start; i < m_order.size(); ++i) {
        if (!m_records.contains(m_order.at(i))) continue;
        const auto& record = m_records.value(m_order.at(i));
        QJsonObject object{
            { "taskId", record.id },
            { "operation", record.operation },
            { "parameters", record.parameters },
            { "state", record.state },
            { "status", record.status },
            { "error", record.error },
            { "progress", static_cast<double>(record.progress) },
            { "total", static_cast<double>(record.total) },
            { "instanceId", record.instanceId },
            { "createdInstanceId", record.createdInstanceId },
            { "pid", static_cast<double>(record.pid) },
            { "deviceCode", record.deviceCode },
            { "deviceUrl", record.deviceUrl },
            { "deviceExpiresIn", record.deviceExpiresIn },
            { "conflictKey", record.conflictKey },
            { "reads", QJsonArray::fromStringList(QStringList(record.reads.begin(), record.reads.end())) },
            { "writes", QJsonArray::fromStringList(QStringList(record.writes.begin(), record.writes.end())) },
            { "startedAt", static_cast<double>(record.startedAt) },
            { "idempotencyKey", record.idempotencyKey },
            { "createdAt", static_cast<double>(record.queuedAt) },
            { "finishedAt", static_cast<double>(record.finishedAt) },
        };
        tasks.append(object);
    }
    QSaveFile file(journalPath());
    if (!file.open(QIODevice::WriteOnly)) return;
    file.write(QJsonDocument(tasks).toJson(QJsonDocument::Compact));
    file.commit();
}
