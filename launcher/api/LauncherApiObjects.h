// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <QJsonObject>
#include <QPointer>
#include <QJsonArray>
#include <QCoreApplication>
#include <QFileInfo>
#include <QUrl>

#include <memory>

#include "Application.h"
#include "BaseInstance.h"
#include "InstanceList.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "minecraft/auth/AccountList.h"
#include "minecraft/auth/MinecraftAccount.h"
#include "java/JavaInstall.h"
#include "java/JavaInstallList.h"
#include "tasks/Task.h"
#include "tasks/ConcurrentTask.h"
#include "minecraft/mod/Resource.h"
#include "minecraft/mod/ResourceFolderModel.h"

/**
 * Object handles used by the transport-neutral API.
 *
 * The GUI and the API both operate on the same BaseInstance objects.  Keeping
 * a guarded handle here prevents each API domain from resolving references and
 * rebuilding the public instance shape independently.
 */
namespace LauncherApiModel {

class LauncherObject {
   public:
    LauncherObject() : m_application(APPLICATION) {}
    explicit LauncherObject(Application* application) : m_application(application) {}

    bool isValid() const { return !m_application.isNull(); }

    QJsonObject snapshot() const
    {
        if (!isValid())
            return {};
        QJsonArray capabilities;
        const auto caps = m_application->capabilities();
        if (caps & Application::SupportsMSA) capabilities.append("microsoft-auth");
        if (caps & Application::SupportsFlame) capabilities.append("curseforge");
        if (caps & Application::SupportsGameMode) capabilities.append("game-mode");
        if (caps & Application::SupportsMangoHud) capabilities.append("mangohud");
        return QJsonObject{ { "applicationVersion", QCoreApplication::applicationVersion() },
                            { "root", m_application->root() },
                            { "dataRoot", m_application->dataRoot() },
                            { "javaRoot", m_application->javaPath() },
                            { "portable", m_application->isPortable() },
                            { "headless", m_application->isHeadless() },
                            { "capabilities", capabilities } };
    }

   private:
    QPointer<Application> m_application;
};

class AccountObject {
   public:
    AccountObject() = default;
    explicit AccountObject(MinecraftAccount* account) : m_account(account) {}

    static AccountObject resolve(const QString& reference)
    {
        auto* list = APPLICATION->accounts();
        for (int i = 0; i < list->count(); ++i) {
            const auto account = list->at(i);
            if (account->profileId() == reference || account->internalId() == reference ||
                account->profileName().compare(reference, Qt::CaseInsensitive) == 0)
                return AccountObject(account.get());
        }
        return {};
    }

    MinecraftAccount* get() const { return m_account.data(); }
    explicit operator bool() const { return !m_account.isNull(); }

    QJsonObject snapshot(bool isDefault = false) const
    {
        auto* account = get();
        if (!account)
            return {};
        const auto* data = account->accountData();
        QString stateName;
        switch (data->accountState) {
            case AccountState::Unchecked: stateName = "unchecked"; break;
            case AccountState::Offline: stateName = "offline"; break;
            case AccountState::Working: stateName = "working"; break;
            case AccountState::Online: stateName = "online"; break;
            case AccountState::Disabled: stateName = "disabled"; break;
            case AccountState::Errored: stateName = "errored"; break;
            case AccountState::Expired: stateName = "expired"; break;
            case AccountState::Gone: stateName = "gone"; break;
        }
        QJsonObject result{ { "id", account->profileId() },
                            { "internalId", account->internalId() },
                            { "profileName", account->profileName() },
                            { "displayName", account->displayName() },
                            { "type", account->typeString() },
                            { "state", static_cast<int>(data->accountState) },
                            { "stateName", stateName },
                            { "active", account->isActive() },
                            { "default", isDefault },
                            { "ownsMinecraft", account->ownsMinecraft() },
                            { "canPlayMinecraft", data->minecraftEntitlement.canPlayMinecraft },
                            { "hasProfile", account->hasProfile() },
                            { "lastError", account->lastError() },
                            { "profile", QJsonObject{ { "id", data->minecraftProfile.id },
                                                        { "name", data->minecraftProfile.name },
                                                        { "skin", QJsonObject{ { "id", data->minecraftProfile.skin.id },
                                                                                  { "url", data->minecraftProfile.skin.url },
                                                                                  { "variant", data->minecraftProfile.skin.variant } } },
                                                        { "currentCape", data->minecraftProfile.currentCape } } } };
        QJsonArray capes;
        for (auto it = data->minecraftProfile.capes.constBegin(); it != data->minecraftProfile.capes.constEnd(); ++it)
            capes.append(QJsonObject{ { "id", it.value().id }, { "url", it.value().url }, { "alias", it.value().alias },
                                      { "current", it.key() == data->minecraftProfile.currentCape } });
        auto profile = result.value("profile").toObject();
        profile.insert("capes", capes);
        result.insert("profile", profile);
        if (data->type == AccountType::Yggdrasil) {
            result.insert("yggdrasil", QJsonObject{ { "authUrl", data->yggdrasilConfig.authServerUrl },
                                                       { "sessionUrl", data->yggdrasilConfig.sessionServerUrl },
                                                       { "sourceName", data->yggdrasilConfig.sourceName },
                                                       { "authenticateEndpoint", data->yggdrasilConfig.authenticateEndpoint },
                                                       { "refreshEndpoint", data->yggdrasilConfig.refreshEndpoint },
                                                       { "validateEndpoint", data->yggdrasilConfig.validateEndpoint },
                                                       { "profileEndpoint", data->yggdrasilConfig.profileEndpoint },
                                                       { "oauthTokenEndpoint", data->yggdrasilConfig.oauthTokenEndpoint },
                                                       { "tokenType", data->yggdrasilConfig.tokenType == YggdrasilTokenType::OAuth ? "OAuth" : "Standard" } });
        } else if (data->type == AccountType::UnifiedPass) {
            result.insert("unifiedPass", QJsonObject{ { "serverId", data->unifiedPassConfig.serverId },
                                                        { "baseUrl", data->unifiedPassConfig.baseUrl },
                                                        { "serverName", data->unifiedPassConfig.serverName },
                                                        { "serverIP", data->unifiedPassConfig.serverIP },
                                                        { "jarHash", data->unifiedPassConfig.jarHash } });
        }
        return result;
    }

