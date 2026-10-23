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
#include "meta/Index.h"
#include "minecraft/VanillaInstanceCreationTask.h"
#include "minecraft/PackProfile.h"
#include "net/Mode.h"
#include "api/LauncherApi.h"
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

class NoopInteraction final : public UserInteraction {
   public:
    void status(const QString&) override {}
    void deviceCode(const QString&, const QString&, int) override {}
    std::optional<QString> input(const QString&, bool) override { return std::nullopt; }
    std::optional<int> select(const QString&, const QJsonArray&) override { return std::nullopt; }
};

}  // namespace

NeoTaskService& NeoTaskService::instance()
{
    static NeoTaskService service;
    return service;
}

NeoTaskService::NeoTaskService(QObject* parent) : QObject(parent), m_temp() {}

QString NeoTaskService::conflictKeyFor(const QString& operation, const QJsonObject& parameters) const
{
    if (operation == "instance.import" || operation == "instance.create" || operation == "modpack.install") return "instances";
    if (operation == "resource.install" || operation == "instance.component.set-version") {
        return "instance:" + parameters.value("instance").toString();
    }
    return "global";
}

QJsonObject NeoTaskService::start(const QString& operation, const QJsonObject& parameters, UserInteraction& interaction)
{
    Q_UNUSED(interaction);
    if (!QStringList{ "instance.import", "instance.create", "instance.component.set-version", "modpack.install", "resource.install" }.contains(operation)) {
        return { { "ok", false }, { "error", QStringLiteral("Unsupported neo task operation: %1").arg(operation) } };
    }
    const auto id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    Record record;
    record.id = id;
    record.operation = operation;
    record.parameters = parameters;
    record.conflictKey = conflictKeyFor(operation, parameters);
    record.highPriority = parameters.value("priority").toString() == "high";
    if (operation == "instance.import" || operation == "instance.create" || operation == "modpack.install") {
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
    if (record.operation == "instance.component.set-version") {
        const auto parameters = record.parameters;
        const auto instance = dynamic_cast<MinecraftInstance*>(findInstance(parameters.value("instance").toString()));
        if (!instance) { finish(id, false, "Minecraft instance not found."); return; }
        if (instance->isRunning()) { finish(id, false, "Components cannot be modified while the instance is running."); return; }
        const auto uid = parameters.value("component").toString().trimmed();
        const auto version = parameters.value("version").toString().trimmed();
        if (uid.isEmpty() || version.isEmpty()) { finish(id, false, "A component id and version are required."); return; }
        auto profile = instance->getPackProfile();
        if (!profile->isLoaded()) {
            const auto loaded = profile->reload(Net::Mode::Offline);
            if (!loaded) { finish(id, false, loaded.error); return; }
        }
        if (!profile->setComponentVersion(uid, version, parameters.value("important").toBool(false))) {
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
        connect(task.get(), &Task::finished, this, [this, id, profile, instanceId = instance->id()]() {
            if (!m_records.contains(id) || m_records[id].finished) return;
            const bool success = profile->getCurrentTask() ? profile->getCurrentTask()->wasSuccessful() : true;
            if (success) profile->saveNow();
            finish(id, success, success ? QString() : (profile->getCurrentTask() ? profile->getCurrentTask()->failReason() : "Component resolution failed."), instanceId);
        });
        task->start();
        return;
    }

    if (record.operation == "modpack.install") {
        const auto parameters = record.parameters;
        const auto provider = parameters.value("provider").toString().toLower();
        const auto projectId = parameters.value("projectId").toString();
        const auto versionId = parameters.value("versionId").toString();
        if (provider != "modrinth" && provider != "curseforge") {
            finish(id, false, "Async modpack install currently supports Modrinth and CurseForge.");
            return;
        }
        if (projectId.isEmpty() || versionId.isEmpty()) {
            finish(id, false, "A pack project and version are required.");
            return;
        }
        NoopInteraction interaction;
        LauncherApi api;
        const QJsonObject request{ { "provider", provider }, { "kind", "modpacks" }, { "projectId", projectId } };
        const auto versions = api.execute("resource.versions", request, interaction);
        if (!versions.value("ok").toBool()) {
            finish(id, false, versions.value("error").toString("Could not read pack versions."));
            return;
        }
        QString download;
        for (const auto& value : versions.value("data").toArray()) {
            const auto entry = value.toObject();
            if (entry.value("versionId").toString() == versionId) {
                download = entry.value("downloadUrl").toString();
                break;
            }
        }
        if (download.isEmpty()) {
            finish(id, false, "The provider did not supply a downloadable pack archive.");
            return;
        }
        const QUrl url(download);
        if (!url.isValid() || (url.scheme() != "http" && url.scheme() != "https")) {
            finish(id, false, "The provider returned an invalid pack URL.");
            return;
        }
        auto* importTask = new InstanceImportTask(url, nullptr, { { "pack_id", projectId }, { "pack_version_id", versionId } });
        const auto name = parameters.value("name").toString();
        if (!name.isEmpty()) importTask->setName(name);
        auto* task = APPLICATION->instances()->wrapInstanceTask(importTask);
        if (!task) {
            finish(id, false, "The modpack import task could not be created.");
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
                    if (!before.contains(candidate->id())) { created = candidate->id(); break; }
                }
            }
            if (!created.isEmpty()) m_records[id].createdInstanceId = created;
            finish(id, success, success ? QString() : task->failReason());
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
        const auto loader = requestedLoader == "fabric" ? "net.fabricmc.fabric-loader" :
                            requestedLoader == "quilt" ? "org.quiltmc.quilt-loader" :
                            requestedLoader == "forge" ? "net.minecraftforge" :
                            requestedLoader == "neoforge" ? "net.neoforged" :
                            requestedLoader == "cleanroom" ? "com.cleanroommc.cleanroom" : requestedLoader;

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
            auto createWrapped = [this, id, minecraft, name, group, icon](BaseVersion::Ptr loaderVersion) {
                if (!m_records.contains(id) || m_records[id].finished) return;
                auto* raw = loaderVersion ? new VanillaCreationTask(minecraft, m_records[id].parameters.value("loader").toString(), loaderVersion)
                                          : new VanillaCreationTask(minecraft);
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
    // Mark terminal before aborting. Qt tasks may emit finished synchronously
    // from abort(), and their callbacks must not overwrite the cancelled state.
    const bool wasActive = record.state == "running";
    record.finished = true;
    record.state = "cancelled";
    record.error = "cancelled";
    record.status = "已取消";
    if (wasActive) {
        m_activeKeys.remove(record.conflictKey);
        m_active = qMax(0, m_active - 1);
    }
    if (record.task) record.task->abort();
    pump();
    return { { "ok", true }, { "data", snapshot(record) } };
}
