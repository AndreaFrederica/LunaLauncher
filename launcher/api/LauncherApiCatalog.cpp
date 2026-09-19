// SPDX-License-Identifier: GPL-3.0-only
#include "LauncherApiCatalog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QUrl>

#include "Application.h"
#include "InstanceList.h"
#include "ResourceDownloadTask.h"
#include "api/LauncherApiSupport.h"
#include "cli/OperationService.h"
#include "meta/Index.h"
#include "meta/Version.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/mod/ModFolderModel.h"
#include "minecraft/mod/PluginFolderModel.h"
#include "minecraft/mod/ResourcePackFolderModel.h"
#include "minecraft/mod/ShaderPackFolderModel.h"
#include "minecraft/mod/DataPackFolderModel.h"
#include "modplatform/ResourceAPI.h"
#include "modplatform/modrinth/ModrinthAPI.h"
#include "modplatform/flame/FlameAPI.h"
#include "modplatform/hangar/HangarAPI.h"
#include "server/ServerInstance.h"

namespace {
using namespace ApiSupport;
using Pack = ModPlatform::IndexedPack;
using PackVersion = ModPlatform::IndexedVersion;
using Kind = ModPlatform::ResourceType;

std::unique_ptr<ResourceAPI> provider(const QString& name)
{
    if (name == "modrinth") return std::make_unique<ModrinthAPI>();
    if (name == "curseforge") return std::make_unique<FlameAPI>();
    if (name == "hangar") return std::make_unique<HangarAPI>();
    return {};
}

const QMap<QString, Kind> kinds{ { "mods", Kind::Mod }, { "plugins", Kind::Plugin }, { "resourcepacks", Kind::ResourcePack },
                               { "shaderpacks", Kind::ShaderPack }, { "datapacks", Kind::DataPack }, { "modpacks", Kind::Modpack } };
QStringList supportedKinds(const QString& name)
{
    if (name == "hangar") return { "plugins" };
    if (name == "curseforge") return { "mods", "resourcepacks", "modpacks" };
    return { "mods", "resourcepacks", "shaderpacks", "datapacks", "modpacks" };
}

QJsonObject packJson(const Pack& pack)
{
    QJsonArray authors;
    for (const auto& author : pack.authors)
        authors.append(QJsonObject{ { "name", author.name }, { "url", author.url } });
    return { { "projectId", pack.addonId.toString() }, { "name", pack.name }, { "slug", pack.slug },
             { "description", pack.description }, { "iconUrl", pack.logoUrl }, { "websiteUrl", pack.websiteUrl },
             { "authors", authors }, { "side", ModPlatform::SideUtils::toString(pack.side) },
             { "body", pack.extraData.body }, { "sourceUrl", pack.extraData.sourceUrl },
             { "issuesUrl", pack.extraData.issuesUrl }, { "wikiUrl", pack.extraData.wikiUrl }, { "discordUrl", pack.extraData.discordUrl } };
}
QJsonObject versionJson(const PackVersion& version)
{
    QJsonArray dependencies;
    for (const auto& dependency : version.dependencies)
        dependencies.append(QJsonObject{ { "projectId", dependency.addonId.toString() },
                                         { "type", ModPlatform::DependencyTypeUtils::toString(dependency.type) },
                                         { "versionId", dependency.version } });
    return { { "projectId", version.addonId.toString() }, { "versionId", version.fileId.toString() },
             { "name", version.version }, { "version", version.version_number }, { "type", version.version_type.toString() },
             { "minecraftVersions", QJsonArray::fromStringList(version.mcVersion) }, { "date", version.date },
             { "fileName", version.fileName }, { "downloadUrl", version.downloadUrl }, { "hashType", version.hash_type },
             { "hash", version.hash }, { "loaders", flagNames(version.loaders, modLoaders()) },
             { "pluginLoaders", flagNames(version.pluginLoaders, pluginLoaders()) },
             { "changelog", version.changelog }, { "dependencies", dependencies } };
}

template <typename T>
struct Reply {
    T data{};
    QString error;
    bool delivered = false;
    ResourceAPI::Callback<T> callbacks()
    {
        return { [this](T& value) { data = value; delivered = true; },
                 [this](const QString& message, int) { error = message; },
                 [this] { error = QStringLiteral("The task was cancelled."); } };
    }
    bool finish(LauncherApi& api, const Task::Ptr& task, UserInteraction& interaction)
    {
        if (!wait(api, task, interaction, error)) return false;
        if (!delivered && error.isEmpty()) error = QStringLiteral("Provider returned an invalid response.");
        return delivered && error.isEmpty();
    }
};

struct Filters {
    std::optional<std::list<::Version>> versions;
    std::optional<ModPlatform::ModLoaderTypes> loaders;
    std::optional<ModPlatform::PluginLoaderTypes> plugins;
    bool parse(const QJsonObject& p)
    {
        if (p.contains("minecraftVersion")) {
            const auto version = p.value("minecraftVersion").toString();
            if (!identifier(version)) return false;
            versions = std::list<::Version>{ ::Version(version) };
        }
        if (p.contains("loaders")) {
            ModPlatform::ModLoaderTypes flags;
            if (!parseFlags(p.value("loaders"), modLoaders(), flags)) return false;
            if (flags) loaders = flags;
        }
        if (p.contains("pluginLoaders")) {
            ModPlatform::PluginLoaderTypes flags;
            if (!parseFlags(p.value("pluginLoaders"), pluginLoaders(), flags)) return false;
            if (flags) plugins = flags;
        }
        return true;
    }
};

bool projectIdValid(const QString& id, const QString& source)
{
    if (source == "hangar") {
        const auto parts = id.split('/');
        return parts.size() <= 2 && std::all_of(parts.begin(), parts.end(), identifier);
    }
    if (source == "curseforge") {
        bool ok;
        const auto number = id.toLongLong(&ok);
        return ok && number > 0;
    }
    return identifier(id);
}

QJsonObject browse(LauncherApi& api, const QString& operation, const QJsonObject& p, UserInteraction& interaction)
{
    const auto source = p.value("provider").toString();
    auto service = provider(source);
    if (!service) return OperationService::failure("provider must be modrinth, curseforge, or hangar.", 2);
    const auto kind = p.value("kind").toString();
    if (!supportedKinds(source).contains(kind)) return OperationService::failure("This provider does not support the requested kind.", 2);
    Filters filters;
    if (!filters.parse(p)) return OperationService::failure("Invalid Minecraft version or loader filter.", 2);

    if (operation == "resource.search") {
        ResourceAPI::SearchArgs args;
        args.type = kinds.value(kind);
        args.offset = p.value("offset").toInt(0);
        if (args.offset < 0 || args.offset > 100000) return OperationService::failure("offset must be between 0 and 100000.", 2);
        if (p.contains("query")) args.search = QString::fromLatin1(QUrl::toPercentEncoding(p.value("query").toString()));
        args.versions = filters.versions;
        args.loaders = filters.loaders;
        args.pluginLoaders = filters.plugins;
        args.openSource = p.value("openSource").toBool();
        const auto sort = p.value("sort").toString();
        if (!sort.isEmpty()) {
            for (const auto& method : service->getSortingMethods())
                if (method.name == sort) args.sorting = method;
            if (!args.sorting) return OperationService::failure("Unknown sorting method. See resource.providers.", 2);
        }
        Reply<QList<Pack::Ptr>> reply;
        const auto task = service->searchProjects(std::move(args), reply.callbacks());
        if (!reply.finish(api, task, interaction)) return OperationService::failure(reply.error);
        QJsonArray projects;
        for (const auto& pack : reply.data) projects.append(packJson(*pack));
        return OperationService::success(QJsonObject{ { "provider", source }, { "projects", projects },
            { "offset", p.value("offset").toInt() }, { "pageSize", 25 }, { "nextOffset", p.value("offset").toInt() + projects.size() },
            { "mayHaveMore", projects.size() == 25 } });
    }

    const auto projectId = p.value("projectId").toString();
    if (!projectIdValid(projectId, source)) return OperationService::failure("Invalid projectId.", 2);
    auto pack = std::make_shared<Pack>();
    pack->addonId = source == "curseforge" ? QVariant(projectId.toLongLong()) : QVariant(projectId);
    pack->provider = source == "modrinth" ? ModPlatform::ResourceProvider::MODRINTH :
                     source == "curseforge" ? ModPlatform::ResourceProvider::FLAME : ModPlatform::ResourceProvider::HANGAR;
    if (operation == "resource.project" || operation == "resource.install-version") {
        Reply<Pack::Ptr> reply;
        const auto task = service->getProjectInfo({ pack }, reply.callbacks());
        if (!reply.finish(api, task, interaction)) return OperationService::failure(reply.error);
        if (pack->name.isEmpty()) return OperationService::failure("Provider returned an invalid project.");
        if (operation == "resource.project") return OperationService::success(packJson(*pack));
    }
    if (operation == "resource.resolve-dependency") {
        if (source == "hangar") return OperationService::failure("Hangar does not support dependency resolution.", 2);
        if (!filters.versions) return OperationService::failure("minecraftVersion is required for dependency resolution.", 2);
        ResourceAPI::DependencySearchArgs args;
        args.dependency = { pack->addonId, ModPlatform::DependencyType::Required, p.value("versionId").toString() };
        if (!args.dependency.version.isEmpty() && !identifier(args.dependency.version)) return OperationService::failure("Invalid versionId.", 2);
        args.mcVersion = filters.versions->front();
        args.loader = filters.loaders.value_or(ModPlatform::ModLoaderTypes{});
        Reply<PackVersion> reply;
        const auto task = service->getDependencyVersion(std::move(args), reply.callbacks());
        if (!reply.finish(api, task, interaction)) return OperationService::failure(reply.error);
        if (!reply.data.fileId.isValid()) return OperationService::failure("No matching dependency version was found.", 2);
        return OperationService::success(versionJson(reply.data));
    }
    ResourceAPI::VersionSearchArgs args;
    args.pack = pack;
    args.resourceType = kinds.value(kind);
    args.mcVersions = filters.versions;
    args.loaders = filters.loaders;
    args.pluginLoaders = filters.plugins;
    args.includeChangelog = p.value("includeChangelog").toBool();
    Reply<QList<PackVersion>> reply;
    const auto task = service->getProjectVersions(std::move(args), reply.callbacks());
    if (!reply.finish(api, task, interaction)) return OperationService::failure(reply.error);
    QJsonArray versions;
    std::optional<PackVersion> selected;
    for (const auto& version : reply.data) {
        if (filters.versions && !version.mcVersion.contains(p.value("minecraftVersion").toString())) continue;
        if (filters.loaders && *filters.loaders && !(version.loaders & *filters.loaders)) continue;
        if (filters.plugins && *filters.plugins && !(version.pluginLoaders & *filters.plugins)) continue;
        versions.append(versionJson(version));
        if (version.fileId.toString() == p.value("versionId").toString()) selected = version;
    }
    if (operation == "resource.versions") return OperationService::success(versions);
    if (!selected) return OperationService::failure("The selected version was not found or does not match the filters.", 2);
    const auto instance = APPLICATION->instances()->getInstanceById(p.value("instance").toString());
    if (!instance) return OperationService::failure("Instance ID not found.", 2);
    if (instance->isRunning()) return OperationService::failure("Stop the instance before installing resources.", 2);
    std::shared_ptr<ResourceFolderModel> model;
    if (const auto client = dynamic_cast<MinecraftInstance*>(instance)) {
        if (kind == "mods") model = client->loaderModList();
        if (kind == "resourcepacks") model = client->resourcePackList();
        if (kind == "shaderpacks") model = client->shaderPackList();
        if (kind == "datapacks") model = client->dataPackList();
    } else if (const auto server = dynamic_cast<ServerInstance*>(instance)) {
        if (kind == "mods") model = server->loaderModList();
        if (kind == "plugins") model = server->pluginList();
    }
    if (!model) return OperationService::failure("This resource kind cannot be installed into this instance. Use instance.import for modpacks.", 2);
    const auto name = selected->fileName;
    if (name.isEmpty() || name == "." || name == ".." || name.contains('/') || name.contains('\\') || name.contains(':') ||
        name.endsWith('.') || name.endsWith(' ')) return OperationService::failure("Provider returned an unsafe filename.", 2);
    const QUrl url(selected->downloadUrl);
    if (!url.isValid() || (url.scheme() != "https" && url.scheme() != "http")) return OperationService::failure("Invalid download URL.", 2);
    if (QFileInfo::exists(model->dir().absoluteFilePath(name)) && !p.value("replace").toBool())
        return OperationService::failure("File already exists; set replace=true to replace it.", 2);
    auto download = makeShared<ResourceDownloadTask>(pack, *selected, model.get());
    QString error;
    if (!wait(api, download, interaction, error)) return OperationService::failure(error);
    model->update();
    auto result = versionJson(*selected);
    result.insert("instance", instance->id());
    result.insert("path", model->dir().absoluteFilePath(name));
    result.insert("dependenciesInstalled", false);
    return OperationService::success(result);
}

bool loadMetadata(LauncherApi& api, Meta::BaseEntity& entity, bool offline, UserInteraction& interaction, QString& error)
{
    if (!offline) return wait(api, entity.loadTask(Net::Mode::Online), interaction, error);
    // Upstream's Offline task may fetch missing files. The API's offline flag
    // explicitly promises no network access, including when a cache is missing.
    QFile file(QDir("meta").filePath(entity.localFilename()));
    if (!file.open(QIODevice::ReadOnly)) { error = "Offline metadata file not found: " + entity.localFilename(); return false; }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) { error = "Invalid offline metadata JSON."; return false; }
    try {
        entity.parse(document.object());
        entity.setLoadStatus(Meta::BaseEntity::LoadStatus::Local);
    } catch (const Exception& exception) {
        error = exception.cause();
        return false;
    }
    return true;
}