   private:
    QPointer<MinecraftAccount> m_account;
};

class JavaObject {
   public:
    JavaObject() = default;
    explicit JavaObject(JavaInstallPtr java) : m_java(std::move(java)) {}

    static JavaObject resolve(const QString& reference)
    {
        auto* list = APPLICATION->javalist();
        for (int i = 0; i < list->count(); ++i) {
            auto java = std::dynamic_pointer_cast<JavaInstall>(list->at(i));
            if (java && (java->path == reference || java->id.toString() == reference))
                return JavaObject(std::move(java));
        }
        return {};
    }

    bool isValid() const { return static_cast<bool>(m_java); }
    const JavaInstallPtr& get() const { return m_java; }

    QJsonObject snapshot() const
    {
        if (!isValid())
            return {};
        return QJsonObject{ { "id", m_java->id.toString() },
                            { "version", m_java->id.toString() },
                            { "architecture", m_java->arch },
                            { "path", m_java->path },
                            { "is64Bit", m_java->is_64bit } };
    }

   private:
    JavaInstallPtr m_java;
};

class TaskObject {
   public:
    TaskObject() = default;
    explicit TaskObject(Task* task) : m_task(task) {}

    Task* get() const { return m_task.data(); }
    bool isValid() const { return !m_task.isNull(); }

    QJsonObject snapshot(const QString& taskId = {}) const
    {
        auto* task = get();
        if (!task)
            return taskId.isEmpty() ? QJsonObject{} : QJsonObject{ { "id", taskId } };
        QJsonArray warnings;
        for (const auto& warning : task->warnings()) warnings.append(warning);
        QJsonArray steps;
        for (const auto& progress : task->getStepProgress()) {
            if (!progress) continue;
            steps.append(QJsonObject{ { "id", progress->uid.toString(QUuid::WithoutBraces) },
                                      { "label", progress->label },
                                      { "current", progress->current }, { "total", progress->total },
                                      { "transferRate", progress->transferRate },
                                      { "status", progress->status }, { "details", progress->details },
                                      { "state", stepStateName(progress->state) },
                                      { "oldCurrent", progress->old_current }, { "oldTotal", progress->old_total },
                                      { "finished", progress->isDone() } });
        }
        const auto id = taskId.isEmpty() ? task->getUid().toString(QUuid::WithoutBraces) : taskId;
        QJsonArray children;
        for (auto* child : task->getSubTasks()) {
            if (!child || child == task) continue;
            children.append(TaskObject(child).snapshot());
        }
        QJsonObject result{ { "id", id },
                            { "type", QString::fromLatin1(task->metaObject()->className()) },
                            { "name", task->objectName() },
                            { "state", stateName(task->getState()) },
                            { "running", task->isRunning() }, { "finished", task->isFinished() },
                            { "successful", task->wasSuccessful() }, { "canAbort", task->canAbort() },
                            { "status", task->getStatus() }, { "details", task->getDetails() },
                            { "progress", task->getProgress() }, { "totalProgress", task->getTotalProgress() },
                            { "transferRate", task->getTransferRate() }, { "failReason", task->failReason() },
                            { "warnings", warnings }, { "steps", steps }, { "children", children } };

        // Download tasks use an internal object name such as FILE:<url> or
        // ARIA2_CACHE:<url>. Keep the useful, non-sensitive part available to
        // UI clients without exposing signed query strings in the API.
        const auto objectName = task->objectName();
        const QList<QPair<QString, QString>> sourcePrefixes = {
            { "ARIA2_FILE:", "aria2" }, { "ARIA2_CACHE:", "aria2-cache" },
            { "PCL_FILE:", "pcl" },     { "PCL_CACHE:", "pcl-cache" },
            { "FILE:", "qt" },          { "CACHE:", "cache" },
            { "BYTES:", "memory" },
        };
        for (const auto& prefix : sourcePrefixes) {
            if (!objectName.startsWith(prefix.first))
                continue;
            const QUrl url(objectName.mid(prefix.first.size()));
            const auto fileName = QFileInfo(url.path()).fileName();
            result.insert("source", QJsonObject{ { "kind", "download" },
                                                  { "transport", prefix.second },
                                                  { "fileName", fileName } });
            break;
        }
        return result;
    }

