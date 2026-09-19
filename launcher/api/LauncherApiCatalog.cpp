// SPDX-License-Identifier: GPL-3.0-only
#include "LauncherApiCatalog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QUuid>
#include <QUrl>

#include "Application.h"
#include "InstanceList.h"
#include "ResourceDownloadTask.h"
#include "api/LauncherApiSupport.h"
#include "cli/OperationService.h"
#include "meta/Index.h"
#include "meta/Version.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "minecraft/mod/MetadataHandler.h"
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

std::shared_ptr<ResourceFolderModel> resourceModel(BaseInstance* instance, const QString& kind)
{
    if (const auto client = dynamic_cast<MinecraftInstance*>(instance)) {
        if (kind == "mods") return client->loaderModList();
        if (kind == "resourcepacks") return client->resourcePackList();
        if (kind == "shaderpacks") return client->shaderPackList();
        if (kind == "datapacks") return client->dataPackList();
    } else if (const auto server = dynamic_cast<ServerInstance*>(instance)) {
        if (kind == "mods") return server->loaderModList();
        if (kind == "plugins") return server->pluginList();
    }
    return {};
}

bool safeFilename(const QString& name)
{
    static const QRegularExpression forbidden(R"([<>:"/\\|?*\x00-\x1f])");
    static const QRegularExpression reserved(R"(^(CON|PRN|AUX|NUL|COM[0-9]|LPT[0-9])($|\.))", QRegularExpression::CaseInsensitiveOption);
    return !name.isEmpty() && name != "." && name != ".." && !name.endsWith('.') && !name.endsWith(' ') &&
           !forbidden.match(name).hasMatch() && !reserved.match(name).hasMatch();
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
    const auto model = resourceModel(instance, kind);
    if (!model) return OperationService::failure("This resource kind cannot be installed into this instance. Use instance.import for modpacks.", 2);
    const auto name = selected->fileName;
    if (!safeFilename(name)) return OperationService::failure("Provider returned an unsafe filename.", 2);
    const QUrl url(selected->downloadUrl);
    if (!url.isValid() || (url.scheme() != "https" && url.scheme() != "http")) return OperationService::failure("Invalid download URL.", 2);
    if (QFileInfo::exists(model->dir().absoluteFilePath(name)) && !p.value("replace").toBool())
        return OperationService::failure("File already exists; set replace=true to replace it.", 2);
    auto download = makeShared<ResourceDownloadTask>(pack, *selected, model.get());
    QString error;
    if (!wait(api, download, interaction, error)) return OperationService::failure(error);
    // ResourceFolderModel watches this directory; avoid starting a second
    // asynchronous refresh while a headless sidecar may be shutting down.
    auto result = versionJson(*selected);
    result.insert("instance", instance->id());
    result.insert("path", model->dir().absoluteFilePath(name));
    result.insert("dependenciesInstalled", false);
    return OperationService::success(result);
}

// Plans contain immutable provider results. Callers select item IDs, never URLs.
struct UpdateItem {
    QString id, oldName, indexName;
    QByteArray fileHash, indexHash;
    Pack::Ptr pack;
    PackVersion version;
    bool disabled = false;
    QString destination() const { return version.fileName + (disabled ? ".disabled" : ""); }
};
struct UpdatePlan {
    QString id, instance, kind, root, indexRoot;
    QDateTime expires;
    QList<UpdateItem> items;
};
struct UpdatePlans {
    QHash<QString, UpdatePlan> plans;
    void prune()
    {
        const auto now = QDateTime::currentDateTimeUtc();
        for (auto it = plans.begin(); it != plans.end();) {
            if (it->expires <= now) it = plans.erase(it);
            else ++it;
        }
    }
};

QByteArray fingerprint(LauncherApi& api, const QString& path)
{
    const QFileInfo info(path);
    QFile file(path);
    if (!info.isFile() || info.isSymLink() || !file.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        const auto bytes = file.read(1024 * 1024);
        if (bytes.isEmpty() && file.error() != QFileDevice::NoError) return {};
        hash.addData(bytes);
        QCoreApplication::processEvents();
        if (api.isCancellationRequested()) return {};
    }
    return hash.result();
}