QJsonObject metadata(LauncherApi& api, const QJsonObject& p, UserInteraction& interaction, bool catalog)
{
    auto index = APPLICATION->metadataIndex();
    QString error;
    const auto offline = p.value("offline").toBool();
    if (catalog) {
        if (!loadMetadata(api, *index, offline, interaction, error)) return OperationService::failure(error);
        QJsonArray entries;
        for (const auto& list : index->lists()) entries.append(QJsonObject{ { "uid", list->uid() }, { "name", list->name() } });
        return OperationService::success(entries);
    }
    const auto uid = p.value("uid").toString();
    if (!identifier(uid)) return OperationService::failure("Invalid component UID.", 2);
    const auto parentVersion = p.value("minecraftVersion").toString();
    if (!parentVersion.isEmpty() && !identifier(parentVersion)) return OperationService::failure("Invalid Minecraft version.", 2);
    auto list = index->get(uid);
    if (!loadMetadata(api, *list, offline, interaction, error)) return OperationService::failure(error);
    QJsonArray versions;
    for (const auto& version : list->versions()) {
        QJsonArray requirements;
        bool matches = true;
        for (const auto& requirement : version->requiredSet()) {
            requirements.append(QJsonObject{ { "uid", requirement.uid }, { "equals", requirement.equalsVersion }, { "suggests", requirement.suggests } });
            if (!parentVersion.isEmpty() && requirement.uid == "net.minecraft" && !requirement.equalsVersion.isEmpty() &&
                requirement.equalsVersion != parentVersion) matches = false;
        }
        if (matches) versions.append(QJsonObject{ { "version", version->version() }, { "type", version->type() },
            { "time", version->time().toString(Qt::ISODate) }, { "recommended", version->isRecommended() }, { "requires", requirements } });
    }
    return OperationService::success(QJsonObject{ { "uid", uid }, { "name", list->name() }, { "versions", versions } });
}
}  // namespace