   private:
    static QString stateName(Task::State state)
    {
        switch (state) {
            case Task::State::Inactive: return "inactive";
            case Task::State::Running: return "running";
            case Task::State::Succeeded: return "succeeded";
            case Task::State::Failed: return "failed";
            case Task::State::AbortedByUser: return "aborted";
        }
        return "unknown";
    }

    static QString stepStateName(TaskStepState state)
    {
        switch (state) {
            case TaskStepState::Waiting: return "waiting";
            case TaskStepState::Running: return "running";
            case TaskStepState::Failed: return "failed";
            case TaskStepState::Succeeded: return "succeeded";
        }
        return "unknown";
    }

    QPointer<Task> m_task;
};

class ResourceObject {
   public:
    ResourceObject() = default;
    explicit ResourceObject(Resource* resource) : m_resource(resource) {}
    ResourceObject(ResourceFolderModel* model, Resource* resource) : m_model(model), m_resource(resource) {}

    bool isValid() const { return !m_resource.isNull(); }
    Resource* get() const { return m_resource.data(); }
    ResourceFolderModel* model() const { return m_model.data(); }

    QJsonObject snapshot() const
    {
        auto* resource = get();
        if (!isValid()) return {};
        return QJsonObject{ { "id", resource->internal_id() }, { "name", resource->name() },
                            { "fileName", resource->fileinfo().fileName() },
                            { "originalFileName", resource->getOriginalFileName() },
                            { "enabled", resource->enabled() }, { "size", resource->sizeInfo() },
                            { "sizeString", resource->sizeStr() }, { "path", resource->fileinfo().absoluteFilePath() },
                            { "modified", resource->dateTimeChanged().toString(Qt::ISODate) },
                            { "provider", resource->provider() }, { "homepage", resource->homepage() },
                            { "valid", resource->valid() }, { "resolved", resource->isResolved() },
                            { "resolving", resource->isResolving() } };
    }

   private:
    QPointer<ResourceFolderModel> m_model;
    QPointer<Resource> m_resource;
};

class InstanceObject {
   public:
    InstanceObject() = default;
    explicit InstanceObject(BaseInstance* instance) : m_instance(instance) {}

    static InstanceObject resolve(const QString& reference)
    {
        auto* list = APPLICATION->instances();
        auto* instance = list->getInstanceById(reference);
        if (!instance)
            instance = list->getInstanceByManagedName(reference);
        if (!instance) {
            for (int i = 0; i < list->count(); ++i) {
                if (list->at(i)->name().compare(reference, Qt::CaseInsensitive) == 0) {
                    instance = list->at(i);
                    break;
                }
            }
        }
        return InstanceObject(instance);
    }

    BaseInstance* get() const { return m_instance.data(); }
    explicit operator bool() const { return !m_instance.isNull(); }

    QJsonObject snapshot() const
    {
        auto* instance = get();
        if (!instance)
            return {};
        const auto id = instance->id();
        QJsonObject result{
            { "id", id },
            { "name", instance->name() },
            { "type", instance->typeName() },
            { "group", APPLICATION->instances()->getInstanceGroup(id) },
            { "icon", instance->iconKey() },
            { "notes", instance->notes() },
            { "paths", QJsonObject{ { "root", instance->instanceRoot() },
                                    { "game", instance->gameRoot() },
                                    { "mods", instance->modsRoot() } } },
            { "lifecycle", QJsonObject{ { "running", instance->isRunning() },
                                         { "lastLaunch", instance->lastLaunch() },
                                         { "playTimeSeconds", static_cast<qint64>(instance->totalTimePlayed()) } } },
            { "managed", QJsonObject{ { "enabled", instance->isManagedPack() },
                                       { "type", instance->getManagedPackType() },
                                       { "projectId", instance->getManagedPackID() },
                                       { "versionId", instance->getManagedPackVersionID() } } },
            { "capabilities", QJsonObject{ { "launch", instance->canLaunch() },
                                            { "stop", instance->isRunning() },
                                            { "resources", true },
                                            { "settings", true } } } };
        if (const auto minecraft = dynamic_cast<MinecraftInstance*>(instance))
            result.insert("runtime", QJsonObject{ { "minecraftVersion", minecraft->getPackProfile()->getComponentVersion("net.minecraft") } });
        else
            result.insert("runtime", QJsonObject{});
        return result;
    }

   private:
    QPointer<BaseInstance> m_instance;
};

}  // namespace LauncherApiModel
