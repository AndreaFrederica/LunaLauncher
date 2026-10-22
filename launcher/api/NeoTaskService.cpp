// SPDX-License-Identifier: GPL-3.0-only

#include "NeoTaskService.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QUrl>

#include <memory>

#include "Application.h"
#include "BaseInstance.h"
#include "FileSystem.h"
#include "InstanceImportTask.h"
#include "InstanceList.h"
#include "api/LauncherApiSupport.h"
#include "api/ResourceModelSupport.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/mod/ModFolderModel.h"
#include "minecraft/mod/PluginFolderModel.h"
#include "minecraft/mod/ResourceFolderModel.h"
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

}  // namespace

NeoTaskService& NeoTaskService::instance()
{
    static NeoTaskService service;
    return service;
}

NeoTaskService::NeoTaskService(QObject* parent) : QObject(parent), m_temp() {}

QString NeoTaskService::conflictKeyFor(const QString& operation, const QJsonObject& parameters) const
{
    if (operation == "instance.import" || operation == "instance.create") return "instances";
    if (operation == "resource.install") {
        return "instance:" + parameters.value("instance").toString();
    }
    return "global";
}

QJsonObject NeoTaskService::start(const QString& operation, const QJsonObject& parameters, UserInteraction& interaction)
{
    Q_UNUSED(interaction);
    if (!QStringList{ "instance.import", "instance.create", "resource.install" }.contains(operation)) {
        return { { "ok", false }, { "error", QStringLiteral("Unsupported neo task operation: %1").arg(operation) } };
    }
    const auto id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    Record record;
    record.id = id;
    record.operation = operation;
    record.parameters = parameters;
    record.conflictKey = conflictKeyFor(operation, parameters);
    record.highPriority = parameters.value("priority").toString() == "high";
    if (operation == "instance.import") {
        for (int i = 0; i < APPLICATION->instances()->count(); ++i)
            record.instancesBefore.insert(APPLICATION->instances()->at(i)->id());
    }
    m_records.insert(id, record);
    m_order.append(id);
    m_pending.enqueue({ id, operation, parameters, record.conflictKey, record.highPriority });
    pump();
    return { { "ok", true }, { "data", snapshot(m_records.value(id)) } };
}

void NeoTaskService::pump()
{
    while (m_active < 3 && !m_pending.isEmpty()) {
        int index = -1;
        for (int i = 0; i < m_pending.size(); ++i) {
            if (m_pending.at(i).highPriority && !m_activeKeys.contains(m_pending.at(i).conflictKey)) {
                index = i;
                break;
            }
        }
        if (index < 0) {
            for (int i = 0; i < m_pending.size(); ++i) {
                if (!m_activeKeys.contains(m_pending.at(i).conflictKey)) {
                    index = i;
                    break;
                }
            }
        }
        if (index < 0) break;
        const auto pending = m_pending.takeAt(index);
        auto& record = m_records[pending.id];
        record.state = "running";
        record.conflictKey = pending.conflictKey;
        m_activeKeys.insert(record.conflictKey);
        ++m_active;
        startRecord(record);
    }
}

void NeoTaskService::startRecord(const Record& record)
{
    const auto id = record.id;
    if (record.operation == "instance.import") {
        const auto parameters = record.parameters;
        const auto source = parameters.value("source").toString().trimmed();
        QUrl url(source, QUrl::TolerantMode);
        if (QFileInfo::exists(source)) {
            url = QUrl::fromLocalFile(QFileInfo(source).absoluteFilePath());
        } else if (url.scheme() != "http" && url.scheme() != "https") {
            finish(id, false, "A local pack file or HTTP(S) URL is required.");
            return;
        }
        if (source.isEmpty() || !url.isValid()) {
            finish(id, false, "A valid local path or URL is required.");
            return;
        }
        auto* importTask = new InstanceImportTask(url, nullptr);
        const auto name = parameters.value("name").toString();
        if (!name.isEmpty()) importTask->setName(name);
        auto* task = APPLICATION->instances()->wrapInstanceTask(importTask);
        if (!task) {
            finish(id, false, "The import task could not be created.");
            return;
        }
        auto& stored = m_records[id];
        stored.task = task;
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
                    if (!before.contains(candidate->id())) {
                        created = candidate->id();
                        break;
                    }
                }
            }
            if (!created.isEmpty()) m_records[id].createdInstanceId = created;
            finish(id, success, success ? QString() : task->failReason());
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

void NeoTaskService::finish(const QString& taskId, bool success, const QString& error, const QString& instanceId)
{
    if (!m_records.contains(taskId)) return;
    auto& record = m_records[taskId];
    if (record.finished) return;
    record.finished = true;
    record.state = success ? "succeeded" : (error.contains("cancel", Qt::CaseInsensitive) ? "cancelled" : "failed");
    record.error = error;
    if (!instanceId.isEmpty()) record.instanceId = instanceId;
    m_activeKeys.remove(record.conflictKey);
    m_active = qMax(0, m_active - 1);
    pump();
}

QJsonObject NeoTaskService::snapshot(const Record& record) const
{
    return QJsonObject{
        { "taskId", record.id },
        { "operation", record.operation },
        { "state", record.state },
        { "status", record.status },
        { "error", record.error },
        { "progress", static_cast<double>(record.progress) },
        { "total", static_cast<double>(record.total) },
        { "finished", record.finished },
        { "conflictKey", record.conflictKey },
        { "instanceId", record.instanceId },
        { "createdInstanceId", record.createdInstanceId },
    };
}

QJsonObject NeoTaskService::status(const QString& taskId) const
{
    if (!m_records.contains(taskId)) return { { "ok", false }, { "error", "Unknown neo task." } };
    return { { "ok", true }, { "data", snapshot(m_records.value(taskId)) } };
}

QJsonObject NeoTaskService::list() const
{
    QJsonArray tasks;
    for (auto it = m_order.crbegin(); it != m_order.crend(); ++it) {
        if (m_records.contains(*it)) tasks.append(snapshot(m_records.value(*it)));
    }
    return { { "ok", true }, { "data", QJsonObject{ { "tasks", tasks } } } };
}

QJsonObject NeoTaskService::cancel(const QString& taskId)
{
    if (!m_records.contains(taskId)) return { { "ok", false }, { "error", "Unknown neo task." } };
    auto& record = m_records[taskId];
    if (record.finished) return { { "ok", true }, { "data", snapshot(record) } };
    if (record.task) record.task->abort();
    finish(taskId, false, "cancelled");
    return { { "ok", true }, { "data", snapshot(record) } };
}
