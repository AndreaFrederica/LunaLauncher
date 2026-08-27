// SPDX-License-Identifier: GPL-3.0-only

#include "OperationService.h"

#include <QCryptographicHash>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QUrlQuery>
#include <QVariant>

#include "Application.h"
#include "BaseInstance.h"
#include "FileSystem.h"
#include "InstanceCopyPrefs.h"
#include "InstanceCopyTask.h"
#include "InstanceImportTask.h"
#include "InstanceList.h"
#include "LaunchController.h"
#include "java/JavaInstall.h"
#include "java/JavaInstallList.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "minecraft/auth/AccountData.h"
#include "minecraft/auth/AccountList.h"
#include "minecraft/auth/AuthFlow.h"
#include "minecraft/auth/MinecraftAccount.h"
#include "minecraft/launch/MinecraftTarget.h"
#include "minecraft/mod/Resource.h"
#include "minecraft/mod/ResourceFolderModel.h"
#include "modplatform/ModApiMirror.h"
#include "modplatform/flame/CurseForgeDownloadPageService.h"
#include "net/ApiDownload.h"
#include "net/Download.h"
#include "net/NetJob.h"
#include "net/RawHeaderProxy.h"
#include "net/Upload.h"
#include "settings/Setting.h"
#include "settings/SettingsObject.h"
#include "tasks/SequentialTask.h"
#include "tasks/Task.h"

namespace {

QString accountTypeName(AccountType type)
{
    switch (type) {
        case AccountType::MSA:
            return "microsoft";
        case AccountType::Offline:
            return "offline";
        case AccountType::Yggdrasil:
            return "yggdrasil";
        case AccountType::UnifiedPass:
            return "unified-pass";
    }
    return "unknown";
}

QString accountStateName(AccountState state)
{
    switch (state) {
        case AccountState::Unchecked:
            return "unchecked";
        case AccountState::Offline:
            return "offline";
        case AccountState::Working:
            return "working";
        case AccountState::Online:
            return "online";
        case AccountState::Disabled:
            return "disabled";
        case AccountState::Errored:
            return "errored";
        case AccountState::Expired:
            return "expired";
        case AccountState::Gone:
            return "gone";
    }
    return "unknown";
}

QString hashName(int algorithm)
{
    return algorithm == 2 ? QStringLiteral("md5") : QStringLiteral("sha1");
}

QCryptographicHash::Algorithm hashAlgorithm(const QString& name)
{
    return name == "md5" ? QCryptographicHash::Md5 : QCryptographicHash::Sha1;
}

BaseInstance* findInstance(const QString& reference)
{
    auto list = APPLICATION->instances();
    auto instance = list->getInstanceById(reference);
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
    return instance;
}

MinecraftAccountPtr findAccount(const QString& reference, int* row = nullptr)
{
    const auto accounts = APPLICATION->accounts();
    for (int i = 0; i < accounts->count(); ++i) {
        const auto account = accounts->at(i);
        if (account->profileId() == reference || account->internalId() == reference ||
            account->profileName().compare(reference, Qt::CaseInsensitive) == 0) {
            if (row)
                *row = i;
            return account;
        }
    }
    return nullptr;
}

std::shared_ptr<ResourceFolderModel> findResourceModel(MinecraftInstance* instance, QString kind)
{
    kind = kind.toLower().remove('-').remove('_');
    static const QMap<QString, int> indexes{ { "mods", 0 },           { "coremods", 1 },           { "nilmods", 2 },
                                             { "resourcepacks", 3 },  { "texturepacks", 4 },       { "shaderpacks", 5 },
                                             { "yesstevemodels", 6 }, { "customplayermodels", 7 }, { "schematics", 8 },
                                             { "datapacks", 9 } };
    if (!indexes.contains(kind))
        return nullptr;
    const auto lists = instance->resourceLists();
    const auto index = indexes.value(kind);
    return index < lists.size() ? lists.at(index) : nullptr;
}

QString resourceTypeName(ResourceType type)
{
    switch (type) {
        case ResourceType::ZIPFILE:
            return "archive";
        case ResourceType::SINGLEFILE:
            return "file";
        case ResourceType::FOLDER:
            return "folder";
        case ResourceType::LITEMOD:
            return "litemod";
        case ResourceType::UNKNOWN:
            return "unknown";
    }
    return "unknown";
}

std::unique_ptr<Resource> findResource(const std::shared_ptr<ResourceFolderModel>& model, const QString& reference)
{
    const auto entries = model->dir().entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const auto& entry : entries) {
        if (entry.fileName() == ".index")
            continue;
        auto resource = std::make_unique<Resource>(entry);
        if (resource->internal_id() == reference || resource->fileinfo().fileName().compare(reference, Qt::CaseInsensitive) == 0 ||
            resource->getOriginalFileName().compare(reference, Qt::CaseInsensitive) == 0 ||
            resource->name().compare(reference, Qt::CaseInsensitive) == 0)
            return resource;
    }
    return {};
}

bool isSensitiveSetting(const QString& id)
{
    const auto lower = id.toLower();
    return lower.endsWith("password") || lower.endsWith("proxypass") || lower.contains("token") || lower.contains("secret") ||
           lower.endsWith("keyoverride");
}

QJsonValue variantToJson(const QVariant& value)
{
    if (!value.isValid() || value.isNull())
        return QJsonValue::Null;
    if (value.metaType().flags().testFlag(QMetaType::IsEnumeration))
        return value.toInt();
    if (value.metaType().id() == QMetaType::QByteArray)
        return QString::fromLatin1(value.toByteArray().toBase64());
    const auto converted = QJsonValue::fromVariant(value);
    return converted.isUndefined() ? QJsonValue(value.toString()) : converted;
}

QString variantTypeName(const QVariant& value, const QVariant& fallback)
{
    const auto type = value.isValid() ? value.metaType() : fallback.metaType();
    return type.isValid() && type.name() ? QString::fromLatin1(type.name()) : QStringLiteral("QVariant");
}

QJsonObject describeSetting(SettingsObject* settings, const std::shared_ptr<Setting>& setting, bool reveal)
{
    const auto value = settings->get(setting->id());
    const auto defaultValue = setting->defValue();
    const bool sensitive = isSensitiveSetting(setting->id());
    const bool redacted = sensitive && !reveal;
    return { { "key", setting->id() },
             { "type", variantTypeName(value, defaultValue) },
             { "value", redacted ? QJsonValue(QStringLiteral("***")) : variantToJson(value) },
             { "default", redacted ? QJsonValue(QStringLiteral("***")) : variantToJson(defaultValue) },
             { "isDefault", value == defaultValue },
             { "sensitive", sensitive },
             { "redacted", redacted } };
}

