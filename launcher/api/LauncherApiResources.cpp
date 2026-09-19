// SPDX-License-Identifier: GPL-3.0-only

#include "LauncherApiResources.h"

#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QUrl>
#include <QStandardPaths>
#include <QProcess>
#include <QRegularExpression>

#include <memory>

#include "Application.h"
#include "BaseInstance.h"
#include "FileSystem.h"
#include "InstanceList.h"
#include "api/ApiTypes.h"
#include "api/LauncherApi.h"
#include "cli/OperationService.h"
#include "cli/UserInteraction.h"
#include "java/JavaInstall.h"
#include "java/JavaInstallList.h"
#include "java/JavaMetadata.h"
#include "java/download/ArchiveDownloadTask.h"
#include "java/download/ManifestDownloadTask.h"
#include "minecraft/MinecraftInstance.h"
#include "server/ServerInstance.h"
#include "minecraft/mod/PluginFolderModel.h"
#include "minecraft/mod/ModFolderModel.h"
#include "minecraft/mod/Resource.h"
#include "minecraft/mod/ResourceFolderModel.h"
#include "settings/SettingsObject.h"
#include "tasks/Task.h"

namespace {

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

std::shared_ptr<ResourceFolderModel> findResourceModel(BaseInstance* instance, QString kind)
{
    kind = kind.toLower().remove('-').remove('_');
    if (auto server = dynamic_cast<ServerInstance*>(instance)) {
        if (kind == "mods") return server->loaderModList();
        if (kind == "plugins") return server->pluginList();
        return nullptr;
    }
    static const QMap<QString, int> indexes{ { "mods", 0 },           { "coremods", 1 },           { "nilmods", 2 },
                                             { "resourcepacks", 3 },  { "texturepacks", 4 },       { "shaderpacks", 5 },
                                             { "yesstevemodels", 6 }, { "customplayermodels", 7 }, { "schematics", 8 },
                                             { "datapacks", 9 } };
    if (!indexes.contains(kind))
        return nullptr;
    const auto client = dynamic_cast<MinecraftInstance*>(instance);
    if (!client) return nullptr;
    const auto lists = client->resourceLists();
    const auto index = indexes.value(kind);
    return index < lists.size() ? lists.at(index) : nullptr;
}

std::unique_ptr<Resource> findResource(const std::shared_ptr<ResourceFolderModel>& model, const QString& reference)
{
    for (const auto& entry : model->dir().entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
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

Resource* findLoadedResource(const std::shared_ptr<ResourceFolderModel>& model, const QString& reference)
{
    for (auto* resource : model->allResources()) {
        if (resource->internal_id() == reference || resource->fileinfo().fileName().compare(reference, Qt::CaseInsensitive) == 0 ||
            resource->getOriginalFileName().compare(reference, Qt::CaseInsensitive) == 0 ||
            resource->name().compare(reference, Qt::CaseInsensitive) == 0)
            return resource;
    }
    return nullptr;
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
        default:
            return "unknown";
    }
}

QString resourceStatusName(ResourceStatus status)
{
    switch (status) {
        case ResourceStatus::INSTALLED:
            return "installed";
        case ResourceStatus::NOT_INSTALLED:
            return "not-installed";
        case ResourceStatus::NO_METADATA:
            return "no-metadata";
        default:
            return "unknown";
    }
}

QJsonObject resourceInfo(const Resource& resource)
{
    QJsonObject result{ { "id", resource.internal_id() },
                        { "name", resource.name() },
                        { "fileName", resource.fileinfo().fileName() },
                        { "originalFileName", resource.getOriginalFileName() },
                        { "enabled", resource.enabled() },
                        { "type", resourceTypeName(resource.type()) },
                        { "status", resourceStatusName(resource.status()) },
                        { "valid", resource.valid() },
                        { "size", resource.sizeInfo() },
                        { "sizeString", resource.sizeStr() },
                        { "path", resource.fileinfo().absoluteFilePath() },
                        { "modified", resource.dateTimeChanged().toString(Qt::ISODate) },
                        { "provider", resource.provider() },
                        { "homepage", resource.homepage() },
                        { "resolved", resource.isResolved() },
                        { "resolving", resource.isResolving() } };

    QJsonArray issues;
    for (const auto& issue : resource.issues())
        issues.append(issue);
    result.insert("issues", issues);

    if (const auto metadata = resource.metadata()) {
        QJsonObject metadataObject{ { "slug", metadata->slug },
                                    { "name", metadata->name },
                                    { "filename", metadata->filename },
                                    { "side", static_cast<int>(metadata->side) },
                                    { "mode", metadata->mode },
                                    { "url", metadata->url.toString() },
                                    { "hashFormat", metadata->hash_format },
                                    { "hash", metadata->hash },
                                    { "provider", static_cast<int>(metadata->provider) },
                                    { "projectId", QJsonValue::fromVariant(metadata->project_id) },
                                    { "fileId", QJsonValue::fromVariant(metadata->file_id) },
                                    { "version", metadata->version_number } };
        QJsonArray gameVersions;
        for (const auto& version : metadata->mcVersions)
            gameVersions.append(version);
        metadataObject.insert("minecraftVersions", gameVersions);
        QJsonArray dependencies;
        for (const auto& dependency : metadata->dependencies)
            dependencies.append(QJsonObject{ { "addonId", QJsonValue::fromVariant(dependency.addonId) },
                                             { "type", static_cast<int>(dependency.type) },
                                             { "version", dependency.version } });
        metadataObject.insert("dependencies", dependencies);
        result.insert("metadata", metadataObject);
    }
    return result;
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

QJsonObject inspectResource(const QJsonObject& parameters)
{
    const auto instance = findInstance(parameters.value("instance").toString());
    if (!instance)
        return OperationService::failure(QObject::tr("Minecraft instance not found: %1").arg(parameters.value("instance").toString()), 2);
    const auto model = findResourceModel(instance, parameters.value("kind").toString());
    if (!model)
        return OperationService::failure(QObject::tr("Unknown or unavailable resource kind: %1").arg(parameters.value("kind").toString()),
                                         2);
    const auto reference = parameters.value("resource").toString();
    if (const auto* loaded = findLoadedResource(model, reference))
        return OperationService::success(resourceInfo(*loaded));
    const auto resource = findResource(model, reference);
    if (!resource)
        return OperationService::failure(QObject::tr("Resource not found: %1").arg(parameters.value("resource").toString()), 2);
    return OperationService::success(resourceInfo(*resource));
}

QJsonObject updateResources(const QJsonObject& parameters, UserInteraction& interaction)
{
    const auto instance = findInstance(parameters.value("instance").toString());
    if (!instance)
        return OperationService::failure(QObject::tr("Minecraft instance not found: %1").arg(parameters.value("instance").toString()), 2);
    if (instance->isRunning())
        return OperationService::failure(QObject::tr("Resources cannot be updated while the instance is running."), 2);
    const auto model = findResourceModel(instance, parameters.value("kind").toString());
    if (!model)
        return OperationService::failure(QObject::tr("Unknown or unavailable resource kind: %1").arg(parameters.value("kind").toString()),
                                         2);

    QEventLoop loop;
    QObject::connect(model.get(), &ResourceFolderModel::updateFinished, &loop, &QEventLoop::quit);
    if (!model->update())
        return OperationService::success(QJsonObject{ { "updated", false }, { "reason", "already-updating-or-unavailable" } });
    loop.exec();
    Q_UNUSED(interaction);
    return OperationService::success(QJsonObject{ { "updated", true },
                                                  { "instance", instance->id() },
                                                  { "kind", parameters.value("kind").toString() },
                                                  { "count", static_cast<int>(model->size()) } });
}

QString javaRoot()
{
    const auto configuredPath = APPLICATION->javaPath().trimmed();
    return configuredPath.isEmpty() ? QString() : QDir(configuredPath).absolutePath();
}

bool validJavaName(const QString& name)
{
    return !name.isEmpty() && name != "." && name != ".." && !QDir::isAbsolutePath(name) && !name.contains('/') && !name.contains('\\') &&
           !name.contains("..", Qt::CaseSensitive);
}

QJsonObject installJava(const QJsonObject& parameters, UserInteraction& interaction, LauncherApi& api)
{
    QJsonObject metadata = parameters.value("metadata").toObject();
    const auto url = QUrl(parameters.value("url").toString(metadata.value("url").toString()));
    const auto name = parameters.value("name").toString(metadata.value("name").toString());
    const auto type = parameters.value("downloadType").toString(metadata.value("downloadType").toString());
    const auto checksumType = parameters.value("checksumType").toString(metadata.value("checksumType").toString());
    const auto checksumHash = parameters.value("checksumHash").toString(metadata.value("checksumHash").toString());
    if (!url.isValid() || (url.scheme() != "http" && url.scheme() != "https"))
        return OperationService::failure(QObject::tr("Java installation requires an HTTP(S) URL."), 2);
    if (!validJavaName(name))
        return OperationService::failure(QObject::tr("Java installation name is invalid."), 2);
    const auto root = javaRoot();
    if (root.isEmpty())
        return OperationService::failure(QObject::tr("The managed Java directory is not configured."), 2);
    const auto finalPath = QDir(root).filePath(name);
    if (QFileInfo::exists(finalPath)) {
        if (!parameters.value("replace").toBool())
            return OperationService::failure(QObject::tr("Java installation already exists: %1").arg(name), 2);
        if (!FS::deletePath(finalPath))
            return OperationService::failure(QObject::tr("Could not replace existing Java installation: %1").arg(name));
    }

    Task::Ptr task;
    if (type.compare("manifest", Qt::CaseInsensitive) == 0)
        task = makeShared<Java::ManifestDownloadTask>(url, finalPath, checksumType, checksumHash);
    else if (type.compare("archive", Qt::CaseInsensitive) == 0)
        task = makeShared<Java::ArchiveDownloadTask>(url, finalPath, checksumType, checksumHash);
    else
        return OperationService::failure(QObject::tr("Unsupported Java download type: %1").arg(type), 2);

    QString error;
    if (!waitForTask(task.get(), interaction, &error, &api)) {
        FS::deletePath(finalPath);
        return OperationService::failure(error);
    }
    return OperationService::success(QJsonObject{ { "name", name }, { "path", finalPath }, { "downloadType", type } });
}

QJsonObject refreshJava(const QJsonObject& parameters, UserInteraction& interaction, LauncherApi& api)
{
    JavaInstallList list(nullptr, parameters.value("managedOnly").toBool(false));
    const auto task = list.getLoadTask();
    QString error;
    if (task && !waitForTask(task.get(), interaction, &error, &api))
        return OperationService::failure(error);

    QJsonArray installations;
    for (int i = 0; i < list.count(); ++i) {
        const auto java = std::dynamic_pointer_cast<JavaInstall>(list.at(i));
        if (!java)
            continue;
        installations.append(QJsonObject{
            { "version", java->id.toString() }, { "architecture", java->arch }, { "path", java->path }, { "is64Bit", java->is_64bit } });
    }
    return OperationService::success(installations);
}

QString resolveManagedJavaPath(const QString& reference)
{
    const auto configuredRoot = javaRoot();
    if (configuredRoot.isEmpty())
        return {};
    const auto root = QFileInfo(configuredRoot).canonicalFilePath();
    if (root.isEmpty())
        return {};
    const auto requested = QFileInfo(reference);
    const auto requestedCanonical =
        QDir::fromNativeSeparators(requested.exists() ? requested.canonicalFilePath() : QDir(root).filePath(reference));
    for (const auto& entry : QDir(root).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const auto candidate = QDir::fromNativeSeparators(entry.canonicalFilePath());
        if (candidate.isEmpty())
            continue;
        if (requestedCanonical == candidate || requestedCanonical.startsWith(candidate + QLatin1Char('/')))
            return entry.absoluteFilePath();
        if (entry.fileName().compare(reference, Qt::CaseInsensitive) == 0)
            return entry.absoluteFilePath();
    }
    return {};
}

QJsonObject removeJava(const QJsonObject& parameters)
{
    if (!parameters.value("confirm").toBool())
        return OperationService::failure(QObject::tr("Java removal requires confirm=true."), 2);
    const auto reference = parameters.value("name").toString(parameters.value("path").toString());
    const auto path = resolveManagedJavaPath(reference);
    if (path.isEmpty())
        return OperationService::failure(QObject::tr("Managed Java installation not found: %1").arg(reference), 2);
    if (!FS::deletePath(path))
        return OperationService::failure(QObject::tr("Could not remove Java installation: %1").arg(path));
    return OperationService::success(QJsonObject{ { "name", QFileInfo(path).fileName() }, { "path", path }, { "removed", true } });
}

QJsonObject selectJava(const QJsonObject& parameters, UserInteraction& interaction)
{
    auto path = parameters.value("path").toString(parameters.value("java").toString());
    if (path.isEmpty())
        return OperationService::failure(QObject::tr("A Java executable path is required."), 2);
    auto info = QFileInfo(path);
    if (!info.exists()) {
        auto list = APPLICATION->javalist();
        if (!list->isLoaded()) {
            const auto task = list->getLoadTask();
            QString error;
            if (task && !waitForTask(task.get(), interaction, &error))
                return OperationService::failure(error);
        }
        for (int i = 0; i < list->count(); ++i) {
            const auto java = std::dynamic_pointer_cast<JavaInstall>(list->at(i));
            if (java && (java->id.toString().compare(path, Qt::CaseInsensitive) == 0 ||
                         QFileInfo(java->path).fileName().compare(path, Qt::CaseInsensitive) == 0)) {
                path = java->path;
                info = QFileInfo(path);
                break;
            }
        }
    }
    const auto executable = info.exists() ? info.absoluteFilePath() : path;
    if (info.exists() && !info.isFile())
        return OperationService::failure(QObject::tr("The Java path is not a file: %1").arg(path), 2);

    const auto scope = parameters.value("scope").toString("launcher").toLower();
    SettingsObject* settings = nullptr;
    BaseInstance* instance = nullptr;
    if (scope == "launcher" || scope == "global") {
        settings = APPLICATION->settings();
    } else if (scope == "instance") {
        instance = findInstance(parameters.value("instance").toString());
        if (!instance)
            return OperationService::failure(QObject::tr("Instance not found: %1").arg(parameters.value("instance").toString()), 2);
        settings = instance->settings();
    } else {
        return OperationService::failure(QObject::tr("Unknown Java selection scope: %1").arg(scope), 2);
    }
    settings->set("JavaPath", executable);
    if (settings->getSetting("OverrideJavaLocation"))
        settings->set("OverrideJavaLocation", true);
    if (settings->getSetting("AutomaticJava"))
        settings->set("AutomaticJava", false);
    if (instance)
        instance->saveNow();
    return OperationService::success(QJsonObject{ { "path", executable }, { "scope", scope } });
}

QJsonObject diagnoseJava(const QJsonObject& parameters)
{
    const auto scope = parameters.value("scope").toString("launcher").toLower();
    SettingsObject* settings = APPLICATION->settings();
    BaseInstance* instance = nullptr;
    if (scope == "instance") {
        instance = findInstance(parameters.value("instance").toString());
        if (!instance) return OperationService::failure(QObject::tr("Instance not found."), 2);
        settings = instance->settings();
    } else if (scope != "launcher" && scope != "global") {
        return OperationService::failure(QObject::tr("Unknown Java diagnostic scope."), 2);
    }
    const auto configured = settings->get("JavaPath").toString();
    const auto resolved = FS::ResolveExecutable(configured);
    const QFileInfo file(resolved);
    const auto found = QStandardPaths::findExecutable(resolved);
    const auto executable = found.isEmpty() ? resolved : found;
    QProcess process;
    if (!executable.isEmpty() && (file.exists() || !found.isEmpty())) {
        process.start(executable, { "-version" });
        process.waitForFinished(10000);
    }
    const auto output = QString::fromLocal8Bit(process.readAllStandardError() + process.readAllStandardOutput()).trimmed();
    QJsonObject result{ { "scope", scope }, { "configured", configured }, { "resolved", resolved },
                        { "exists", file.exists() || !found.isEmpty() }, { "executable", executable },
                        { "automatic", settings->get("AutomaticJava").toBool() }, { "override", settings->get("OverrideJavaLocation").toBool() },
                        { "javaRoot", javaRoot() }, { "probeStarted", process.processId() != 0 }, { "probeExitCode", process.exitCode() },
                        { "probeOutput", output } };
    QRegularExpression versionRx("(?:openjdk|java) version \\\"([^\\\"]+)\\\"");
    auto match = versionRx.match(output);
    if (match.hasMatch()) result.insert("version", match.captured(1));
    const auto lower = output.toLower();
    result.insert("vendor", lower.contains("openjdk") ? "OpenJDK" : lower.contains("oracle") ? "Oracle" : "Unknown");
    result.insert("architecture", lower.contains("64-bit") || lower.contains("amd64") || lower.contains("x86_64") ? "x86_64" : "unknown");
    if (match.hasMatch()) {
        const auto major = match.captured(1).startsWith("1.") ? match.captured(1).mid(2).section('.', 0, 0) : match.captured(1).section('.', 0, 0);
        result.insert("major", major.toInt());
    }
    result.insert("usable", process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0 && !output.isEmpty());
    return OperationService::success(result);
}

}  // namespace

void registerLauncherApiResourceOperations(LauncherApi& api)
{
    const auto resourceRef = QJsonObject{ { "instance", stringProperty("Instance ID, managed name, or display name.") },
                                          { "kind", stringProperty("Resource kind (mods, resourcepacks, shaderpacks, ...).") },
                                          { "resource", stringProperty("Resource ID, name, or file name.") } };
    api.registerOperation({ "resource.inspect", "Inspect an installed resource and its metadata.",
                            objectSchema(resourceRef, { "instance", "kind", "resource" }), "resources" },
                          [](const QJsonObject& parameters, UserInteraction&) { return inspectResource(parameters); });
    api.registerOperation(
        { "resource.update", "Reload resource files and metadata for an instance.", objectSchema(resourceRef, { "instance", "kind" }),
          "resources" },
        [](const QJsonObject& parameters, UserInteraction& interaction) { return updateResources(parameters, interaction); });
    api.registerOperation({ "java.install", "Install a managed Java runtime from launcher metadata.",
                            objectSchema({ { "url", stringProperty("HTTP(S) manifest or archive URL.") },
                                           { "name", stringProperty("Managed runtime directory name.") },
                                           { "downloadType", stringProperty("manifest or archive.") },
                                           { "checksumType", stringProperty("Optional checksum algorithm.") },
                                           { "checksumHash", stringProperty("Optional checksum hash.") },
                                           { "metadata", QJsonObject{ { "description", "Optional Java metadata object." } } },
                                           { "replace", boolProperty("Replace an existing managed runtime.") } }),
                            "java" },
                          [&api](const QJsonObject& parameters, UserInteraction& interaction) { return installJava(parameters, interaction, api); });
    api.registerOperation({ "java.refresh", "Rescan Java installations and return the current list.",
                            objectSchema({ { "managedOnly", boolProperty("Only scan runtimes managed by the launcher.") } }), "java" },
                          [&api](const QJsonObject& parameters, UserInteraction& interaction) { return refreshJava(parameters, interaction, api); });
    api.registerOperation({ "java.remove", "Remove a managed Java runtime.",
                            objectSchema({ { "name", stringProperty("Managed runtime directory name.") },
                                           { "path", stringProperty("Managed runtime path or executable path.") },
                                           { "confirm", boolProperty("Confirm removal.") } },
                                         { "confirm" }),
                            "java", true },
                          [](const QJsonObject& parameters, UserInteraction&) { return removeJava(parameters); });
    api.registerOperation({ "java.select", "Select a Java executable for launcher or instance settings.",
                            objectSchema({ { "path", stringProperty("Java executable path.") },
                                           { "java", stringProperty("Alias for path.") },
                                           { "scope", stringProperty("launcher or instance.") },
                                           { "instance", stringProperty("Instance ID when scope=instance.") } }),
                            "java" },
                          [](const QJsonObject& parameters, UserInteraction& interaction) { return selectJava(parameters, interaction); });
    api.registerOperation({ "java.diagnose", "Inspect Java selection, resolution, and executable availability.",
                            objectSchema({ { "scope", stringProperty("launcher or instance.") }, { "instance", stringProperty("Instance ID when scope=instance.") } }), "java" },
                          [](const QJsonObject& parameters, UserInteraction&) { return diagnoseJava(parameters); });
}
