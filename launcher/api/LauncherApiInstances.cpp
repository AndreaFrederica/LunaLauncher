// SPDX-License-Identifier: GPL-3.0-only

#include "LauncherApiInstances.h"

#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonArray>
#include <QUrl>

#include "Application.h"
#include "BaseInstance.h"
#include "cli/OperationService.h"
#include "DesktopServices.h"
#include "InstanceImportTask.h"
#include "InstanceList.h"
#include "InstanceTask.h"
#include "LaunchController.h"
#include "LauncherApi.h"
#include "icons/IconList.h"
#include "icons/IconUtils.h"
#include "minecraft/ShortcutUtils.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/update/AssetUpdateTask.h"
#include "minecraft/VanillaInstanceCreationTask.h"
#include "meta/Index.h"
#include "meta/Version.h"
#include "server/ServerInstance.h"
#include "server/ServerInstanceCreationTask.h"
#include "tasks/Task.h"

namespace {

QJsonObject stringProperty(const QString& description)
{
    return { { "type", "string" }, { "description", description } };
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

QString loaderUid(QString loader)
{
    loader = loader.trimmed().toLower();
    static const QHash<QString, QString> aliases{ { "fabric", "net.fabricmc.fabric-loader" },
                                                  { "quilt", "org.quiltmc.quilt-loader" },
                                                  { "forge", "net.minecraftforge" },
                                                  { "neoforge", "net.neoforged" },
                                                  { "cleanroom", "com.cleanroommc.cleanroom" },
                                                  { "liteloader", "com.mumfrey.liteloader" } };
    return aliases.value(loader, loader);
}

bool waitForTask(Task* task, UserInteraction& interaction, QString* error, LauncherApi* api = nullptr)
{
    if (!task) {
        if (error)
            *error = QObject::tr("The operation did not create a task.");
        return false;
    }
    QEventLoop loop;
    if (api)
        api->trackTask(task);
    QObject::connect(task, &Task::status, &loop, [&interaction](const QString& status) { interaction.status(status); });
    QObject::connect(task, &Task::finished, &loop, &QEventLoop::quit);
    if (!task->isFinished()) {
        if (!task->isRunning())
            task->start();
        if (!task->isFinished())
            loop.exec();
    }
    if (api)
        api->clearTrackedTask(task);
    if (!task->wasSuccessful() && error)
        *error = task->failReason().isEmpty() ? QObject::tr("The operation was aborted.") : task->failReason();
    return task->wasSuccessful();
}

QJsonObject createInstance(const QJsonObject& parameters, UserInteraction& interaction, LauncherApi& api)
{
    const auto source = parameters.value("source").toString().trimmed();
    const auto type = parameters.value("type").toString().trimmed().toLower();
    const auto name = parameters.value("name").toString().trimmed();
    const auto group = parameters.value("group").toString();
    const auto icon = parameters.value("icon").toString("default");

    InstanceTask* rawTask = nullptr;
    if (!source.isEmpty()) {
        const QFileInfo localFile(source);
        QUrl url = localFile.exists() && localFile.isFile() ? QUrl::fromLocalFile(localFile.absoluteFilePath())
                                                             : QUrl(source, QUrl::TolerantMode);
        if (!url.isValid() || (url.scheme().isEmpty() && !url.isLocalFile()))
            return OperationService::failure(QObject::tr("The instance source is not a valid path or URL."), 2);
        rawTask = new InstanceImportTask(url);
    } else if (type == "server") {
        const auto executable = parameters.value("executable").toString();
        QStringList arguments;
        if (parameters.value("arguments").isArray()) {
            for (const auto& argument : parameters.value("arguments").toArray())
                arguments.append(argument.toString());
        } else {
            arguments = parameters.value("arguments").toString().split(' ', Qt::SkipEmptyParts);
        }
        rawTask = new ServerInstanceCreationTask(executable, arguments);
    } else {
        const auto versionName = parameters.value("version").toString().trimmed();
        if (versionName.isEmpty())
            return OperationService::failure(QObject::tr("A Minecraft version is required."), 2);

        interaction.status(QObject::tr("Loading Minecraft version %1...").arg(versionName));
        const auto version = APPLICATION->metadataIndex()->getLoadedVersion("net.minecraft", versionName);
        if (!version || !version->isLoaded())
            return OperationService::failure(QObject::tr("Minecraft version not found: %1").arg(versionName), 2);

        const auto requestedLoader = parameters.value("loader").toString();
        if (requestedLoader.isEmpty()) {
            rawTask = new VanillaCreationTask(version);
        } else {
            const auto loader = loaderUid(requestedLoader);
            auto loaderVersionName = parameters.value("loaderVersion").toString().trimmed();
            if (loaderVersionName.isEmpty())
                return OperationService::failure(QObject::tr("loaderVersion is required when loader is specified."), 2);
            interaction.status(QObject::tr("Loading %1 version %2...").arg(loader, loaderVersionName));
            const auto loaderVersion = APPLICATION->metadataIndex()->getLoadedVersion(loader, loaderVersionName);
            if (!loaderVersion || !loaderVersion->isLoaded())
                return OperationService::failure(QObject::tr("Loader version not found: %1:%2").arg(loader, loaderVersionName), 2);
            rawTask = new VanillaCreationTask(version, loader, loaderVersion);
        }
    }

    if (!rawTask)
        return OperationService::failure(QObject::tr("Could not create an instance task."));
    if (!name.isEmpty())
        rawTask->setName(name);
    rawTask->setGroup(group);
    rawTask->setIcon(icon);

    QSet<QString> previousIds;
    for (int i = 0; i < APPLICATION->instances()->count(); ++i)
        previousIds.insert(APPLICATION->instances()->at(i)->id());

    auto task = APPLICATION->instances()->wrapInstanceTask(rawTask);
    QString error;
    if (!waitForTask(task, interaction, &error, &api))
        return OperationService::failure(error);
    APPLICATION->instances()->saveNow();

    for (int i = 0; i < APPLICATION->instances()->count(); ++i) {
        auto instance = APPLICATION->instances()->at(i);
        if (!previousIds.contains(instance->id()))
            return OperationService::success(QJsonObject{ { "id", instance->id() },
                                                           { "name", instance->name() },
                                                           { "root", instance->instanceRoot() } });
    }
    return OperationService::success(QJsonObject{ { "name", name } });
}

QJsonObject stopInstance(const QJsonObject& parameters, bool force)
{
    const auto reference = parameters.value("instance").toString();
    auto instance = findInstance(reference);
    if (!instance)
        return OperationService::failure(QObject::tr("Instance not found: %1").arg(reference), 2);
    if (!instance->isRunning())
        return OperationService::success(QJsonObject{ { "id", instance->id() }, { "running", false }, { "stopped", false } });

    bool stopped = false;
    if (auto server = dynamic_cast<ServerInstance*>(instance)) {
        server->stopServer();
        stopped = !server->isRunning();
    } else {
        // Application::kill is the existing launcher-wide lifecycle hook. It
        // also releases the controller held by Application, which direct task
        // abortion would bypass.
        stopped = APPLICATION->kill(instance);
    }
    if (!stopped)
        return OperationService::failure(force ? QObject::tr("The instance could not be killed.")
                                               : QObject::tr("The instance could not be stopped."));
    return OperationService::success(QJsonObject{ { "id", instance->id() }, { "running", instance->isRunning() }, { "stopped", true } });
}

QJsonObject openInstanceFolder(const QJsonObject& parameters)
{
    auto instance = findInstance(parameters.value("instance").toString());
    if (!instance)
        return OperationService::failure(QObject::tr("Instance not found: %1").arg(parameters.value("instance").toString()), 2);
    auto kind = parameters.value("kind").toString("root").trimmed().toLower();
    QString path = instance->instanceRoot();
    if (kind == "game")
        path = instance->gameRoot();
    else if (kind == "mods")
        path = instance->modsRoot();
    else if (kind == "screenshots")
        path = QDir(instance->gameRoot()).filePath("screenshots");
    else if (kind == "logs") {
        const auto paths = instance->getLogFileSearchPaths();
        if (!paths.isEmpty())
            path = paths.first();
    } else if (kind != "root") {
        return OperationService::failure(QObject::tr("Unknown instance folder kind: %1").arg(kind), 2);
    }
    const bool opened = DesktopServices::openPath(path, true);
    return opened ? OperationService::success(QJsonObject{ { "path", path } })
                  : OperationService::failure(QObject::tr("Could not open instance folder: %1").arg(path));
}

QJsonObject setInstanceIcon(const QJsonObject& parameters)
{
    auto instance = findInstance(parameters.value("instance").toString());
    if (!instance)
        return OperationService::failure(QObject::tr("Instance not found: %1").arg(parameters.value("instance").toString()), 2);
    auto key = parameters.value("icon").toString().trimmed();
    const auto source = parameters.value("path").toString().trimmed();
    bool imported = false;
    if (!source.isEmpty()) {
        if (!QFileInfo(source).isFile())
            return OperationService::failure(QObject::tr("Icon file not found: %1").arg(source), 2);
        auto name = parameters.value("name").toString().trimmed();
        if (name.isEmpty())
            name = QFileInfo(source).fileName();
        if (!IconUtils::isIconSuffix(QFileInfo(name).suffix().toLower()))
            return OperationService::failure(QObject::tr("Unsupported icon file type: %1").arg(name), 2);
        APPLICATION->icons()->installIcon(source, name);
        key = QFileInfo(name).completeBaseName();
        imported = QFileInfo(QDir(APPLICATION->icons()->getDirectory()).filePath(name)).exists();
    }
    if (key.isEmpty() || (!imported && APPLICATION->icons()->getIcon(key).isNull()))
        return OperationService::failure(QObject::tr("Unknown or invalid icon: %1").arg(key), 2);
    instance->setIconKey(key);
    instance->saveNow();
    return OperationService::success(QJsonObject{ { "id", instance->id() }, { "icon", key } });
}

QJsonObject setInstanceNotes(const QJsonObject& parameters)
{
    auto instance = findInstance(parameters.value("instance").toString());
    if (!instance)
        return OperationService::failure(QObject::tr("Instance not found: %1").arg(parameters.value("instance").toString()), 2);
    instance->setNotes(parameters.value("notes").toString());
    instance->saveNow();
    return OperationService::success(QJsonObject{ { "id", instance->id() }, { "notes", instance->notes() } });
}

QJsonObject verifyInstance(LauncherApi& api, const QJsonObject& parameters, UserInteraction& interaction)
{
    auto instance = dynamic_cast<MinecraftInstance*>(findInstance(parameters.value("instance").toString()));
    if (!instance)
        return OperationService::failure(QObject::tr("Minecraft instance not found: %1").arg(parameters.value("instance").toString()), 2);

    auto* settings = instance->settings();
    const bool hadOverride = settings->get("OverrideAssetVerification").toBool();
    const int savedMode = settings->get("AssetVerificationMode").toInt();
    settings->set("OverrideAssetVerification", true);
    settings->set("AssetVerificationMode", static_cast<int>(AlwaysVerify));

    auto task = new AssetUpdateTask(instance, true);
    QString error;
    const bool successful = waitForTask(task, interaction, &error, &api);

    settings->set("OverrideAssetVerification", hadOverride);
    if (hadOverride)
        settings->set("AssetVerificationMode", savedMode);
    else
        settings->reset("AssetVerificationMode");

    if (!successful)
        return OperationService::failure(error);
    return OperationService::success(QJsonObject{ { "id", instance->id() }, { "verified", true } });
}

QJsonObject createInstanceShortcut(const QJsonObject& parameters)
{
    auto instance = findInstance(parameters.value("instance").toString());
    if (!instance)
        return OperationService::failure(QObject::tr("Instance not found: %1").arg(parameters.value("instance").toString()), 2);
    ShortcutUtils::Shortcut shortcut;
    shortcut.instance = instance;
    shortcut.name = parameters.value("name").toString(instance->name()).trimmed();
    shortcut.iconKey = parameters.value("icon").toString();
    shortcut.targetString = "instance";
    shortcut.target = ShortcutTarget::Other;
    for (const auto& argument : parameters.value("extraArgs").toArray())
        shortcut.extraArgs.append(argument.toString());

    const auto target = parameters.value("target").toString("path").trimmed().toLower();
    bool created = false;
    if (target == "desktop") {
        shortcut.target = ShortcutTarget::Desktop;
        created = ShortcutUtils::createInstanceShortcutOnDesktop(shortcut);
    } else if (target == "applications") {
        shortcut.target = ShortcutTarget::Applications;
        created = ShortcutUtils::createInstanceShortcutInApplications(shortcut);
    } else {
        const auto path = parameters.value("path").toString().trimmed();
        if (path.isEmpty())
            return OperationService::failure(QObject::tr("A shortcut path or target is required."), 2);
        created = ShortcutUtils::createInstanceShortcut(shortcut, path);
    }
    return created ? OperationService::success(QJsonObject{ { "name", shortcut.name }, { "target", target } })
                   : OperationService::failure(QObject::tr("Could not create instance shortcut."));
}

}  // namespace

void registerInstanceApiOperations(LauncherApi& api)
{
    const auto instance = stringProperty("Instance ID, managed name, or display name.");
    api.registerOperation({ "instance.create", "Create a vanilla, server, or imported instance.",
                            objectSchema({ { "type", stringProperty("vanilla or server; defaults to vanilla.") },
                                           { "version", stringProperty("Minecraft version for vanilla.") },
                                           { "loader", stringProperty("Optional loader id or alias.") },
                                           { "loaderVersion", stringProperty("Optional loader version.") },
                                           { "source", stringProperty("Local path or URL to an instance pack.") },
                                           { "name", stringProperty("Instance name.") }, { "group", stringProperty("Instance group.") },
                                           { "icon", stringProperty("Icon key.") }, { "executable", stringProperty("Server executable.") },
                                           { "arguments", QJsonObject{ { "description", "Server arguments." } } } }) },
                         [&api](const QJsonObject& parameters, UserInteraction& interaction) { return createInstance(parameters, interaction, api); });
    api.registerOperation({ "instance.stop", "Stop a running instance.", objectSchema({ { "instance", instance } }, { "instance" }) },
                           [](const QJsonObject& parameters, UserInteraction&) { return stopInstance(parameters, false); });
    api.registerOperation({ "instance.kill", "Force-stop a running instance.", objectSchema({ { "instance", instance } }, { "instance" }),
                            {}, true },
                           [](const QJsonObject& parameters, UserInteraction&) { return stopInstance(parameters, true); });
    api.registerOperation({ "instance.open-folder", "Open an instance directory.",
                            objectSchema({ { "instance", instance }, { "kind", stringProperty("root, game, mods, logs, or screenshots.") } },
                                          { "instance" }) },
                           [](const QJsonObject& parameters, UserInteraction&) { return openInstanceFolder(parameters); });
    api.registerOperation({ "instance.set-icon", "Assign a built-in or imported icon to an instance.",
                            objectSchema({ { "instance", instance }, { "icon", stringProperty("Icon key.") },
                                           { "path", stringProperty("Icon file to import.") },
                                           { "name", stringProperty("Imported icon filename.") } },
                                          { "instance" }) },
                           [](const QJsonObject& parameters, UserInteraction&) { return setInstanceIcon(parameters); });
    api.registerOperation({ "instance.set-notes", "Set the notes shown for an instance.",
                            objectSchema({ { "instance", instance }, { "notes", stringProperty("Instance notes.") } }, { "instance", "notes" }) },
                           [](const QJsonObject& parameters, UserInteraction&) { return setInstanceNotes(parameters); });
    api.registerOperation({ "instance.verify", "Verify and repair Minecraft asset integrity.",
                            objectSchema({ { "instance", instance } }, { "instance" }) },
                           [&api](const QJsonObject& parameters, UserInteraction& interaction) {
                               return verifyInstance(api, parameters, interaction);
                           });
    api.registerOperation({ "instance.create-shortcut", "Create a launcher shortcut for an instance.",
                            objectSchema({ { "instance", instance }, { "target", stringProperty("desktop, applications, or path.") },
                                           { "path", stringProperty("Shortcut path when target=path.") },
                                           { "name", stringProperty("Shortcut name.") },
                                           { "extraArgs", QJsonObject{ { "type", "array" }, { "items", stringProperty("Launcher argument.") } } } },
                                          { "instance" }) },
                           [](const QJsonObject& parameters, UserInteraction&) { return createInstanceShortcut(parameters); });
}