bool convertSettingValue(const QJsonValue& input, const QVariant& reference, QVariant* output, QString* error)
{
    if (!reference.isValid()) {
        *output = input.toVariant();
        return true;
    }

    const auto type = reference.metaType();
    const auto fail = [error, type] {
        if (error)
            *error = QObject::tr("The value cannot be converted to %1.").arg(QString::fromLatin1(type.name()));
        return false;
    };

    if (type.id() == QMetaType::QString) {
        if (input.isString()) {
            *output = input.toString();
        } else if (input.isBool()) {
            *output = input.toBool() ? QStringLiteral("true") : QStringLiteral("false");
        } else if (input.isDouble()) {
            *output = QString::number(input.toDouble(), 'g', 16);
        } else {
            return fail();
        }
        return true;
    }
    if (type.id() == QMetaType::Bool) {
        if (input.isBool()) {
            *output = input.toBool();
            return true;
        }
        if (input.isDouble() && (input.toDouble() == 0 || input.toDouble() == 1)) {
            *output = input.toDouble() == 1;
            return true;
        }
        const auto text = input.toString().trimmed().toLower();
        if (text == "true" || text == "yes" || text == "on" || text == "1") {
            *output = true;
            return true;
        }
        if (text == "false" || text == "no" || text == "off" || text == "0") {
            *output = false;
            return true;
        }
        return fail();
    }
    if (type.id() == QMetaType::QStringList) {
        if (!input.isArray())
            return fail();
        QStringList values;
        for (const auto& item : input.toArray()) {
            if (!item.isString())
                return fail();
            values.append(item.toString());
        }
        *output = values;
        return true;
    }
    if (type.id() == QMetaType::QByteArray) {
        if (!input.isString())
            return fail();
        *output = QByteArray::fromBase64(input.toString().toLatin1());
        return true;
    }

    QVariant converted = input.toVariant();
    if (type.flags().testFlag(QMetaType::IsEnumeration)) {
        bool ok = input.isDouble();
        const auto value = input.isDouble() ? static_cast<int>(input.toDouble()) : input.toString().toInt(&ok);
        if (!ok)
            return fail();
        converted = value;
    }
    if (!converted.convert(type))
        return fail();
    *output = converted;
    return true;
}

}  // namespace

OperationService::OperationService(QObject* parent) : QObject(parent) {}

void OperationService::cancelCurrent()
{
    if (m_currentTask)
        m_currentTask->abort();
}

QJsonObject OperationService::success(const QJsonValue& data)
{
    return { { "ok", true }, { "exitCode", 0 }, { "data", data } };
}

QJsonObject OperationService::failure(const QString& message, int exitCode)
{
    return { { "ok", false }, { "exitCode", exitCode }, { "error", message } };
}

QJsonObject OperationService::execute(const QString& operation, const QJsonObject& parameters, UserInteraction& interaction)
{
    if (operation == "instance.list")
        return listInstances();
    if (operation == "account.list")
        return listAccounts();
    if (operation == "account.set-default")
        return setDefaultAccount(parameters);
    if (operation == "account.remove")
        return removeAccount(parameters);
    if (operation == "account.refresh")
        return refreshAccount(parameters, interaction);
    if (operation == "instance.info")
        return instanceInfo(parameters);
    if (operation == "instance.rename")
        return renameInstance(parameters);
    if (operation == "instance.group")
        return groupInstance(parameters);
    if (operation == "instance.copy")
        return copyInstance(parameters, interaction);
    if (operation == "instance.update")
        return updateInstance(parameters, interaction);
    if (operation == "instance.delete")
        return deleteInstance(parameters);
    if (operation == "instance.undo-delete")
        return undoDeleteInstance();
    if (operation == "resource.list")
        return listResources(parameters);
    if (operation == "resource.install")
        return installResource(parameters, interaction);
    if (operation == "resource.enable")
        return setResourceEnabled(parameters, true);
    if (operation == "resource.disable")
        return setResourceEnabled(parameters, false);
    if (operation == "resource.remove")
        return removeResource(parameters);
    if (operation == "java.list")
        return listJava(interaction);
    if (operation == "settings.list")
        return listSettings(parameters);
    if (operation == "settings.get")
        return getSetting(parameters);
    if (operation == "settings.set")
        return setSetting(parameters, interaction);
    if (operation == "settings.reset")
        return resetSetting(parameters);
    if (operation == "account.login")
        return loginAccount(parameters, interaction);
    if (operation == "instance.import")
        return importInstance(parameters, interaction);
    if (operation == "instance.launch")
        return launchInstance(parameters, interaction);
    return failure(tr("Unknown operation: %1").arg(operation), 2);
}

QJsonObject OperationService::listInstances()
{
    QJsonArray instances;
    auto list = APPLICATION->instances();
    for (int i = 0; i < list->count(); ++i) {
        const auto instance = list->at(i);
        instances.append(QJsonObject{ { "id", instance->id() },
                                      { "name", instance->name() },
                                      { "group", list->getInstanceGroup(instance->id()) },
                                      { "running", instance->isRunning() },
                                      { "lastLaunch", instance->lastLaunch() },
                                      { "playTimeSeconds", static_cast<qint64>(instance->totalTimePlayed()) } });
    }
    return success(instances);
}

QJsonObject OperationService::listAccounts()
{
    QJsonArray accounts;
    const auto list = APPLICATION->accounts();
    const auto defaultAccount = list->defaultAccount();
    for (int i = 0; i < list->count(); ++i) {
        const auto account = list->at(i);
        accounts.append(QJsonObject{ { "id", account->profileId() },
                                     { "profileName", account->profileName() },
                                     { "type", accountTypeName(account->accountType()) },
                                     { "state", accountStateName(account->accountState()) },
                                     { "default", account == defaultAccount },
                                     { "ownsMinecraft", account->ownsMinecraft() } });
    }
    return success(accounts);
}