QJsonObject checkUpdates(LauncherApi& api, UpdatePlans& state, QJsonObject p, UserInteraction& interaction)
{
    state.prune();
    if (state.plans.size() >= 16) return OperationService::failure("Discard an existing update plan before creating another (limit 16).", 2);
    const auto instance = APPLICATION->instances()->getInstanceById(p.value("instance").toString());
    if (!instance) return OperationService::failure("Instance ID not found.", 2);
    const auto kind = p.value("kind").toString();
    const auto model = resourceModel(instance, kind);
    if (!model) return OperationService::failure("This resource kind is unavailable for the instance.", 2);
    if (const auto client = dynamic_cast<MinecraftInstance*>(instance)) {
        const auto profile = client->getPackProfile();
        if (!p.contains("minecraftVersion")) p.insert("minecraftVersion", profile->getComponentVersion("net.minecraft"));
        if (kind == "mods" && !p.contains("loaders")) p.insert("loaders", flagNames(profile->getModLoaders().value_or(ModPlatform::ModLoaderTypes{}), modLoaders()));
    } else if (const auto server = dynamic_cast<ServerInstance*>(instance)) {
        if (!p.contains("minecraftVersion")) p.insert("minecraftVersion", server->getMinecraftVersion());
        if (kind == "mods" && !p.contains("loaders")) p.insert("loaders", flagNames(server->getModLoaderTypes(), modLoaders()));
        if (kind == "plugins" && !p.contains("pluginLoaders")) p.insert("pluginLoaders", flagNames(server->getPluginLoaderTypes(), pluginLoaders()));
    }
    Filters filters;
    if (!filters.parse(p)) return OperationService::failure("Set a valid Minecraft version and loader filters before checking updates.", 2);
    QStringList releases{ "release" };
    if (p.contains("releaseTypes")) {
        releases.clear();
        for (const auto& value : p.value("releaseTypes").toArray()) {
            const auto type = value.toString();
            if (type != "release" && type != "beta" && type != "alpha") return OperationService::failure("Unknown release type.", 2);
            releases.append(type);
        }
        if (releases.isEmpty()) return OperationService::failure("Select at least one release type.", 2);
    }
    const auto entries = model->indexDir().entryInfoList({ "*.pw.toml" }, QDir::Files, QDir::Name);
    if (entries.size() > 256) return OperationService::failure("At most 256 indexed resources can be checked in one plan.", 2);
    UpdatePlan plan;
    plan.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    plan.instance = instance->id();
    plan.kind = kind;
    plan.root = QFileInfo(model->dir().absolutePath()).canonicalFilePath();
    plan.indexRoot = QFileInfo(model->indexDir().absolutePath()).canonicalFilePath();
    if (!entries.isEmpty() && plan.indexRoot != plan.root + "/.index")
        return OperationService::failure("The resource index must be inside the resource directory.", 2);
    QJsonArray updates, skipped;
    QSet<QString> indexedFiles, projects;
    for (const auto& entry : entries) {
        QCoreApplication::processEvents();
        if (api.isCancellationRequested()) return OperationService::failure("Update check cancelled; no plan was created.");
        const auto skip = [&](const QString& reason) { skipped.append(QJsonObject{ { "index", entry.fileName() }, { "reason", reason } }); };
        const auto meta = Metadata::get(model->indexDir(), entry.fileName());
        if (!meta.isValid() || !safeFilename(meta.filename) || !safeFilename(meta.slug) || entry.isSymLink()) { skip("invalid-metadata"); continue; }
        const auto source = meta.provider == ModPlatform::ResourceProvider::MODRINTH ? QString("modrinth") :
                            meta.provider == ModPlatform::ResourceProvider::FLAME ? QString("curseforge") : QString();
        if (source.isEmpty() || !supportedKinds(source).contains(kind)) { skip("unsupported-provider-or-kind"); continue; }
        if (!projectIdValid(meta.project_id.toString(), source)) { skip("invalid-project-id"); continue; }
        const auto key = source + ':' + meta.project_id.toString();
        if (projects.contains(key)) return OperationService::failure("Duplicate project metadata; repair the resource index before checking updates.", 2);
        projects.insert(key);
        UpdateItem item;
        item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        item.oldName = meta.filename;
        if (QFileInfo::exists(model->dir().filePath(item.oldName)) && QFileInfo::exists(model->dir().filePath(item.oldName + ".disabled"))) {
            skip("both-enabled-and-disabled-files-exist"); continue;
        }
        if (!QFileInfo::exists(model->dir().filePath(item.oldName))) item.oldName += ".disabled";
        item.disabled = item.oldName.endsWith(".disabled");
        indexedFiles.insert(item.oldName);
        item.indexName = entry.fileName();
        item.fileHash = fingerprint(api, model->dir().filePath(item.oldName));
        item.indexHash = fingerprint(api, entry.absoluteFilePath());
        if (item.fileHash.isEmpty() || item.indexHash.isEmpty()) { skip("missing-or-unreadable-file"); continue; }
        item.pack = std::make_shared<Pack>();
        item.pack->addonId = meta.project_id;
        item.pack->provider = meta.provider;
        item.pack->name = meta.name;
        item.pack->slug = meta.slug;
        item.pack->side = meta.side;
        auto service = provider(source);
        ResourceAPI::VersionSearchArgs args;
        args.pack = item.pack;
        args.resourceType = kinds.value(kind);
        args.mcVersions = filters.versions;
        args.loaders = filters.loaders;
        args.pluginLoaders = filters.plugins;
        Reply<QList<PackVersion>> reply;
        interaction.status(QString("Checking resource updates: %1").arg(meta.name));
        if (!reply.finish(api, service->getProjectVersions(std::move(args), reply.callbacks()), interaction)) { skip(reply.error); continue; }
        QDateTime currentDate;
        for (const auto& version : reply.data)
            if (version.fileId.toString() == meta.file_id.toString()) currentDate = QDateTime::fromString(version.date, Qt::ISODate);
        if (!currentDate.isValid()) { skip("current-version-not-in-results"); continue; }
        QDateTime newest = currentDate;
        for (const auto& version : reply.data) {
            const auto date = QDateTime::fromString(version.date, Qt::ISODate);
            if (!date.isValid() || date <= newest || !releases.contains(version.version_type.toString())) continue;
            if (!version.mcVersion.contains(p.value("minecraftVersion").toString())) continue;
            if (filters.loaders && !(version.loaders & *filters.loaders)) continue;
            if (filters.plugins && !(version.pluginLoaders & *filters.plugins)) continue;
            const QUrl url(version.downloadUrl);
            if (!safeFilename(version.fileName) || version.fileName.endsWith(".disabled") ||
                !url.isValid() || (url.scheme() != "https" && url.scheme() != "http")) continue;
            newest = date;
            item.version = version;
        }
        if (newest == currentDate) { skip("up-to-date"); continue; }
        plan.items.append(item);
        updates.append(QJsonObject{ { "itemId", item.id }, { "provider", source }, { "projectId", meta.project_id.toString() },
            { "name", meta.name }, { "fileName", item.oldName }, { "enabled", !item.disabled }, { "currentVersionId", meta.file_id.toString() },
            { "currentVersion", meta.version_number }, { "target", versionJson(item.version) } });
    }
    if (api.isCancellationRequested()) return OperationService::failure("Update check cancelled; no plan was created.");
    for (const auto& file : model->dir().entryInfoList(QDir::Files, QDir::Name))
        if (!indexedFiles.contains(file.fileName())) skipped.append(QJsonObject{ { "fileName", file.fileName() }, { "reason", "no-supported-metadata" } });
    plan.expires = QDateTime::currentDateTimeUtc().addSecs(600);
    state.plans.insert(plan.id, plan);
    return OperationService::success(QJsonObject{ { "planId", plan.id }, { "instance", plan.instance }, { "kind", kind },
        { "expiresAt", plan.expires.toString(Qt::ISODate) }, { "filters", p }, { "updates", updates }, { "skipped", skipped },
        { "dependenciesInstalled", false } });
}