void registerLauncherApiCatalogOperations(LauncherApi& api)
{
    using namespace ApiSupport;
    api.registerOperation({ "resource.providers", "List remote resource providers, resource kinds and sorting methods.", schema({}), "catalog" },
        [](const QJsonObject&, UserInteraction&) {
            QJsonArray result;
            for (const auto& name : { QString("modrinth"), QString("curseforge"), QString("hangar") }) {
                const auto service = provider(name);
                QJsonArray sorts;
                for (const auto& sort : service->getSortingMethods()) sorts.append(QJsonObject{ { "id", sort.name }, { "name", sort.readable_name } });
                result.append(QJsonObject{ { "id", name }, { "kinds", QJsonArray::fromStringList(supportedKinds(name)) }, { "sorts", sorts },
                    { "dependencyResolution", name != "hangar" }, { "minecraftSearchFilter", name != "hangar" } });
            }
            return OperationService::success(result);
        });
    const QJsonObject common{ { "provider", string("modrinth, curseforge, or hangar.") }, { "kind", string("Resource kind from resource.providers.") },
        { "minecraftVersion", string("Optional Minecraft version filter.") }, { "loaders", strings() }, { "pluginLoaders", strings() } };
    auto search = common;
    search.insert("query", string("Search text."));
    search.insert("offset", QJsonObject{ { "type", "integer" }, { "minimum", 0 }, { "maximum", 100000 } });
    search.insert("sort", string("Sorting ID from resource.providers."));
    search.insert("openSource", boolean());
    api.registerOperation({ "resource.search", "Search remote resources; results use provider pages of 25 entries.", schema(search, { "provider", "kind" }), "catalog" },
        [&api](const QJsonObject& p, UserInteraction& i) { return browse(api, "resource.search", p, i); });
    auto project = common;
    project.insert("projectId", string("Provider project ID (Hangar accepts owner/project)."));
    project.insert("includeChangelog", boolean());
    for (const auto& name : { QString("resource.project"), QString("resource.versions") })
        api.registerOperation({ name, name == "resource.project" ? "Read a remote project." : "List matching versions with dependencies and download metadata.",
            schema(project, { "provider", "kind", "projectId" }), "catalog" },
            [&api, name](const QJsonObject& p, UserInteraction& i) { return browse(api, name, p, i); });
    project.insert("versionId", string("Provider version ID."));
    api.registerOperation({ "resource.resolve-dependency", "Resolve one dependency against a Minecraft version and loader filter.",
        schema(project, { "provider", "kind", "projectId", "minecraftVersion" }), "catalog" },
        [&api](const QJsonObject& p, UserInteraction& i) { return browse(api, "resource.resolve-dependency", p, i); });
    project.insert("instance", string("Installed instance ID."));
    project.insert("replace", boolean());
    api.registerOperation({ "resource.install-version", "Install one selected provider version using the existing indexed resource download task. Dependencies are selected separately.",
        schema(project, { "provider", "kind", "projectId", "versionId", "instance" }), "catalog", true },
        [&api](const QJsonObject& p, UserInteraction& i) { return browse(api, "resource.install-version", p, i); });
    api.registerOperation({ "component.catalog", "List Minecraft metadata component identifiers.", schema({ { "offline", boolean() } }), "components" },
        [&api](const QJsonObject& p, UserInteraction& i) { return metadata(api, p, i, true); });
    api.registerOperation({ "component.versions", "List Minecraft or loader versions and requirements for version selection.",
        schema({ { "uid", string("Component UID, such as net.minecraft or net.fabricmc.fabric-loader.") }, { "minecraftVersion", string("Optional Minecraft version filter.") },
            { "offline", boolean() } }, { "uid" }), "components" },
        [&api](const QJsonObject& p, UserInteraction& i) { return metadata(api, p, i, false); });
}