QJsonObject OperationService::setDefaultAccount(const QJsonObject& parameters)
{
    const auto reference = parameters.value("account").toString();
    auto accounts = APPLICATION->accounts();
    if (reference.isEmpty() || reference == "-") {
        accounts->setDefaultAccount(nullptr);
        accounts->saveList();
        return success(QJsonObject{ { "default", QJsonValue::Null } });
    }
    const auto account = findAccount(reference);
    if (!account)
        return failure(tr("Account not found: %1").arg(reference), 2);
    accounts->setDefaultAccount(account);
    accounts->saveList();
    return success(QJsonObject{ { "id", account->profileId() }, { "profileName", account->profileName() }, { "default", true } });
}

QJsonObject OperationService::removeAccount(const QJsonObject& parameters)
{
    if (!parameters.value("confirm").toBool())
        return failure(tr("Account removal requires confirm=true."), 2);
    int row = -1;
    const auto reference = parameters.value("account").toString();
    const auto account = findAccount(reference, &row);
    if (!account)
        return failure(tr("Account not found: %1").arg(reference), 2);
    const auto profileName = account->profileName();
    auto accounts = APPLICATION->accounts();
    accounts->removeAccount(accounts->index(row, 0));
    accounts->saveList();
    return success(QJsonObject{ { "removed", profileName } });
}

QJsonObject OperationService::refreshAccount(const QJsonObject& parameters, UserInteraction& interaction)
{
    const auto reference = parameters.value("account").toString();
    const auto account = findAccount(reference);
    if (!account)
        return failure(tr("Account not found: %1").arg(reference), 2);
    auto task = account->refresh();
    connect(task.get(), &AuthFlow::authorizeWithBrowserWithExtra, this,
            [&interaction](const QString& url, const QString& code, int expiresIn) { interaction.deviceCode(url, code, expiresIn); });
    connect(task.get(), &AuthFlow::authorizeWithBrowser, this,
            [&interaction](const QUrl& url) { interaction.deviceCode(url.toString(), QString(), 0); });
    QString error;
    if (!waitForTask(task.get(), interaction, &error))
        return failure(error);
    APPLICATION->accounts()->saveList();
    return success(QJsonObject{ { "id", account->profileId() },
                                { "profileName", account->profileName() },
                                { "state", accountStateName(account->accountState()) } });
}

QJsonObject OperationService::instanceInfo(const QJsonObject& parameters)
{
    const auto reference = parameters.value("instance").toString();
    const auto instance = findInstance(reference);
    if (!instance)
        return failure(tr("Instance not found: %1").arg(reference), 2);
    QJsonObject data{ { "id", instance->id() },
                      { "name", instance->name() },
                      { "type", instance->typeName() },
                      { "group", APPLICATION->instances()->getInstanceGroup(instance->id()) },
                      { "root", instance->instanceRoot() },
                      { "gameRoot", instance->gameRoot() },
                      { "modsRoot", instance->modsRoot() },
                      { "icon", instance->iconKey() },
                      { "notes", instance->notes() },
                      { "running", instance->isRunning() },
                      { "managed", instance->isManagedPack() },
                      { "managedType", instance->getManagedPackType() },
                      { "managedId", instance->getManagedPackID() },
                      { "managedVersionId", instance->getManagedPackVersionID() },
                      { "lastLaunch", instance->lastLaunch() },
                      { "playTimeSeconds", static_cast<qint64>(instance->totalTimePlayed()) } };
    if (const auto minecraft = dynamic_cast<MinecraftInstance*>(instance))
        data.insert("minecraftVersion", minecraft->getPackProfile()->getComponentVersion("net.minecraft"));
    return success(data);
}

QJsonObject OperationService::renameInstance(const QJsonObject& parameters)
{
    const auto reference = parameters.value("instance").toString();
    const auto name = parameters.value("name").toString().trimmed();
    if (name.isEmpty())
        return failure(tr("A non-empty instance name is required."), 2);
    const auto instance = findInstance(reference);
    if (!instance)
        return failure(tr("Instance not found: %1").arg(reference), 2);
    instance->setName(name);
    instance->saveNow();
    return success(QJsonObject{ { "id", instance->id() }, { "name", instance->name() } });
}

QJsonObject OperationService::groupInstance(const QJsonObject& parameters)
{
    const auto reference = parameters.value("instance").toString();
    const auto instance = findInstance(reference);
    if (!instance)
        return failure(tr("Instance not found: %1").arg(reference), 2);
    auto group = parameters.value("group").toString().simplified();
    if (group == "-")
        group.clear();
    APPLICATION->instances()->setInstanceGroup(instance->id(), group);
    return success(QJsonObject{ { "id", instance->id() }, { "group", group } });
}

QJsonObject OperationService::copyInstance(const QJsonObject& parameters, UserInteraction& interaction)
{
    const auto reference = parameters.value("instance").toString();
    const auto instance = findInstance(reference);
    if (!instance)
        return failure(tr("Instance not found: %1").arg(reference), 2);
    const auto name = parameters.value("name").toString().trimmed();
    if (name.isEmpty())
        return failure(tr("A name for the copied instance is required."), 2);

    InstanceCopyPrefs prefs;
    if (parameters.contains("copySaves"))
        prefs.enableCopySaves(parameters.value("copySaves").toBool());
    if (parameters.contains("keepPlaytime"))
        prefs.enableKeepPlaytime(parameters.value("keepPlaytime").toBool());
    if (parameters.contains("copyMods"))
        prefs.enableCopyMods(parameters.value("copyMods").toBool());
    if (parameters.contains("copyResourcePacks"))
        prefs.enableCopyResourcePacks(parameters.value("copyResourcePacks").toBool());
    if (parameters.contains("copyShaderPacks"))
        prefs.enableCopyShaderPacks(parameters.value("copyShaderPacks").toBool());
    if (parameters.contains("copyScreenshots"))
        prefs.enableCopyScreenshots(parameters.value("copyScreenshots").toBool());

    QSet<QString> previousIds;
    for (int i = 0; i < APPLICATION->instances()->count(); ++i)
        previousIds.insert(APPLICATION->instances()->at(i)->id());
    auto copyTask = new InstanceCopyTask(instance, prefs);
    copyTask->setName(name);
    copyTask->setIcon(parameters.value("icon").toString(instance->iconKey()));
    copyTask->setGroup(parameters.value("group").toString(APPLICATION->instances()->getInstanceGroup(instance->id())));
    auto task = APPLICATION->instances()->wrapInstanceTask(copyTask);
    QString error;
    if (!waitForTask(task, interaction, &error))
        return failure(error);
    for (int i = 0; i < APPLICATION->instances()->count(); ++i) {
        const auto copied = APPLICATION->instances()->at(i);
        if (!previousIds.contains(copied->id()))
            return success(QJsonObject{ { "id", copied->id() }, { "name", copied->name() } });
    }
    return success(QJsonObject{ { "name", name } });
}

