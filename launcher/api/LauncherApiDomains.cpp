// SPDX-License-Identifier: GPL-3.0-only

#include "LauncherApiDomains.h"

#include "Application.h"
#include "api/LauncherApi.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/WorldList.h"
#include "minecraft/auth/AccountList.h"
#include "minecraft/auth/MinecraftAccount.h"
#include "minecraft/skins/CapeChange.h"
#include "minecraft/skins/SkinDelete.h"
#include "minecraft/skins/SkinList.h"
#include "minecraft/skins/SkinModel.h"
#include "minecraft/skins/SkinUpload.h"
#include "InstanceList.h"
#include "cli/OperationService.h"
#include "net/NetJob.h"
#include "tasks/Task.h"

#include <QDir>
#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>

namespace {

QJsonObject stringProperty(const QString& description)
{
    return { { "type", "string" }, { "description", description } };
}

QJsonObject boolProperty(const QString& description)
{
    return { { "type", "boolean" }, { "description", description } };
}

QJsonObject objectSchema(QJsonObject properties, QJsonArray required = {})
{
    QJsonObject schema{ { "type", "object" }, { "properties", properties }, { "additionalProperties", false } };
    if (!required.isEmpty())
        schema.insert("required", required);
    return schema;
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

QJsonObject worldJson(const World& world)
{
    return { { "folder", world.folderName() },
             { "name", world.name() },
             { "path", world.container().absoluteFilePath() },
             { "icon", world.iconFile() },
             { "size", static_cast<qint64>(world.bytes()) },
             { "lastPlayed", world.lastPlayed().toString(Qt::ISODate) },
             { "seed", static_cast<qint64>(world.seed()) },
             { "gameType", world.gameType().toLogString() },
             { "valid", world.isValid() },
             { "directory", world.isOnFS() } };
}

QString skinDirectory()
{
    return APPLICATION->settings()->get("SkinsDir").toString();
}

SkinModel* findSkin(SkinList& skins, const QString& reference)
{
    const auto normalized = QDir::fromNativeSeparators(reference.trimmed());
    for (int row = 0; row < skins.rowCount(); ++row) {
        const auto key = skins.index(row, 0).data(Qt::UserRole).toString();
        auto skin = skins.skin(key);
        if (!skin)
            continue;
        if (key.compare(reference, Qt::CaseInsensitive) == 0 ||
            QDir::fromNativeSeparators(skin->getPath()).compare(normalized, Qt::CaseInsensitive) == 0 ||
            QFileInfo(skin->getPath()).fileName().compare(reference, Qt::CaseInsensitive) == 0)
            return skin;
    }
    return nullptr;
}

QJsonObject skinJson(const SkinModel& skin, MinecraftAccount& account)
{
    const auto& profile = account.accountData()->minecraftProfile;
    return { { "name", skin.name() },
             { "path", skin.getPath() },
             { "model", skin.getModelString() },
             { "cape", skin.getCapeId() },
             { "url", skin.getURL() },
             { "selected", !profile.skin.url.isEmpty() && profile.skin.url == skin.getURL() } };
}

bool waitForAccountTask(Task* task, UserInteraction& interaction, QString* error, LauncherApi& api)
{
    if (!task) {
        if (error)
            *error = QObject::tr("The account operation did not create a task.");
        return false;
    }
    QEventLoop loop;
    api.trackTask(task);
    QObject::connect(task, &Task::status, &loop, [&interaction](const QString& status) { interaction.status(status); });
    QObject::connect(task, &Task::finished, &loop, &QEventLoop::quit);
    if (!task->isFinished()) {
        if (!task->isRunning())
            task->start();
        if (!task->isFinished())
            loop.exec();
    }
    api.clearTrackedTask(task);
    if (!task->wasSuccessful() && error)
        *error = task->failReason().isEmpty() ? QObject::tr("The account operation failed.") : task->failReason();
    return task->wasSuccessful();
}

QJsonObject listAccountSkins(const QJsonObject& parameters)
{
    const auto account = findAccount(parameters.value("account").toString());
    if (!account)
        return OperationService::failure(QObject::tr("Account not found: %1").arg(parameters.value("account").toString()), 2);
    SkinList skins(nullptr, skinDirectory(), account);
    QJsonArray result;
    for (int row = 0; row < skins.rowCount(); ++row) {
        const auto key = skins.index(row, 0).data(Qt::UserRole).toString();
        if (const auto* skin = skins.skin(key))
            result.append(skinJson(*skin, *account));
    }
    return OperationService::success(result);
}

QJsonObject installAccountSkin(const QJsonObject& parameters)
{
    const auto account = findAccount(parameters.value("account").toString());
    if (!account)
        return OperationService::failure(QObject::tr("Account not found: %1").arg(parameters.value("account").toString()), 2);
    const auto path = parameters.value("path").toString().trimmed();
    if (path.isEmpty())
        return OperationService::failure(QObject::tr("A skin PNG path is required."), 2);
    SkinList skins(nullptr, skinDirectory(), account);
    const auto error = skins.installSkin(path, parameters.value("name").toString().trimmed());
    if (!error.isEmpty())
        return OperationService::failure(error, 2);
    return OperationService::success(QJsonObject{ { "path", path }, { "installed", true } });
}

QJsonObject deleteAccountSkin(const QJsonObject& parameters)
{
    if (!parameters.value("confirm").toBool())
        return OperationService::failure(QObject::tr("Skin deletion requires confirm=true."), 2);
    const auto account = findAccount(parameters.value("account").toString());
    if (!account)
        return OperationService::failure(QObject::tr("Account not found: %1").arg(parameters.value("account").toString()), 2);
    SkinList skins(nullptr, skinDirectory(), account);
    auto* skin = findSkin(skins, parameters.value("skin").toString());
    if (!skin)
        return OperationService::failure(QObject::tr("Skin not found: %1").arg(parameters.value("skin").toString()), 2);
    const auto name = skin->name();
    if (!skins.deleteSkin(name, parameters.value("trash").toBool(true)))
        return OperationService::failure(QObject::tr("The skin could not be deleted."));
    return OperationService::success(QJsonObject{ { "skin", name }, { "deleted", true } });
}

QJsonObject uploadAccountSkin(LauncherApi& api, const QJsonObject& parameters, UserInteraction& interaction)
{
    const auto account = findAccount(parameters.value("account").toString());
    if (!account)
        return OperationService::failure(QObject::tr("Account not found: %1").arg(parameters.value("account").toString()), 2);
    if (account->accountType() != AccountType::MSA)
        return OperationService::failure(QObject::tr("Skin upload requires a Microsoft account."), 2);
    if (account->accessToken().isEmpty())
        return OperationService::failure(QObject::tr("The account has no access token for skin changes."), 3);
    SkinList skins(nullptr, skinDirectory(), account);
    auto* skin = findSkin(skins, parameters.value("skin").toString());
    if (!skin)
        return OperationService::failure(QObject::tr("Skin not found: %1").arg(parameters.value("skin").toString()), 2);
    if (!QFileInfo::exists(skin->getPath()))
        return OperationService::failure(QObject::tr("Skin file not found: %1").arg(skin->getPath()), 2);

    const auto variant = parameters.value("variant").toString(skin->getModelString()).toLower();
    if (variant != "classic" && variant != "slim")
        return OperationService::failure(QObject::tr("Skin variant must be classic or slim."), 2);
    const auto cape = parameters.contains("cape") ? parameters.value("cape").toString() : skin->getCapeId();
    const auto skinPath = skin->getPath();
    const auto skinName = skin->name();

    auto job = makeShared<NetJob>(QObject::tr("Change skin"), APPLICATION->network(), 1);
    job->addNetAction(SkinUpload::make(account->accessToken(), skin->getPath(), variant));
    if (cape != account->accountData()->minecraftProfile.currentCape)
        job->addNetAction(CapeChange::make(account->accessToken(), cape));
    job->addTask(account->refresh().staticCast<Task>());
    QString error;
    if (!waitForAccountTask(job.get(), interaction, &error, api))
        return OperationService::failure(error, 3);
    // SkinList may rebuild its models from filesystem events during the wait.
    const auto& profile = account->accountData()->minecraftProfile;
    if (auto updated = findSkin(skins, skinPath)) {
        updated->setURL(profile.skin.url);
        updated->setCapeId(profile.currentCape);
        skins.save();
    }
    return OperationService::success(QJsonObject{ { "skin", skinName }, { "variant", variant }, { "cape", profile.currentCape },
                                                  { "url", profile.skin.url }, { "uploaded", true } });
}

QJsonObject resetAccountSkin(LauncherApi& api, const QJsonObject& parameters, UserInteraction& interaction)
{
    if (!parameters.value("confirm").toBool())
        return OperationService::failure(QObject::tr("Skin reset requires confirm=true."), 2);
    const auto account = findAccount(parameters.value("account").toString());
    if (!account)
        return OperationService::failure(QObject::tr("Account not found: %1").arg(parameters.value("account").toString()), 2);
    if (account->accountType() != AccountType::MSA)
        return OperationService::failure(QObject::tr("Skin reset requires a Microsoft account."), 2);
    if (account->accessToken().isEmpty())
        return OperationService::failure(QObject::tr("The account has no access token for skin changes."), 3);
    auto job = makeShared<NetJob>(QObject::tr("Reset skin"), APPLICATION->network(), 1);
    job->addNetAction(SkinDelete::make(account->accessToken()));
    job->addTask(account->refresh().staticCast<Task>());
    QString error;
    if (!waitForAccountTask(job.get(), interaction, &error, api))
        return OperationService::failure(error, 3);
    return OperationService::success(QJsonObject{ { "account", account->profileId() }, { "reset", true } });
}

QJsonObject selectAccountCape(LauncherApi& api, const QJsonObject& parameters, UserInteraction& interaction)
{
    const auto account = findAccount(parameters.value("account").toString());
    if (!account)
        return OperationService::failure(QObject::tr("Account not found: %1").arg(parameters.value("account").toString()), 2);
    if (account->accountType() != AccountType::MSA)
        return OperationService::failure(QObject::tr("Cape selection requires a Microsoft account."), 2);
    if (account->accessToken().isEmpty())
        return OperationService::failure(QObject::tr("The account has no access token for cape changes."), 3);
    const auto cape = parameters.value("cape").toString();
    bool knownCape = cape.isEmpty();
    for (const auto& item : account->accountData()->minecraftProfile.capes) {
        if (item.id == cape) {
            knownCape = true;
            break;
        }
    }
    if (!knownCape)
        return OperationService::failure(QObject::tr("Cape not found: %1").arg(cape), 2);
    auto job = makeShared<NetJob>(QObject::tr("Change cape"), APPLICATION->network(), 1);
    job->addNetAction(CapeChange::make(account->accessToken(), cape));
    job->addTask(account->refresh().staticCast<Task>());
    QString error;
    if (!waitForAccountTask(job.get(), interaction, &error, api))
        return OperationService::failure(error, 3);
    return OperationService::success(QJsonObject{ { "cape", account->accountData()->minecraftProfile.currentCape }, { "changed", true } });
}

World* findWorld(WorldList* worlds, const QString& reference)
{
    if (!worlds)
        return nullptr;
    for (size_t i = 0; i < worlds->size(); ++i) {
        auto& world = (*worlds)[i];
        if (world.folderName().compare(reference, Qt::CaseInsensitive) == 0 ||
            world.name().compare(reference, Qt::CaseInsensitive) == 0 ||
            world.container().fileName().compare(reference, Qt::CaseInsensitive) == 0)
            return &world;
    }
    return nullptr;
}

QStringList logRoots(BaseInstance* instance)
{
    QStringList roots = instance->getLogFileSearchPaths();
    roots.removeDuplicates();
    return roots;
}

QFileInfo safeFileInRoots(const QString& reference, const QStringList& roots)
{
    const auto requested = QFileInfo(reference);
    for (const auto& root : roots) {
        const auto rootPath = QFileInfo(root).canonicalFilePath();
        if (rootPath.isEmpty())
            continue;
        auto candidate = requested.isAbsolute() ? requested : QFileInfo(QDir(root).filePath(reference));
        if (!candidate.exists() || !candidate.isFile())
            continue;
        const auto canonical = candidate.canonicalFilePath();
        if (canonical == rootPath || canonical.startsWith(rootPath + '/'))
            return candidate;
    }
    return {};
}

QJsonObject listLogs(const QJsonObject& parameters)
{
    auto instance = findInstance(parameters.value("instance").toString());
    if (!instance)
        return OperationService::failure(QObject::tr("Instance not found."), 2);
    QJsonArray result;
    for (const auto& root : logRoots(instance)) {
        const auto entries = QDir(root).entryInfoList(QDir::Files | QDir::Readable, QDir::Name | QDir::IgnoreCase);
        for (const auto& entry : entries)
            result.append(QJsonObject{ { "name", entry.fileName() }, { "path", entry.absoluteFilePath() },
                                       { "size", static_cast<qint64>(entry.size()) },
                                       { "modified", entry.lastModified().toString(Qt::ISODate) } });
    }
    return OperationService::success(result);
}

QJsonObject readLog(const QJsonObject& parameters)
{
    auto instance = findInstance(parameters.value("instance").toString());
    if (!instance)
        return OperationService::failure(QObject::tr("Instance not found."), 2);
    const auto file = safeFileInRoots(parameters.value("file").toString(), logRoots(instance));
    if (!file.exists())
        return OperationService::failure(QObject::tr("Log file not found: %1").arg(parameters.value("file").toString()), 2);
    QFile input(file.absoluteFilePath());
    if (!input.open(QIODevice::ReadOnly))
        return OperationService::failure(QObject::tr("Could not read log file: %1").arg(input.errorString()));
    const auto maxBytes = qBound<qint64>(1, parameters.value("maxBytes").toInteger(1024 * 1024), qint64(16 * 1024 * 1024));
    return OperationService::success(QJsonObject{ { "path", file.absoluteFilePath() },
                                                  { "content", QString::fromUtf8(input.read(maxBytes)) },
                                                  { "truncated", !input.atEnd() } });
}

QJsonObject listScreenshots(const QJsonObject& parameters)
{
    auto instance = findInstance(parameters.value("instance").toString());
    if (!instance)
        return OperationService::failure(QObject::tr("Instance not found."), 2);
    const auto root = QDir(instance->gameRoot()).filePath("screenshots");
    QJsonArray result;
    const auto entries = QDir(root).entryInfoList({ "*.png", "*.jpg", "*.jpeg" }, QDir::Files | QDir::Readable, QDir::Name);
    for (const auto& entry : entries)
        result.append(QJsonObject{ { "name", entry.fileName() }, { "path", entry.absoluteFilePath() },
                                   { "size", static_cast<qint64>(entry.size()) },
                                   { "modified", entry.lastModified().toString(Qt::ISODate) } });
    return OperationService::success(result);
}

QJsonObject deleteScreenshot(const QJsonObject& parameters)
{
    if (!parameters.value("confirm").toBool())
        return OperationService::failure(QObject::tr("Screenshot deletion requires confirm=true."), 2);
    auto instance = findInstance(parameters.value("instance").toString());
    if (!instance)
        return OperationService::failure(QObject::tr("Instance not found."), 2);
    const auto root = QDir(instance->gameRoot()).filePath("screenshots");
    const auto file = safeFileInRoots(parameters.value("file").toString(), { root });
    if (!file.exists() || !QFile::remove(file.absoluteFilePath()))
        return OperationService::failure(QObject::tr("Screenshot could not be deleted."));
    return OperationService::success(QJsonObject{ { "path", file.absoluteFilePath() }, { "deleted", true } });
}

}  // namespace

void registerLauncherApiDomains(LauncherApi& api)
{
    api.registerOperation({ "runtime.info", "Read launcher paths, platform, and optional integration capabilities.", objectSchema({}) },
                           [](const QJsonObject&, UserInteraction&) {
                               const auto caps = APPLICATION->capabilities();
                               QJsonArray capabilities;
                               if (caps & Application::SupportsMSA)
                                   capabilities.append("microsoft-auth");
                               if (caps & Application::SupportsFlame)
                                   capabilities.append("curseforge");
                               if (caps & Application::SupportsGameMode)
                                   capabilities.append("game-mode");
                               if (caps & Application::SupportsMangoHud)
                                   capabilities.append("mangohud");
                               return OperationService::success(QJsonObject{ { "applicationVersion", QCoreApplication::applicationVersion() },
                                                                               { "root", APPLICATION->root() },
                                                                               { "dataRoot", APPLICATION->dataRoot() },
                                                                               { "javaRoot", APPLICATION->javaPath() },
                                                                               { "portable", APPLICATION->isPortable() },
                                                                               { "headless", APPLICATION->isHeadless() },
                                                                               { "capabilities", capabilities } });
                           });

    api.registerOperation({ "account.move", "Move an account by a relative offset or to an absolute list position.",
                            objectSchema({ { "account", stringProperty("Account ID or profile name.") },
                                           { "delta", QJsonObject{ { "type", "integer" }, { "description", "Relative movement." } } },
                                           { "index", QJsonObject{ { "type", "integer" }, { "description", "Absolute list position." } } } },
                                          { "account" }) },
                           [](const QJsonObject& parameters, UserInteraction&) {
                               int row = -1;
                               const auto account = findAccount(parameters.value("account").toString(), &row);
                               if (!account)
                                   return OperationService::failure(QObject::tr("Account not found: %1").arg(parameters.value("account").toString()), 2);
                               int delta = parameters.value("delta").toInt(0);
                               if (parameters.contains("index"))
                                   delta = parameters.value("index").toInt() - row;
                               if (delta == 0)
                                   return OperationService::success(QJsonObject{ { "id", account->profileId() }, { "index", row }, { "changed", false } });
                               if (row + delta < 0 || row + delta >= APPLICATION->accounts()->count())
                                   return OperationService::failure(QObject::tr("The requested account position is outside the account list."), 2);
                               APPLICATION->accounts()->moveAccount(APPLICATION->accounts()->index(row, 0), delta);
                               APPLICATION->accounts()->saveList();
                               return OperationService::success(QJsonObject{ { "id", account->profileId() },
                                                                               { "index", row + delta },
                                                                               { "changed", true } });
                           });

    api.registerOperation({ "account.profile", "Read the selected Minecraft profile, skin, and capes for an account.",
                            objectSchema({ { "account", stringProperty("Account ID or profile name.") } }, { "account" }) },
                           [](const QJsonObject& parameters, UserInteraction&) {
                               const auto account = findAccount(parameters.value("account").toString());
                               if (!account)
                                   return OperationService::failure(QObject::tr("Account not found: %1").arg(parameters.value("account").toString()), 2);

                               const auto* data = account->accountData();
                               const auto& profile = data->minecraftProfile;
                               QJsonObject skin{ { "id", profile.skin.id }, { "url", profile.skin.url }, { "variant", profile.skin.variant } };
                               QJsonArray capes;
                               for (auto it = profile.capes.constBegin(); it != profile.capes.constEnd(); ++it)
                                   capes.append(QJsonObject{ { "id", it.value().id }, { "url", it.value().url }, { "alias", it.value().alias },
                                                             { "current", it.key() == profile.currentCape } });

                               QString state;
                               switch (data->accountState) {
                                   case AccountState::Unchecked:
                                       state = "unchecked";
                                       break;
                                   case AccountState::Offline:
                                       state = "offline";
                                       break;
                                   case AccountState::Working:
                                       state = "working";
                                       break;
                                   case AccountState::Online:
                                       state = "online";
                                       break;
                                   case AccountState::Disabled:
                                       state = "disabled";
                                       break;
                                   case AccountState::Errored:
                                       state = "errored";
                                       break;
                                   case AccountState::Expired:
                                       state = "expired";
                                       break;
                                   case AccountState::Gone:
                                       state = "gone";
                                       break;
                               }
                               return OperationService::success(QJsonObject{ { "id", account->profileId() },
                                                                               { "name", account->profileName() },
                                                                               { "type", account->typeString() },
                                                                               { "state", state },
                                                                               { "ownsMinecraft", data->minecraftEntitlement.ownsMinecraft },
                                                                               { "canPlayMinecraft", data->minecraftEntitlement.canPlayMinecraft },
                                                                               { "skin", skin },
                                                                               { "currentCape", profile.currentCape },
                                                                               { "capes", capes } });
                           });

    const auto accountRef = stringProperty("Account ID or profile name.");
    api.registerOperation({ "account.skin.list", "List locally stored skins for an account.",
                            objectSchema({ { "account", accountRef } }, { "account" }), "account.skins" },
                           [](const QJsonObject& parameters, UserInteraction&) { return listAccountSkins(parameters); });
    api.registerOperation({ "account.skin.install", "Add a local PNG skin to the launcher skin library.",
                            objectSchema({ { "account", accountRef }, { "path", stringProperty("Skin PNG path.") },
                                           { "name", stringProperty("Optional skin file name.") } },
                                          { "account", "path" }), "account.skins" },
                           [](const QJsonObject& parameters, UserInteraction&) { return installAccountSkin(parameters); });
    api.registerOperation({ "account.skin.delete", "Delete a local skin from the launcher skin library.",
                            objectSchema({ { "account", accountRef }, { "skin", stringProperty("Skin name or path.") },
                                           { "confirm", boolProperty("Confirm deletion.") }, { "trash", boolProperty("Move to trash.") } },
                                          { "account", "skin", "confirm" }),
                            "account.skins", true },
                           [](const QJsonObject& parameters, UserInteraction&) { return deleteAccountSkin(parameters); });
    api.registerOperation({ "account.skin.upload", "Upload a local skin and optionally equip a cape.",
                            objectSchema({ { "account", accountRef }, { "skin", stringProperty("Skin name or path.") },
                                           { "variant", stringProperty("classic or slim.") }, { "cape", stringProperty("Cape id, or empty to remove it.") } },
                                          { "account", "skin" }),
                            "account.skins", true },
                           [&api](const QJsonObject& parameters, UserInteraction& interaction) {
                               return uploadAccountSkin(api, parameters, interaction);
                           });
    api.registerOperation({ "account.skin.reset", "Remove the active remote skin.",
                            objectSchema({ { "account", accountRef }, { "confirm", boolProperty("Confirm reset.") } }, { "account", "confirm" }),
                            "account.skins", true },
                           [&api](const QJsonObject& parameters, UserInteraction& interaction) {
                               return resetAccountSkin(api, parameters, interaction);
                           });
    api.registerOperation({ "account.cape.select", "Equip or remove an account cape.",
                            objectSchema({ { "account", accountRef }, { "cape", stringProperty("Cape id, or empty to remove it.") } },
                                          { "account" }),
                            "account.skins", true },
                           [&api](const QJsonObject& parameters, UserInteraction& interaction) {
                               return selectAccountCape(api, parameters, interaction);
                           });

    api.registerOperation({ "instance.world.list", "List worlds stored in a Minecraft instance.",
                            objectSchema({ { "instance", stringProperty("Instance ID or name.") } }, { "instance" }) },
                           [](const QJsonObject& parameters, UserInteraction&) {
                               auto instance = dynamic_cast<MinecraftInstance*>(findInstance(parameters.value("instance").toString()));
                               if (!instance)
                                   return OperationService::failure(QObject::tr("Minecraft instance not found: %1").arg(parameters.value("instance").toString()), 2);
                               auto worlds = instance->worldList();
                               worlds->update();
                               QJsonArray result;
                               for (const auto& world : worlds->allWorlds())
                                   result.append(worldJson(world));
                               return OperationService::success(result);
                           });

    api.registerOperation({ "instance.world.rename", "Rename a world folder and its level name.",
                            objectSchema({ { "instance", stringProperty("Instance ID or name.") },
                                           { "world", stringProperty("World folder or display name.") },
                                           { "name", stringProperty("New world name.") } },
                                          { "instance", "world", "name" }) },
                           [](const QJsonObject& parameters, UserInteraction&) {
                               auto instance = dynamic_cast<MinecraftInstance*>(findInstance(parameters.value("instance").toString()));
                               if (!instance)
                                   return OperationService::failure(QObject::tr("Minecraft instance not found."), 2);
                               if (instance->isRunning())
                                   return OperationService::failure(QObject::tr("Worlds cannot be modified while the instance is running."), 2);
                               auto worlds = instance->worldList();
                               worlds->update();
                               auto world = findWorld(worlds.get(), parameters.value("world").toString());
                               const auto name = parameters.value("name").toString().trimmed();
                               if (!world || name.isEmpty())
                                   return OperationService::failure(QObject::tr("A world and non-empty name are required."), 2);
                               if (!world->rename(name))
                                   return OperationService::failure(QObject::tr("The world could not be renamed."));
                               worlds->update();
                               return OperationService::success(QJsonObject{ { "name", name }, { "changed", true } });
                           });

    api.registerOperation({ "instance.world.delete", "Delete a world from a Minecraft instance.",
                            objectSchema({ { "instance", stringProperty("Instance ID or name.") },
                                           { "world", stringProperty("World folder or display name.") },
                                           { "confirm", boolProperty("Confirm deletion.") } },
                                          { "instance", "world", "confirm" }), {}, true },
                           [](const QJsonObject& parameters, UserInteraction&) {
                               if (!parameters.value("confirm").toBool())
                                   return OperationService::failure(QObject::tr("World deletion requires confirm=true."), 2);
                               auto instance = dynamic_cast<MinecraftInstance*>(findInstance(parameters.value("instance").toString()));
                               if (!instance)
                                   return OperationService::failure(QObject::tr("Minecraft instance not found."), 2);
                               if (instance->isRunning())
                                   return OperationService::failure(QObject::tr("Worlds cannot be modified while the instance is running."), 2);
                               auto worlds = instance->worldList();
                               worlds->update();
                               auto world = findWorld(worlds.get(), parameters.value("world").toString());
                               if (!world || !world->destroy())
                                   return OperationService::failure(QObject::tr("The world could not be deleted."));
                               worlds->update();
                               return OperationService::success(QJsonObject{ { "deleted", true } });
                           });

    api.registerOperation({ "instance.world.reset-icon", "Remove the icon associated with a world.",
                            objectSchema({ { "instance", stringProperty("Instance ID or name.") },
                                           { "world", stringProperty("World folder or display name.") } },
                                          { "instance", "world" }) },
                           [](const QJsonObject& parameters, UserInteraction&) {
                               auto instance = dynamic_cast<MinecraftInstance*>(findInstance(parameters.value("instance").toString()));
                               if (!instance)
                                   return OperationService::failure(QObject::tr("Minecraft instance not found."), 2);
                               auto worlds = instance->worldList();
                               worlds->update();
                               auto world = findWorld(worlds.get(), parameters.value("world").toString());
                               if (!world || !world->resetIcon())
                                   return OperationService::failure(QObject::tr("The world icon could not be removed."));
                               worlds->update();
                               return OperationService::success(QJsonObject{ { "reset", true } });
                           });

    api.registerOperation({ "instance.log.list", "List log files available for an instance.",
                            objectSchema({ { "instance", stringProperty("Instance ID or name.") } }, { "instance" }) },
                           [](const QJsonObject& parameters, UserInteraction&) { return listLogs(parameters); });
    api.registerOperation({ "instance.log.read", "Read a bounded portion of an instance log file.",
                            objectSchema({ { "instance", stringProperty("Instance ID or name.") },
                                           { "file", stringProperty("Log file name or path returned by log.list.") },
                                           { "maxBytes", QJsonObject{ { "type", "integer" }, { "default", 1048576 } } } },
                                          { "instance", "file" }) },
                           [](const QJsonObject& parameters, UserInteraction&) { return readLog(parameters); });
    api.registerOperation({ "instance.screenshot.list", "List screenshots in an instance.",
                            objectSchema({ { "instance", stringProperty("Instance ID or name.") } }, { "instance" }) },
                           [](const QJsonObject& parameters, UserInteraction&) { return listScreenshots(parameters); });
    api.registerOperation({ "instance.screenshot.delete", "Delete an instance screenshot.",
                            objectSchema({ { "instance", stringProperty("Instance ID or name.") },
                                           { "file", stringProperty("Screenshot name or path returned by screenshot.list.") },
                                           { "confirm", boolProperty("Confirm deletion.") } },
                                          { "instance", "file", "confirm" }), {}, true },
                           [](const QJsonObject& parameters, UserInteraction&) { return deleteScreenshot(parameters); });
}