QString validateUpdate(LauncherApi& api, const UpdatePlan& plan, const UpdateItem& item, ResourceFolderModel& model)
{
    const auto instance = APPLICATION->instances()->getInstanceById(plan.instance);
    if (!instance || instance->isRunning()) return "The instance is missing or running.";
    if (QFileInfo(model.dir().absolutePath()).canonicalFilePath() != plan.root ||
        QFileInfo(model.indexDir().absolutePath()).canonicalFilePath() != plan.indexRoot) return "Resource directories changed since the update check.";
    if (fingerprint(api, model.dir().filePath(item.oldName)) != item.fileHash ||
        fingerprint(api, model.indexDir().filePath(item.indexName)) != item.indexHash) return "Resource or metadata changed since the update check.";
    // Check enabled and disabled names so an update cannot accidentally load both.
    for (const auto& name : { item.version.fileName, item.version.fileName + ".disabled" })
        if (name != item.oldName && QFileInfo::exists(model.dir().filePath(name))) return "An unrelated resource occupies the destination filename.";
    return {};
}

QJsonObject applyUpdates(LauncherApi& api, UpdatePlans& state, const QJsonObject& p, UserInteraction& interaction)
{
    state.prune();
    const auto id = p.value("planId").toString();
    if (!state.plans.contains(id)) return OperationService::failure("Update plan not found or expired; check again.", 2);
    const auto plan = state.plans.value(id);
    QSet<QString> selection;
    for (const auto& value : p.value("items").toArray()) {
        if (selection.contains(value.toString())) return OperationService::failure("Duplicate update item ID.", 2);
        selection.insert(value.toString());
    }
    if (selection.isEmpty() || selection.size() > plan.items.size()) return OperationService::failure("Select one or more items from the plan.", 2);
    QList<UpdateItem> items;
    QSet<QString> destinations;
    for (const auto& item : plan.items) {
        if (!selection.remove(item.id)) continue;
        const auto destination = item.version.fileName.toCaseFolded();
        if (destinations.contains(destination)) return OperationService::failure("Selected updates have conflicting filenames.", 2);
        destinations.insert(destination);
        items.append(item);
    }
    if (!selection.isEmpty()) return OperationService::failure("Unknown update item ID.", 2);
    const auto instance = APPLICATION->instances()->getInstanceById(plan.instance);
    const auto model = resourceModel(instance, plan.kind);
    if (!model) return OperationService::failure("The instance or resource directory is unavailable.", 2);
    for (const auto& item : items) {
        const auto error = validateUpdate(api, plan, item, *model);
        if (!error.isEmpty()) return OperationService::failure(error, 2);
    }
    if (api.isCancellationRequested()) return OperationService::failure("Update cancelled before execution.");
    state.plans.remove(id); // Single use once execution begins; preflight failures leave it intact.
    QJsonArray results;
    bool complete = true;
    for (const auto& item : items) {
        QJsonObject result{ { "itemId", item.id }, { "versionId", item.version.fileId.toString() } };
        QString error;
        bool installed = false;
        if (!api.isCancellationRequested()) {
            QTemporaryDir staging(QDir(plan.root).filePath(".api-update-XXXXXX"));
            if (!staging.isValid()) error = "Could not create a resource staging directory.";
            else {
                ResourceFolderModel staged(QDir(staging.path()), instance, true, true);
                const auto download = makeShared<ResourceDownloadTask>(item.pack, item.version, &staged);
                if (wait(api, download, interaction, error)) {
                    error = validateUpdate(api, plan, item, *model);
                    const auto indexes = staged.indexDir().entryList({ "*.pw.toml" }, QDir::Files);
                    QFile index(indexes.size() == 1 ? staged.indexDir().filePath(indexes.first()) : QString());
                    if (error.isEmpty() && !index.open(QIODevice::ReadOnly)) error = "Downloaded resource index is missing.";
                    if (error.isEmpty() && !api.isCancellationRequested()) {
                        const auto content = index.readAll();
                        const auto oldPath = model->dir().filePath(item.oldName);
                        const auto destination = model->dir().filePath(item.destination());
                        const auto backup = QDir(staging.path()).filePath("original.backup");
                        QSaveFile targetIndex(model->indexDir().filePath(item.indexName));
                        if (content.isEmpty() || !targetIndex.open(QIODevice::WriteOnly) || targetIndex.write(content) != content.size())
                            error = "Could not prepare the updated resource index.";
                        else if (!QFile::rename(oldPath, backup)) error = "Could not move the original resource to its backup.";
                        else {
                            const auto moved = QFile::rename(staged.dir().filePath(item.version.fileName), destination);
                            installed = moved && targetIndex.commit();
                            if (!installed) {
                                error = "Could not install the update; restoring the original resource.";
                                const bool removed = !moved || QFile::remove(destination);
                                if (!removed || !QFile::rename(backup, oldPath)) {
                                    staging.setAutoRemove(false);
                                    result.insert("recoveryDirectory", staging.path());
                                    error += " Restore failed; the original is preserved in recoveryDirectory/original.backup.";
                                }
                            }
                        }
                    }
                }
            }
        }
        const auto status = installed ? "updated" : api.isCancellationRequested() ? "cancelled" : "failed";
        result.insert("status", status);
        result.insert("enabled", !item.disabled);
        if (!error.isEmpty()) result.insert("error", error);
        if (installed) {
            result.insert("path", model->dir().filePath(item.destination()));
            if (plan.kind == "shaderpacks") {
                const auto oldConfig = model->dir().filePath((item.disabled ? item.oldName.chopped(9) : item.oldName) + ".txt");
                const auto newConfig = model->dir().filePath(item.version.fileName + ".txt");
                if (oldConfig != newConfig && QFileInfo::exists(oldConfig) && !QFileInfo::exists(newConfig) && !QFile::rename(oldConfig, newConfig))
                    result.insert("warning", "The shader configuration could not be renamed.");
            }
        } else complete = false;
        results.append(result);
    }
    // The directory watcher will refresh the model after the atomic replacement.
    return OperationService::success(QJsonObject{ { "planId", id }, { "complete", complete }, { "results", results },
        { "cancelled", api.isCancellationRequested() }, { "dependenciesInstalled", false } });
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
    const auto plans = std::make_shared<UpdatePlans>();
    api.registerOperation({ "resource.updates.check", "Check indexed resources and create a session update plan valid for ten minutes. Defaults to release builds and instance compatibility filters.",
        schema({ { "instance", string("Installed instance ID.") }, { "kind", string("mods, plugins, resourcepacks, shaderpacks, or datapacks.") },
            { "minecraftVersion", string("Override the instance Minecraft version filter.") }, { "loaders", strings() },
            { "pluginLoaders", strings() }, { "releaseTypes", strings() } }, { "instance", "kind" }), "catalog" },
        [&api, plans](const QJsonObject& p, UserInteraction& i) { return checkUpdates(api, *plans, p, i); });
    api.registerOperation({ "resource.updates.apply", "Apply selected plan items after stale-file checks, preserving disabled state. Inspect complete and per-item results; this is not a batch transaction.",
        schema({ { "planId", string("Session update plan ID.") }, { "items", strings() } }, { "planId", "items" }), "catalog", true },
        [&api, plans](const QJsonObject& p, UserInteraction& i) { return applyUpdates(api, *plans, p, i); });
    api.registerOperation({ "resource.updates.discard", "Discard a session update plan.", schema({ { "planId", string("Session update plan ID.") } }, { "planId" }), "catalog" },
        [plans](const QJsonObject& p, UserInteraction&) {
            plans->prune();
            return OperationService::success(QJsonObject{ { "removed", plans->plans.remove(p.value("planId").toString()) != 0 } });
        });
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
    auto recursiveProject = project;
    recursiveProject.insert("maxDepth", QJsonObject{ { "type", "integer" }, { "minimum", 0 }, { "maximum", 32 } });
    recursiveProject.insert("maxItems", QJsonObject{ { "type", "integer" }, { "minimum", 1 }, { "maximum", 256 } });
    recursiveProject.insert("includeOptional", boolean());
    api.registerOperation({ "resource.install-with-dependencies", "Install a selected resource and recursively install required dependencies through the same provider APIs.",
        schema(recursiveProject, { "provider", "kind", "projectId", "versionId", "instance" }), "catalog", true },
        [&api](const QJsonObject& root, UserInteraction& interaction) {
            const int maxDepth = qBound(0, root.value("maxDepth").toInt(8), 32);
            const int maxItems = qBound(1, root.value("maxItems").toInt(64), 256);
            const bool includeOptional = root.value("includeOptional").toBool(false);
            QSet<QString> visited;
            QJsonArray results;
            auto operationParameters = [](const QJsonObject& source) {
                QJsonObject value;
                for (const auto& key : { "provider", "kind", "projectId", "versionId", "instance", "minecraftVersion", "loaders", "pluginLoaders", "replace", "includeChangelog" })
                    if (source.contains(key)) value.insert(key, source.value(key));
                return value;
            };
            std::function<bool(QJsonObject, int)> install = [&](QJsonObject parameters, int depth) {
                if (depth > maxDepth || results.size() >= maxItems) return false;
                const auto key = parameters.value("provider").toString() + ':' + parameters.value("projectId").toString();
                if (visited.contains(key)) return true;
                visited.insert(key);
                const auto response = api.execute("resource.install-version", operationParameters(parameters), interaction);
                const auto data = response.value("data").toObject();
                results.append(QJsonObject{ { "projectId", parameters.value("projectId") }, { "provider", parameters.value("provider") },
                                            { "depth", depth }, { "ok", response.value("ok") }, { "data", data },
                                            { "error", response.value("error") } });
                if (!response.value("ok").toBool()) return false;
                for (const auto& dependencyValue : data.value("dependencies").toArray()) {
                    const auto dependency = dependencyValue.toObject();
                    const auto type = dependency.value("type").toString().toLower();
                    if (type != "required" && !(includeOptional && type == "optional")) continue;
                    if (results.size() >= maxItems) return false;
                    QJsonObject child = operationParameters(root);
                    child.insert("projectId", dependency.value("projectId"));
                    child.insert("versionId", dependency.value("versionId"));
                    if (child.value("versionId").toString().isEmpty()) {
                        const auto resolved = api.execute("resource.resolve-dependency", child, interaction);
                        if (!resolved.value("ok").toBool()) {
                            results.append(QJsonObject{ { "projectId", child.value("projectId") }, { "depth", depth + 1 },
                                { "ok", false }, { "error", resolved.value("error") } });
                            return false;
                        }
                        child.insert("versionId", resolved.value("data").toObject().value("versionId"));
                    }
                    if (!install(child, depth + 1)) return false;
                }
                return true;
            };
            const bool complete = install(root, 0) && !api.isCancellationRequested();
            return OperationService::success(QJsonObject{ { "complete", complete }, { "cancelled", api.isCancellationRequested() },
                { "items", results }, { "maxDepth", maxDepth }, { "maxItems", maxItems }, { "includeOptional", includeOptional } });
        });
    api.registerOperation({ "component.catalog", "List Minecraft metadata component identifiers.", schema({ { "offline", boolean() } }), "components" },
        [&api](const QJsonObject& p, UserInteraction& i) { return metadata(api, p, i, true); });
    api.registerOperation({ "component.versions", "List Minecraft or loader versions and requirements for version selection.",
        schema({ { "uid", string("Component UID, such as net.minecraft or net.fabricmc.fabric-loader.") }, { "minecraftVersion", string("Optional Minecraft version filter.") },
            { "offline", boolean() } }, { "uid" }), "components" },
        [&api](const QJsonObject& p, UserInteraction& i) { return metadata(api, p, i, false); });
}