QJsonObject OperationService::updateInstance(const QJsonObject& parameters, UserInteraction& interaction)
{
    const auto reference = parameters.value("instance").toString();
    const auto instance = findInstance(reference);
    if (!instance)
        return failure(tr("Instance not found: %1").arg(reference), 2);
    if (instance->isRunning())
        return failure(tr("A running instance cannot be updated."), 2);
    const auto updateTasks = instance->createUpdateTask();
    if (updateTasks.isEmpty())
        return success(QJsonObject{ { "id", instance->id() }, { "updated", false } });
    auto task = makeShared<SequentialTask>(tr("Update instance %1").arg(instance->name()));
    for (const auto& updateTask : updateTasks)
        task->addTask(updateTask);
    QString error;
    if (!waitForTask(task.get(), interaction, &error))
        return failure(error);
    return success(QJsonObject{ { "id", instance->id() }, { "updated", true } });
}

QJsonObject OperationService::deleteInstance(const QJsonObject& parameters)
{
    if (!parameters.value("confirm").toBool())
        return failure(tr("Instance deletion requires confirm=true."), 2);
    const auto reference = parameters.value("instance").toString();
    const auto instance = findInstance(reference);
    if (!instance)
        return failure(tr("Instance not found: %1").arg(reference), 2);
    if (instance->isRunning())
        return failure(tr("A running instance cannot be deleted."), 2);
    const auto linked = APPLICATION->instances()->getLinkedInstancesById(instance->id());
    if (!linked.isEmpty() && !parameters.value("force").toBool())
        return failure(tr("The instance is linked by: %1. Use force=true to continue.").arg(linked.join(", ")), 2);
    const auto id = instance->id();
    const auto name = instance->name();
    if (parameters.value("permanent").toBool()) {
        const auto root = instance->instanceRoot();
        APPLICATION->instances()->deleteInstance(id);
        if (QFileInfo::exists(root))
            return failure(tr("The instance could not be completely deleted."));
        return success(QJsonObject{ { "id", id }, { "name", name }, { "permanent", true } });
    }
    if (!APPLICATION->instances()->trashInstance(id))
        return failure(tr("The instance could not be moved to the trash. Use permanent=true to delete it permanently."));
    return success(QJsonObject{ { "id", id }, { "name", name }, { "permanent", false } });
}

QJsonObject OperationService::undoDeleteInstance()
{
    if (!APPLICATION->instances()->trashedSomething())
        return failure(tr("There is no trashed instance to restore."), 2);
    if (!APPLICATION->instances()->undoTrashInstance())
        return failure(tr("The trashed instance or its shortcuts could not be completely restored."));
    return success(QJsonObject{ { "restored", true } });
}

QJsonObject OperationService::listResources(const QJsonObject& parameters)
{
    const auto reference = parameters.value("instance").toString();
    const auto instance = dynamic_cast<MinecraftInstance*>(findInstance(reference));
    if (!instance)
        return failure(tr("Minecraft instance not found: %1").arg(reference), 2);
    const auto kind = parameters.value("kind").toString();
    const auto model = findResourceModel(instance, kind);
    if (!model)
        return failure(tr("Unknown or unavailable resource kind: %1").arg(kind), 2);

    QJsonArray resources;
    const auto entries = model->dir().entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase);
    for (const auto& entry : entries) {
        if (entry.fileName() == ".index")
            continue;
        Resource resource(entry);
        resources.append(QJsonObject{ { "id", resource.internal_id() },
                                      { "name", resource.name() },
                                      { "fileName", resource.fileinfo().fileName() },
                                      { "enabled", resource.enabled() },
                                      { "type", resourceTypeName(resource.type()) },
                                      { "size", resource.sizeInfo() },
                                      { "path", resource.fileinfo().absoluteFilePath() } });
    }
    return success(resources);
}

QJsonObject OperationService::installResource(const QJsonObject& parameters, UserInteraction& interaction)
{
    const auto reference = parameters.value("instance").toString();
    const auto instance = dynamic_cast<MinecraftInstance*>(findInstance(reference));
    if (!instance)
        return failure(tr("Minecraft instance not found: %1").arg(reference), 2);
    if (instance->isRunning())
        return failure(tr("Resources cannot be installed while the instance is running."), 2);
    const auto kind = parameters.value("kind").toString();
    const auto model = findResourceModel(instance, kind);
    if (!model)
        return failure(tr("Unknown or unavailable resource kind: %1").arg(kind), 2);
    const auto source = parameters.value("source").toString();
    if (source.isEmpty())
        return failure(tr("A local path or direct URL is required."), 2);

    QString localPath;
    const QUrl url(source, QUrl::TolerantMode);
    if (url.isLocalFile()) {
        localPath = url.toLocalFile();
    } else if (QFileInfo::exists(source)) {
        localPath = QFileInfo(source).absoluteFilePath();
    } else if (url.isValid() && (url.scheme() == "http" || url.scheme() == "https")) {
        auto fileName = QFileInfo(url.path()).fileName();
        if (fileName.isEmpty())
            return failure(tr("The resource URL does not contain a file name."), 2);
        localPath = FS::getUniqueResourceName(QDir(m_downloads.path()).filePath(fileName));
        auto job = makeShared<NetJob>("CLI::DownloadResource", APPLICATION->network());
        job->addNetAction(Net::Download::makeFile(url, localPath));
        QString error;
        if (!waitForTask(job.get(), interaction, &error))
            return failure(error);
    } else {
        return failure(tr("Resource source is neither a local file nor a direct URL: %1").arg(source), 2);
    }
    if (!model->installResource(localPath))
        return failure(tr("The resource could not be installed."));
    return success(QJsonObject{ { "instance", instance->id() }, { "kind", kind }, { "fileName", QFileInfo(localPath).fileName() } });
}

QJsonObject OperationService::setResourceEnabled(const QJsonObject& parameters, bool enabled)
{
    const auto reference = parameters.value("instance").toString();
    const auto instance = dynamic_cast<MinecraftInstance*>(findInstance(reference));
    if (!instance)
        return failure(tr("Minecraft instance not found: %1").arg(reference), 2);
    if (instance->isRunning())
        return failure(tr("Resources cannot be changed while the instance is running."), 2);
    const auto kind = parameters.value("kind").toString();
    const auto model = findResourceModel(instance, kind);
    if (!model)
        return failure(tr("Unknown or unavailable resource kind: %1").arg(kind), 2);
    const auto resource = findResource(model, parameters.value("resource").toString());
    if (!resource)
        return failure(tr("Resource not found: %1").arg(parameters.value("resource").toString()), 2);
    if (resource->enabled() == enabled)
        return success(QJsonObject{ { "fileName", resource->fileinfo().fileName() }, { "enabled", enabled }, { "changed", false } });
    if (!resource->enable(enabled ? EnableAction::ENABLE : EnableAction::DISABLE))
        return failure(tr("The resource could not be %1.").arg(enabled ? tr("enabled") : tr("disabled")));
    return success(QJsonObject{ { "fileName", resource->fileinfo().fileName() }, { "enabled", enabled }, { "changed", true } });
}

QJsonObject OperationService::removeResource(const QJsonObject& parameters)
{
    if (!parameters.value("confirm").toBool())
        return failure(tr("Resource removal requires confirm=true."), 2);
    const auto reference = parameters.value("instance").toString();
    const auto instance = dynamic_cast<MinecraftInstance*>(findInstance(reference));
    if (!instance)
        return failure(tr("Minecraft instance not found: %1").arg(reference), 2);
    if (instance->isRunning())
        return failure(tr("Resources cannot be removed while the instance is running."), 2);
    const auto kind = parameters.value("kind").toString();
    const auto model = findResourceModel(instance, kind);
    if (!model)
        return failure(tr("Unknown or unavailable resource kind: %1").arg(kind), 2);
    const auto resource = findResource(model, parameters.value("resource").toString());
    if (!resource)
        return failure(tr("Resource not found: %1").arg(parameters.value("resource").toString()), 2);
    const auto fileName = resource->fileinfo().fileName();
    if (!resource->destroy(model->indexDir(), parameters.value("preserveMetadata").toBool(), true))
        return failure(tr("The resource could not be removed."));
    return success(QJsonObject{ { "fileName", fileName }, { "removed", true } });
}

QJsonObject OperationService::listJava(UserInteraction& interaction)
{
    auto list = APPLICATION->javalist();
    if (!list->isLoaded()) {
        auto task = list->getLoadTask();
        QString error;
        if (task && !waitForTask(task.get(), interaction, &error))
            return failure(error);
    }
    QJsonArray installations;
    for (int i = 0; i < list->count(); ++i) {
        const auto java = std::dynamic_pointer_cast<JavaInstall>(list->at(i));
        if (!java)
            continue;
        installations.append(QJsonObject{
            { "version", java->id.toString() }, { "architecture", java->arch }, { "path", java->path }, { "is64Bit", java->is_64bit } });
    }
    return success(installations);
}

SettingsObject* OperationService::resolveSettings(const QJsonObject& parameters, QString* error, BaseInstance** instance) const
{
    if (instance)
        *instance = nullptr;
    const auto scope = parameters.value("scope").toString("launcher").toLower();
    if (scope == "launcher" || scope == "global")
        return APPLICATION->settings();
    if (scope != "instance") {
        *error = tr("Unknown settings scope: %1").arg(scope);
        return nullptr;
    }

    const auto reference = parameters.value("instance").toString();
    if (reference.isEmpty()) {
        *error = tr("Instance settings require an instance ID or name.");
        return nullptr;
    }
    auto resolved = findInstance(reference);
    if (!resolved) {
        *error = tr("Instance not found: %1").arg(reference);
        return nullptr;
    }
    if (instance)
        *instance = resolved;
    return resolved->settings();
}

QJsonObject OperationService::listSettings(const QJsonObject& parameters)
{
    QString error;
    auto settings = resolveSettings(parameters, &error);
    if (!settings)
        return failure(error, 2);

    const auto filter = parameters.value("filter").toString();
    const bool reveal = parameters.value("reveal").toBool(false);
    QJsonArray values;
    for (const auto& key : settings->settingIds()) {
        if (!filter.isEmpty() && !key.contains(filter, Qt::CaseInsensitive))
            continue;
        values.append(describeSetting(settings, settings->getSetting(key), reveal));
    }
    return success(values);
}

QJsonObject OperationService::getSetting(const QJsonObject& parameters)
{
    QString error;
    auto settings = resolveSettings(parameters, &error);
    if (!settings)
        return failure(error, 2);
    const auto key = parameters.value("key").toString();
    const auto setting = settings->getSetting(key);
    if (!setting)
        return failure(tr("Unknown setting: %1").arg(key), 2);
    return success(describeSetting(settings, setting, parameters.value("reveal").toBool(false)));
}

QJsonObject OperationService::setSetting(const QJsonObject& parameters, UserInteraction& interaction)
{
    QString error;
    BaseInstance* instance = nullptr;
    auto settings = resolveSettings(parameters, &error, &instance);
    if (!settings)
        return failure(error, 2);
    const auto key = parameters.value("key").toString();
    const auto setting = settings->getSetting(key);
    if (!setting)
        return failure(tr("Unknown setting: %1").arg(key), 2);
    if (isSensitiveSetting(key) && parameters.value("valueFromCommandLine").toBool())
        return failure(tr("Sensitive settings must be entered interactively or with --password-stdin."), 2);

    QJsonValue input = parameters.value("value");
    if (input.isUndefined()) {
        const auto value = interaction.input(tr("Value for %1").arg(key), isSensitiveSetting(key));
        if (!value)
            return failure(tr("A value is required."), 2);
        input = *value;
    }
    const auto current = settings->get(key);
    const auto reference = current.isValid() ? current : setting->defValue();
    QVariant converted;
    if (!convertSettingValue(input, reference, &converted, &error))
        return failure(error, 2);
    if (!settings->set(key, converted))
        return failure(tr("Could not set %1.").arg(key));
    if (instance)
        instance->saveNow();
    return success(describeSetting(settings, setting, parameters.value("reveal").toBool(false)));
}

QJsonObject OperationService::resetSetting(const QJsonObject& parameters)
{
    QString error;
    BaseInstance* instance = nullptr;
    auto settings = resolveSettings(parameters, &error, &instance);
    if (!settings)
        return failure(error, 2);
    const auto key = parameters.value("key").toString();
    const auto setting = settings->getSetting(key);
    if (!setting)
        return failure(tr("Unknown setting: %1").arg(key), 2);
    settings->reset(key);
    if (instance)
        instance->saveNow();
    return success(describeSetting(settings, setting, parameters.value("reveal").toBool(false)));
}

bool OperationService::waitForTask(Task* task, UserInteraction& interaction, QString* error)
{
    if (!task) {
        if (error)
            *error = tr("The operation did not create a task.");
        return false;
    }
    QEventLoop loop;
    const auto previousTask = m_currentTask;
    m_currentTask = task;
    connect(task, &Task::status, this, [&interaction](const QString& status) { interaction.status(status); });
    connect(task, &Task::finished, &loop, &QEventLoop::quit);
    if (!task->isFinished()) {
        if (!task->isRunning())
            task->start();
        if (!task->isFinished())
            loop.exec();
    }
    if (!task->wasSuccessful() && error)
        *error = task->failReason().isEmpty() ? tr("The operation was aborted.") : task->failReason();
    m_currentTask = previousTask;
    return task->wasSuccessful();
}

QJsonObject OperationService::setupMicrosoftProfile(const MinecraftAccountPtr& account, const QString& name, UserInteraction& interaction)
{
    static const QRegularExpression permittedNames("^[a-zA-Z0-9_]{3,16}$");
    if (!permittedNames.match(name).hasMatch())
        return failure(tr("Minecraft profile names must contain 3-16 letters, digits, or underscores."), 2);

    const auto payload = QJsonDocument(QJsonObject{ { "profileName", name } }).toJson(QJsonDocument::Compact);
    auto [request, response] = Net::Upload::makeByteArray(QUrl("https://api.minecraftservices.com/minecraft/profile"), payload);
    request->addHeaderProxy(std::make_unique<Net::RawHeaderProxy>(QList<Net::HeaderPair>{
        { "Content-Type", "application/json" },
        { "Accept", "application/json" },
        { "Authorization", QString("Bearer %1").arg(account->accessToken()).toUtf8() },
    }));
    request->setNetwork(APPLICATION->network());
    QString error;
    if (!waitForTask(request.get(), interaction, &error)) {
        const auto serverMessage = QJsonDocument::fromJson(*response).object().value("errorMessage").toString();
        return failure(serverMessage.isEmpty() ? error : serverMessage);
    }

    auto refresh = account->refresh();
    if (!waitForTask(refresh.get(), interaction, &error))
        return failure(error);
    return success();
}

QJsonObject OperationService::loginAccount(const QJsonObject& parameters, UserInteraction& interaction)
{
    const auto type = parameters.value("type").toString();
    auto username = parameters.value("username").toString();
    if (username.isEmpty() && type != "microsoft") {
        const auto value = interaction.input(tr("Username"), false);
        if (!value || value->trimmed().isEmpty())
            return failure(tr("A username is required."), 2);
        username = value->trimmed();
    }

    MinecraftAccountPtr account;
    if (type == "offline") {
        account = MinecraftAccount::createOffline(username);
    } else if (type == "microsoft") {
        account = MinecraftAccount::createBlankMSA();
    } else if (type == "yggdrasil" || type == "unified-pass") {
        const auto password = interaction.input(tr("Password"), true);
        if (!password)
            return failure(tr("A password is required; use an interactive terminal or --password-stdin."), 2);
        if (type == "yggdrasil") {
            const auto authUrl = parameters.value("authUrl").toString();
            const auto sessionUrl = parameters.value("sessionUrl").toString();
            if (authUrl.isEmpty() || sessionUrl.isEmpty())
                return failure(tr("Yggdrasil login requires authUrl and sessionUrl."), 2);
            account =
                MinecraftAccount::createYggdrasil(username, *password, authUrl, sessionUrl, parameters.value("sourceName").toString());
        } else {
            const auto serverId = parameters.value("serverId").toString();
            if (serverId.isEmpty())
                return failure(tr("UnifiedPass login requires serverId."), 2);
            account = MinecraftAccount::createUnifiedPass(username, *password, serverId);
        }
    } else {
        return failure(tr("Unsupported account type: %1").arg(type), 2);
    }

    if (type != "offline") {
        const auto requestedProfile = parameters.value("profileId").toString();
        const auto selector = [&interaction, requestedProfile](const QJsonArray& profiles) -> std::optional<int> {
            if (!requestedProfile.isEmpty()) {
                for (int i = 0; i < profiles.size(); ++i) {
                    if (profiles.at(i).toObject().value("id").toString() == requestedProfile)
                        return i;
                }
                return std::nullopt;
            }
            return interaction.select(QObject::tr("Select a game profile"), profiles);
        };
        auto task = account->login(type == "microsoft", selector);
        connect(task.get(), &AuthFlow::authorizeWithBrowserWithExtra, this,
                [&interaction](const QString& url, const QString& code, int expiresIn) { interaction.deviceCode(url, code, expiresIn); });
        connect(task.get(), &AuthFlow::authorizeWithBrowser, this,
                [&interaction](const QUrl& url) { interaction.deviceCode(url.toString(), QString(), 0); });
        QString error;
        if (!waitForTask(task.get(), interaction, &error))
            return failure(error);
        if (type == "microsoft" && !account->hasProfile()) {
            auto profileName = parameters.value("minecraftProfileName").toString();
            if (profileName.isEmpty()) {
                const auto value = interaction.input(tr("New Minecraft Java profile name"), false);
                if (value)
                    profileName = value->trimmed();
            }
            if (profileName.isEmpty())
                return failure(tr("This Microsoft account has no Java profile. Provide minecraftProfileName to create one."), 2);
            const auto setupResult = setupMicrosoftProfile(account, profileName, interaction);
            if (!setupResult.value("ok").toBool())
                return setupResult;
        }
    }

    auto accounts = APPLICATION->accounts();
    accounts->addAccount(account);
    if (!accounts->defaultAccount())
        accounts->setDefaultAccount(account);
    accounts->saveList();
    return success(QJsonObject{
        { "id", account->profileId() }, { "profileName", account->profileName() }, { "type", accountTypeName(account->accountType()) } });
}

QJsonObject OperationService::fetchJson(const QUrl& url, const QString& taskName, QJsonDocument* document, UserInteraction& interaction)
{
    auto job = makeShared<NetJob>(taskName, APPLICATION->network());
    auto [action, response] = Net::ApiDownload::makeByteArray(url);
    job->addNetAction(action);
    QString error;
    if (!waitForTask(job.get(), interaction, &error))
        return failure(error);
    QJsonParseError parseError;
    *document = QJsonDocument::fromJson(*response, &parseError);
    if (parseError.error != QJsonParseError::NoError)
        return failure(tr("The platform returned invalid JSON: %1").arg(parseError.errorString()));
    return success();
}

QJsonObject OperationService::resolvePackSource(const QString& source, UserInteraction& interaction)
{
    const QFileInfo localFile(source);
    if (localFile.exists() && localFile.isFile())
        return success(QUrl::fromLocalFile(localFile.absoluteFilePath()).toString());

    const QUrl sourceUrl(source, QUrl::TolerantMode);
    if (!sourceUrl.isValid() || sourceUrl.scheme().isEmpty())
        return failure(tr("Pack source is neither a local file nor a valid URL: %1").arg(source), 2);

    const auto host = sourceUrl.host().toLower();
    const auto segments = sourceUrl.path().split('/', Qt::SkipEmptyParts);
    if (host.endsWith("modrinth.com") && segments.size() >= 2 && segments.at(0) == "modpack") {
        const auto project = segments.at(1);
        QUrl apiUrl(ModApiMirror::modrinthBaseUrl() + "/project/" + project + "/version");
        if (segments.size() >= 4 && segments.at(2) == "version")
            apiUrl = QUrl(ModApiMirror::modrinthBaseUrl() + "/version/" + segments.at(3));
        QJsonDocument document;
        auto result = fetchJson(apiUrl, "CLI::ResolveModrinthPack", &document, interaction);
        if (!result.value("ok").toBool())
            return result;
        const auto version = document.isArray() ? document.array().at(0).toObject() : document.object();
        const auto files = version.value("files").toArray();
        if (files.isEmpty())
            return failure(tr("The Modrinth version has no downloadable files."));
        QJsonObject selected = files.at(0).toObject();
        for (const auto& file : files) {
            if (file.toObject().value("primary").toBool()) {
                selected = file.toObject();
                break;
            }
        }
        return success(selected.value("url").toString());
    }

    if (host.endsWith("curseforge.com") && segments.size() >= 5 && segments.at(0) == "minecraft" && segments.at(1) == "modpacks" &&
        segments.at(3) == "files") {
        const auto slug = segments.at(2);
        const auto fileId = segments.at(4);
        QUrl searchUrl(ModApiMirror::curseForgeBaseUrl() + "/mods/search");
        QUrlQuery query;
        query.addQueryItem("gameId", "432");
        query.addQueryItem("classId", "4471");
        query.addQueryItem("slug", slug);
        searchUrl.setQuery(query);
        QJsonDocument projectDocument;
        auto result = fetchJson(searchUrl, "CLI::ResolveCurseForgeProject", &projectDocument, interaction);
        if (!result.value("ok").toBool())
            return result;
        const auto projects = projectDocument.object().value("data").toArray();
        if (projects.isEmpty())
            return failure(tr("CurseForge project not found: %1").arg(slug));
        const auto addonId = projects.at(0).toObject().value("id").toVariant().toString();
        QJsonDocument fileDocument;
        result = fetchJson(QUrl(ModApiMirror::curseForgeBaseUrl() + QString("/mods/%1/files/%2").arg(addonId, fileId)),
                           "CLI::ResolveCurseForgeFile", &fileDocument, interaction);
        if (!result.value("ok").toBool())
            return result;
        const auto file = fileDocument.object().value("data").toObject();
        const auto downloadUrl = file.value("downloadUrl").toString();
        if (!downloadUrl.isEmpty())
            return success(downloadUrl);

        const auto hashes = file.value("hashes").toArray();
        if (hashes.isEmpty())
            return failure(tr("The restricted CurseForge file has no hash metadata."));
        const auto hash = hashes.at(0).toObject();
        const auto algorithm = hashName(hash.value("algo").toInt());
        CurseForgeDownloadPageService service(CurseForgeDownloadPageService::Provider::External);
        QString verifiedPath;
        QString serviceError;
        QEventLoop loop;
        connect(&service, &CurseForgeDownloadPageService::failed, &loop, [&](const QString& error) {
            serviceError = error;
            loop.quit();
        });
        connect(&service, &CurseForgeDownloadPageService::fileReady, &loop, [&](const QString& fileName, int, int) {
            const auto path = QDir(service.downloadDirectory()).filePath(fileName);
            QFile downloaded(path);
            if (!downloaded.open(QIODevice::ReadOnly)) {
                serviceError = downloaded.errorString();
                loop.quit();
                return;
            }
            const auto digest = QCryptographicHash::hash(downloaded.readAll(), hashAlgorithm(algorithm)).toHex();
            verifiedPath = service.acceptDownloadedFile(path, QString::fromLatin1(digest));
            if (verifiedPath.isEmpty()) {
                serviceError = tr("The external CurseForge tool returned an invalid file.");
                loop.quit();
            }
        });
        connect(&service, &CurseForgeDownloadPageService::completed, &loop, &QEventLoop::quit);
        const auto pageUrl = QString("https://www.curseforge.com/minecraft/modpacks/%1/download/%2").arg(slug, fileId);
        const CurseForgeDownloadPage page{ pageUrl, file.value("fileName").toString(), algorithm, hash.value("value").toString() };
        interaction.status(tr("Waiting for the configured CurseForge external tool..."));
        if (!service.open({ page }, "cli"))
            return failure(service.errorString());
        loop.exec();
        if (!serviceError.isEmpty())
            return failure(serviceError);
        if (verifiedPath.isEmpty())
            return failure(tr("The external CurseForge tool did not return the requested file."));
        const auto destination = QDir(m_downloads.path()).filePath(file.value("fileName").toString());
        if (!QFile::copy(verifiedPath, destination))
            return failure(tr("Could not preserve the downloaded CurseForge pack."));
        return success(QUrl::fromLocalFile(destination).toString());
    }

    return success(sourceUrl.toString());
}

QJsonObject OperationService::importInstance(const QJsonObject& parameters, UserInteraction& interaction)
{
    const auto source = parameters.value("source").toString();
    if (source.isEmpty())
        return failure(tr("A pack source is required."), 2);
    const auto resolved = resolvePackSource(source, interaction);
    if (!resolved.value("ok").toBool())
        return resolved;
    const auto resolvedUrl = QUrl(resolved.value("data").toString());
    auto importTask = new InstanceImportTask(resolvedUrl, nullptr);
    const auto name = parameters.value("name").toString();
    if (!name.isEmpty())
        importTask->setName(name);
    auto task = APPLICATION->instances()->wrapInstanceTask(importTask);
    QString error;
    if (!waitForTask(task, interaction, &error))
        return failure(error);
    APPLICATION->instances()->saveNow();
    return success(QJsonObject{ { "name", name }, { "source", resolvedUrl.toString() } });
}

QJsonObject OperationService::launchInstance(const QJsonObject& parameters, UserInteraction& interaction)
{
    const auto instanceId = parameters.value("instance").toString();
    auto instance = findInstance(instanceId);
    if (!instance)
        return failure(tr("Instance not found: %1").arg(instanceId), 2);

    MinecraftAccountPtr account;
    const auto profile = parameters.value("profile").toString();
    if (!profile.isEmpty())
        account = APPLICATION->accounts()->getAccountByProfileName(profile);
    else
        account = APPLICATION->accounts()->defaultAccount();

    const auto offlineName = parameters.value("offlineName").toString();
    auto mode = LaunchMode::Normal;
    if (!offlineName.isEmpty()) {
        mode = LaunchMode::Offline;
        account = MinecraftAccount::createOffline(offlineName);
    }
    if (!account)
        return failure(tr("No account is available. Log in or pass an offline player name."), 2);

    MinecraftTarget::Ptr target;
    if (!parameters.value("server").toString().isEmpty())
        target.reset(new MinecraftTarget(MinecraftTarget::parse(parameters.value("server").toString(), false)));
    else if (!parameters.value("world").toString().isEmpty())
        target.reset(new MinecraftTarget(MinecraftTarget::parse(parameters.value("world").toString(), true)));

    auto controller = new LaunchController();
    controller->setParent(this);
    controller->setHeadless(true);
    controller->setInstance(instance);
    controller->setLaunchMode(mode);
    controller->setOfflineName(offlineName);
    controller->setAccountToUse(account);
    controller->setTargetToJoin(target);
    controller->setReauthenticateHandler([this, &interaction, parameters](const MinecraftAccountPtr& expired,
                                                                          const QString& reason) -> MinecraftAccountPtr {
        interaction.status(reason);
        const auto requestedProfile = parameters.value("profileId").toString();
        const auto selector = [&interaction, requestedProfile](const QJsonArray& profiles) -> std::optional<int> {
            if (!requestedProfile.isEmpty()) {
                for (int i = 0; i < profiles.size(); ++i) {
                    if (profiles.at(i).toObject().value("id").toString() == requestedProfile)
                        return i;
                }
                return std::nullopt;
            }
            return interaction.select(QObject::tr("Select a game profile"), profiles);
        };

        MinecraftAccountPtr replacement = expired;
        if (expired->accountType() != AccountType::MSA) {
            auto username = parameters.value("username").toString();
            if (username.isEmpty()) {
                const auto input = interaction.input(tr("Username"), false);
                if (!input)
                    return nullptr;
                username = input->trimmed();
            }
            const auto password = interaction.input(tr("Password"), true);
            if (!password)
                return nullptr;
            const auto data = expired->accountData();
            if (expired->accountType() == AccountType::Yggdrasil) {
                const auto& config = data->yggdrasilConfig;
                replacement =
                    MinecraftAccount::createYggdrasil(username, *password, config.authServerUrl, config.sessionServerUrl, config.sourceName,
                                                      config.refreshEndpoint, config.validateEndpoint, config.authenticateEndpoint,
                                                      config.profileEndpoint, config.oauthTokenEndpoint, config.tokenType);
            } else {
                const auto& config = data->unifiedPassConfig;
                replacement = MinecraftAccount::createUnifiedPass(username, *password, config.serverId, config.baseUrl);
            }
        }

        auto auth = replacement->login(replacement->accountType() == AccountType::MSA, selector);
        connect(auth.get(), &AuthFlow::authorizeWithBrowserWithExtra, this,
                [&interaction](const QString& url, const QString& code, int expiresIn) { interaction.deviceCode(url, code, expiresIn); });
        connect(auth.get(), &AuthFlow::authorizeWithBrowser, this,
                [&interaction](const QUrl& url) { interaction.deviceCode(url.toString(), QString(), 0); });
        QString error;
        if (!waitForTask(auth.get(), interaction, &error)) {
            interaction.status(error);
            return nullptr;
        }

        auto accounts = APPLICATION->accounts();
        if (replacement != expired) {
            const bool wasDefault = accounts->defaultAccount() == expired;
            const int index = accounts->findAccountByProfileId(expired->profileId());
            if (index >= 0)
                accounts->removeAccount(accounts->index(index));
            accounts->addAccount(replacement);
            if (wasDefault)
                accounts->setDefaultAccount(replacement);
        }
        accounts->saveList();
        return replacement;
    });

    bool started = false;
    qint64 pid = -1;
    QEventLoop loop;
    connect(controller, &LaunchController::gameStarted, &loop, [&](qint64 processId) {
        started = true;
        pid = processId;
        if (!parameters.value("wait").toBool())
            loop.quit();
    });
    connect(controller, &Task::status, this, [&interaction](const QString& status) { interaction.status(status); });
    connect(controller, &LaunchController::logLine, this, [&interaction](const QString& line) { interaction.status(line); });
    connect(controller, &Task::finished, &loop, &QEventLoop::quit);
    m_currentTask = controller;
    controller->start();
    if (!controller->isFinished() && (!started || parameters.value("wait").toBool()))
        loop.exec();

    const bool wait = parameters.value("wait").toBool();
    if (!wait && started) {
        controller->deleteLater();
        m_currentTask.clear();
        return success(QJsonObject{ { "instance", instance->id() }, { "pid", pid }, { "detached", true } });
    }
    const bool ok = controller->wasSuccessful();
    const auto error = controller->failReason();
    controller->deleteLater();
    m_currentTask.clear();
    if (!ok)
        return failure(error.isEmpty() ? tr("Launch failed.") : error);
    return success(QJsonObject{ { "instance", instance->id() }, { "pid", pid }, { "detached", false } });
}
