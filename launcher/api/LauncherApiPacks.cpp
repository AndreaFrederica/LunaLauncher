// SPDX-License-Identifier: GPL-3.0-only
#include "LauncherApiPacks.h"
#include "api/LauncherApiSupport.h"
#include "Application.h"
#include "BuildConfig.h"
#include "InstanceList.h"
#include "InstanceTask.h"
#include "InstanceImportTask.h"
#include "cli/HeadlessBlockedMods.h"
#include "Json.h"
#include "cli/OperationService.h"
#include "meta/Version.h"
#include "meta/VersionList.h"
#include "modplatform/atlauncher/ATLPackIndex.h"
#include "modplatform/atlauncher/ATLPackInstallTask.h"
#include "modplatform/ftb/FTBPackInstallTask.h"
#include "modplatform/technic/SingleZipPackInstallTask.h"
#include "modplatform/technic/SolderPackInstallTask.h"
#include "modplatform/legacy_ftb/PackFetchTask.h"
#include "modplatform/legacy_ftb/PackInstallTask.h"
#include "modplatform/legacy_ftb/PrivatePackManager.h"
#include "modplatform/import_ftb/PackHelpers.h"
#include "modplatform/import_ftb/PackInstallTask.h"
#include "net/ApiDownload.h"
#include "settings/SettingsObject.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSet>
#include <QUrlQuery>

namespace {
using namespace ApiSupport;
struct Fetch {
    LauncherApi& api;
    UserInteraction& ui;
    bool offline;
    QString error;
    QByteArray bytes(const QString& address)
    {
        const QUrl url(address);
        if (!url.isValid() || url.host().isEmpty() || (url.scheme() != "https" && url.scheme() != "http")) {
            error = "Provider returned an invalid URL."; return {};
        }
        const auto key = QString::fromLatin1(QCryptographicHash::hash(url.toEncoded(), QCryptographicHash::Sha256).toHex());
        const auto cache = QDir(APPLICATION->dataRoot()).filePath("cache/api-packs/" + key);
        if (offline) {
            QFile file(cache);
            if (!file.open(QIODevice::ReadOnly)) { error = "No cached provider response is available."; return {}; }
            return file.readAll();
        }
        auto job = makeShared<NetJob>("Pack catalog", APPLICATION->network());
        auto [request, response] = Net::ApiDownload::makeByteArray(url);
        job->addNetAction(request);
        if (!wait(api, job, ui, error)) return {};
        QDir().mkpath(QFileInfo(cache).absolutePath());
        QSaveFile file(cache);
        if (file.open(QIODevice::WriteOnly) && file.write(*response) == response->size()) file.commit();
        return *response;
    }
    QJsonDocument json(const QString& url)
    {
        const auto data = bytes(url);
        if (!error.isEmpty()) return {};
        QJsonParseError parse;
        const auto result = QJsonDocument::fromJson(data, &parse);
        if (parse.error != QJsonParseError::NoError) error = "Invalid provider response: " + parse.errorString();
        return result;
    }
};

class AtlInteraction final : public ATLauncher::UserInteractionSupport {
    LauncherApi& api;
    UserInteraction& ui;
    QJsonObject options;
public:
    AtlInteraction(LauncherApi& a, UserInteraction& i, QJsonObject p) : api(a), ui(i), options(std::move(p)) {}
    std::optional<QList<QString>> chooseOptionalMods(const ATLauncher::PackVersion& version, QList<ATLauncher::VersionMod> mods) override
    {
        QSet<QString> selected;
        const auto supplied = options.value("optionalMods").toArray();
        QMap<QString, ATLauncher::VersionMod> known;
        for (const auto& mod : mods) known.insert(mod.name, mod);
        if (options.contains("optionalMods")) {
            for (const auto& value : supplied) {
                if (!known.contains(value.toString())) { ui.status("Unknown optional mod: " + value.toString()); return {}; }
                selected.insert(value.toString());
            }
        } else {
            for (const auto& mod : mods) {
                if (mod.effectively_hidden || mod.hidden) continue;
                const auto choice = ui.select(mod.name + "\n" + mod.description, QJsonArray{ "Skip", "Install", "Cancel" });
                if (!choice || *choice == 2) return {};
                if (*choice == 1) selected.insert(mod.name);
            }
        }
        // Close dependencies and reject mutually exclusive groups rather than silently changing a supplied selection.
        bool changed = true;
        while (changed) {
            changed = false;
            for (const auto& name : selected.values()) for (const auto& dependency : known.value(name).depends) {
                if (!known.contains(dependency)) { ui.status("Unknown optional dependency: " + dependency); return {}; }
                if (!selected.contains(dependency)) { selected.insert(dependency); changed = true; }
            }
        }
        QSet<QString> groups;
        for (const auto& name : selected) {
            const auto mod = known.value(name);
            if (!mod.group.isEmpty()) {
                if (groups.contains(mod.group)) { ui.status("Conflicting optional mods in group: " + mod.group); return {}; }
                groups.insert(mod.group);
            }
            if (!mod.warning.isEmpty() && version.warnings.contains(mod.warning)) {
                const auto answer = ui.select(version.warnings.value(mod.warning), QJsonArray{ "Cancel", "Continue" });
                if (!answer || *answer != 1) return {};
            }
        }
        return selected.values();
    }
    QString chooseVersion(Meta::VersionList::Ptr list, QString minecraft) override
    {
        if (const auto task = list->loadTask()) { QString error; if (!wait(api, task, ui, error)) { ui.status(error); return {}; } }
        QJsonArray choices;
        QStringList ids;
        for (const auto& version : list->versions()) {
            bool matches = minecraft.isEmpty();
            for (const auto& req : version->requiredSet()) if (req.uid == "net.minecraft" && req.equalsVersion == minecraft) matches = true;
            if (matches) { choices.append(version->descriptor()); ids.append(version->descriptor()); }
        }
        const auto choice = ui.select("Choose component version", choices);
        return choice && *choice >= 0 && *choice < ids.size() ? ids[*choice] : QString();
    }
    void displayMessage(QString message) override { ui.status(message); }
};

QJsonObject install(LauncherApi& api, InstanceTask* raw, const QJsonObject& p, const QString& defaultName, UserInteraction& ui)
{
    raw->setName(p.value("name").toString(defaultName));
    raw->setGroup(p.value("group").toString()); raw->setIcon(p.value("icon").toString("default"));
    QSet<QString> before;
    const auto instances = APPLICATION->instances();
    for (int i = 0; i < instances->count(); ++i) before.insert(instances->at(i)->id());
    Task::Ptr task(instances->wrapInstanceTask(raw));
    QString error;
    if (!wait(api, task, ui, error)) return OperationService::failure(error);
    instances->saveNow();
    QJsonArray added;
    for (int i = 0; i < instances->count(); ++i) if (!before.contains(instances->at(i)->id())) added.append(instances->at(i)->id());
    return OperationService::success(QJsonObject{ { "installed", true }, { "instances", added } });
}

QJsonObject packOperation(LauncherApi& api, const QString& action, const QJsonObject& p, UserInteraction& ui)
{
    const auto source = p.value("provider").toString(), id = p.value("projectId").toString(), version = p.value("versionId").toString();
    const auto query = p.value("query").toString();
    const bool search = action == "search", installing = action == "install";
    if (installing && p.value("offline").toBool()) return OperationService::failure("Offline mode is for cached catalog queries; pack installation requires network access.", 2);
    if (!search && !identifier(id)) return OperationService::failure("Invalid pack ID.", 2);
    if (installing && (version.isEmpty() || version.contains('/') || version.contains('\\') || version.contains("..")))
        return OperationService::failure("A valid versionId is required.", 2);
    if (source == "modrinth" || source == "curseforge" || source.startsWith("js:")) {
        if (p.value("offline").toBool()) return OperationService::failure("This provider does not expose a cached offline catalog.", 2);
        QJsonObject request{ { "provider", source }, { "kind", "modpacks" } };
        if (search) request.insert("query", query); else request.insert("projectId", id);
        if (!installing) return api.execute("resource." + (search ? QString("search") : action == "versions" ? QString("versions") : QString("project")), request, ui);
        const auto versions = api.execute("resource.versions", request, ui);
        if (!versions.value("ok").toBool()) return versions;
        for (const auto& value : versions.value("data").toArray()) {
            const auto entry = value.toObject();
            if (entry.value("versionId").toString() == version) {
                auto download = entry.value("downloadUrl").toString();
                if (download.isEmpty()) {
                    if (source != "curseforge") return OperationService::failure("Provider did not supply a downloadable pack.", 2);
                    const auto project = api.execute("resource.project", request, ui);
                    if (!project.value("ok").toBool()) return project;
                    QList<BlockedMod> blocked{ { entry.value("fileName").toString(), project.value("data").toObject().value("websiteUrl").toString() + "/download/" + version,
                        entry.value("hash").toString(), false, {} } };
                    QString error;
                    if (!resolveHeadlessBlockedMods(blocked, entry.value("hashType").toString(), error)) return OperationService::failure(error);
                    download = QUrl::fromLocalFile(blocked.first().localPath).toString();
                }
                return install(api, new InstanceImportTask(QUrl(download), nullptr, { { "pack_id", id }, { "pack_version_id", version } }), p, id, ui);
            }
        }
        return OperationService::failure("Pack version not found.", 2);
    }
    Fetch fetch{ api, ui, p.value("offline").toBool() };
    try {
        if (source == "atlauncher") {
            const auto document = fetch.json(BuildConfig.ATL_DOWNLOAD_SERVER_URL + "launcher/json/packsnew.json");
            if (!fetch.error.isEmpty()) return OperationService::failure(fetch.error);
            QJsonArray result;
            for (const auto& value : document.array()) {
                auto object = value.toObject();
                ATLauncher::IndexedPack pack{}; ATLauncher::loadIndexedPack(pack, object);
                if (pack.system || pack.versions.isEmpty() || pack.type != ATLauncher::PackType::Public) continue;
                if (search && !pack.name.contains(query, Qt::CaseInsensitive)) continue;
                if (!search && id != pack.safeName && id != QString::number(pack.id)) continue;
                QJsonArray versions;
                bool found = false;
                for (const auto& v : pack.versions) { versions.append(QJsonObject{ { "versionId", v.version }, { "minecraftVersion", v.minecraft } }); found |= v.version == version; }
                QJsonObject entry{ { "projectId", pack.safeName }, { "name", pack.name }, { "description", pack.description }, { "versions", versions }, { "raw", object } };
                if (installing) {
                    if (!found) return OperationService::failure("Pack version not found.", 2);
                    return install(api, new ATLauncher::PackInstallTask(new AtlInteraction(api, ui, p), pack.name, version), p, pack.name, ui);
                }
                if (!search) return OperationService::success(action == "versions" ? QJsonValue(versions) : QJsonValue(entry));
                result.append(entry);
            }
            return search ? OperationService::success(result) : OperationService::failure("Pack not found.", 2);
        }
        if (source == "ftb") {
            if (search) {
                const auto response = fetch.json(BuildConfig.FTB_API_BASE_URL + "/modpack/all");
                if (!fetch.error.isEmpty()) return OperationService::failure(fetch.error);
                QJsonArray result;
                for (const auto& packId : response.object().value("packs").toArray()) {
                    if (api.isCancellationRequested()) return OperationService::failure("Pack search cancelled.");
                    const auto pack = fetch.json(BuildConfig.FTB_API_BASE_URL + "/modpack/" + QString::number(packId.toInt())).object();
                    if (!fetch.error.isEmpty()) return OperationService::failure(fetch.error);
                    if (pack.value("name").toString().contains(query, Qt::CaseInsensitive))
                        result.append(QJsonObject{ { "projectId", QString::number(packId.toInt()) }, { "name", pack.value("name") }, { "description", pack.value("synopsis") }, { "raw", pack } });
                }
                return OperationService::success(result);
            }
            bool validId;
            if (id.toInt(&validId) <= 0 || !validId) return OperationService::failure("FTB IDs must be positive integers.", 2);
            auto object = fetch.json(BuildConfig.FTB_API_BASE_URL + "/modpack/" + id).object();
            if (!fetch.error.isEmpty()) return OperationService::failure(fetch.error);
            FTB::Modpack pack{}; FTB::loadModpack(pack, object);
            QJsonArray versions;
            QString selected;
            for (const auto& v : pack.versions) { versions.append(QJsonObject{ { "versionId", QString::number(v.id) }, { "name", v.name }, { "type", v.type } }); if (QString::number(v.id) == version || v.name == version) selected = v.name; }
            if (installing) {
                if (selected.isEmpty()) return OperationService::failure("Pack version not found.", 2);
                return install(api, new FTB::PackInstallTask(pack, selected), p, pack.name, ui);
            }
            return OperationService::success(action == "versions" ? QJsonValue(versions) : QJsonValue(object));
        }
        if (source == "technic") {
            QUrl url(BuildConfig.TECHNIC_API_BASE_URL + (search ? (query.isEmpty() ? "trending" : "search") : "modpack/" + id));
            QUrlQuery params; params.addQueryItem("build", BuildConfig.TECHNIC_API_BUILD);
            if (search && !query.isEmpty()) params.addQueryItem("q", query);
            const auto clientId = APPLICATION->settings()->get("TechnicClientID").toString();
            if (!clientId.isEmpty()) params.addQueryItem("cid", clientId);
            url.setQuery(params);
            const auto object = fetch.json(url.toString()).object();
            if (!fetch.error.isEmpty()) return OperationService::failure(fetch.error);
            if (search) return OperationService::success(object);
            auto download = object.value("url").toString();
            const bool solder = download.isEmpty();
            QJsonArray versions;
            if (solder) {
                download = object.value("solder").toString();
                while (download.endsWith('/')) download.chop(1);
                const auto builds = fetch.json(download + "/modpack/" + id).object().value("builds").toArray();
                if (!fetch.error.isEmpty()) return OperationService::failure(fetch.error);
                for (const auto& build : builds) versions.append(QJsonObject{ { "versionId", build } });
            } else versions.append(QJsonObject{ { "versionId", object.value("version") } });
            if (action == "versions") return OperationService::success(versions);
            if (!installing) return OperationService::success(object);
            bool found = false;
            for (const auto& v : versions) found |= v.toObject().value("versionId").toString() == version;
            const QUrl downloadUrl(download);
            if (!found || !downloadUrl.isValid() || downloadUrl.host().isEmpty() || (downloadUrl.scheme() != "http" && downloadUrl.scheme() != "https")) return OperationService::failure("Invalid pack version or download URL.", 2);
            InstanceTask* task = solder ? static_cast<InstanceTask*>(new Technic::SolderPackInstallTask(APPLICATION->network().get(), download, id, version, object.value("minecraft").toString())) :
                static_cast<InstanceTask*>(new Technic::SingleZipPackInstallTask(download, object.value("minecraft").toString()));
            return install(api, task, p, object.value("displayName").toString(id), ui);
        }
        if (source == "legacy-ftb") {
            LegacyFTB::ModpackList packs;
            const auto code = p.value("privateCode").toString();
            if (!code.isEmpty() && !identifier(code)) return OperationService::failure("Invalid private pack code.", 2);
            const QStringList catalogs = code.isEmpty() ? QStringList{ "modpacks", "thirdparty" } : QStringList{ code };
            for (const auto& catalog : catalogs) {
                auto bytes = fetch.bytes(BuildConfig.LEGACY_FTB_CDN_BASE_URL + "static/" + catalog + ".xml");
                if (!fetch.error.isEmpty()) return OperationService::failure(fetch.error);
                if (!LegacyFTB::PackFetchTask::parseAndAddPacks(bytes, code.isEmpty() ? (catalog == "modpacks" ? LegacyFTB::PackType::Public : LegacyFTB::PackType::ThirdParty) : LegacyFTB::PackType::Private, packs))
                    return OperationService::failure("Invalid legacy FTB catalog.");
            }
            QJsonArray result;
            for (auto pack : packs) {
                if (pack.broken || (search && !pack.name.contains(query, Qt::CaseInsensitive)) || (!search && pack.dir != id)) continue;
                pack.packCode = code;
                QJsonArray versions;
                for (const auto& v : pack.oldVersions) versions.append(QJsonObject{ { "versionId", v } });
                if (installing) {
                    if (!pack.oldVersions.contains(version)) return OperationService::failure("Pack version not found.", 2);
                    return install(api, new LegacyFTB::PackInstallTask(APPLICATION->network().get(), pack, version), p, pack.name, ui);
                }
                QJsonObject entry{ { "projectId", pack.dir }, { "name", pack.name }, { "description", pack.description },
                    { "author", pack.author }, { "minecraftVersion", pack.mcVersion }, { "versions", versions } };
                if (!search) return OperationService::success(action == "versions" ? QJsonValue(versions) : QJsonValue(entry));
                result.append(entry);
            }
            return search ? OperationService::success(result) : OperationService::failure("Pack not found.", 2);
        }
    } catch (const Exception& error) { return OperationService::failure(error.cause()); }
    return OperationService::failure("Unknown modpack provider.", 2);
}
}

void registerLauncherApiPackOperations(LauncherApi& api)
{
    using namespace ApiSupport;
    api.registerOperation({ "modpack.legacy-ftb.private-codes", "Read or replace saved legacy FTB private pack codes.", schema({ { "codes", strings() } }), "modpacks", true },
        [](const QJsonObject& p, UserInteraction&) {
            LegacyFTB::PrivatePackManager manager; manager.load();
            if (p.contains("codes")) {
                QStringList codes;
                for (const auto& value : p.value("codes").toArray()) {
                    if (!identifier(value.toString())) return OperationService::failure("Invalid private pack code.", 2);
                    codes.append(value.toString());
                }
                for (const auto& code : manager.getCurrentPackCodes().values()) manager.remove(code);
                for (const auto& code : codes) manager.add(code);
                manager.save();
            }
            return OperationService::success(QJsonArray::fromStringList(manager.getCurrentPackCodes().values()));
        });
    for (const QString action : { "search", "project", "versions", "install" }) {
        QJsonObject properties{ { "provider", string("atlauncher, ftb, technic, legacy-ftb, modrinth, curseforge, or js:<id>.") },
            { "projectId", string("Pack ID returned by search.") }, { "query", string("Search term.") }, { "versionId", string("Selected pack version.") },
            { "offline", boolean() }, { "privateCode", string("Optional legacy FTB private pack code.") } };
        if (action == "install") {
            for (const auto key : { "name", "group", "icon" }) properties.insert(key, string(QString::fromLatin1(key)));
            properties.insert("optionalMods", strings());
        }
        QJsonArray required{ "provider" };
        if (action != "search") required.append("projectId");
        if (action == "install") required.append("versionId");
        api.registerOperation({ "modpack." + action, "Access GUI modpack providers through existing parsers and installation tasks: " + action + ".", schema(properties, required), "modpacks", action == "install" },
            [&api, action](const QJsonObject& p, UserInteraction& ui) { return packOperation(api, action, p, ui); });
    }
    api.registerOperation({ "modpack.ftb-local.list", "List FTB App instances in a selected directory.", schema({ { "path", string("FTB App instances directory; defaults to saved setting.") } }), "modpacks" },
        [](const QJsonObject& p, UserInteraction&) {
            const auto path = p.value("path").toString(APPLICATION->settings()->get("FTBAppInstancesPath").toString());
            if (path.isEmpty() || !QFileInfo(path).isDir()) return OperationService::failure("Choose an FTB App instances directory.", 2);
            QJsonArray result;
            for (const auto& dir : QDir(path).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks)) {
                const auto pack = FTBImportAPP::parseDirectory(dir.absoluteFilePath());
                if (!pack.path.isEmpty()) result.append(QJsonObject{ { "path", pack.path }, { "uuid", pack.uuid }, { "name", pack.name },
                    { "projectId", pack.id }, { "versionId", pack.versionId }, { "version", pack.version }, { "minecraftVersion", pack.mcVersion }, { "playTime", pack.totalPlayTime } });
            }
            return OperationService::success(result);
        });
    api.registerOperation({ "modpack.ftb-local.import", "Copy an FTB App instance using the existing migration task.",
        schema({ { "path", string("FTB App instance directory.") }, { "name", string("New instance name.") }, { "group", string("Instance group.") }, { "icon", string("Instance icon.") } }, { "path" }), "modpacks", true },
        [&api](const QJsonObject& p, UserInteraction& ui) {
            const auto pack = FTBImportAPP::parseDirectory(p.value("path").toString());
            if (pack.path.isEmpty()) return OperationService::failure("Invalid FTB App instance.", 2);
            return install(api, new FTBImportAPP::PackInstallTask(pack), p, pack.name, ui);
        });
    api.registerOperation({ "modpack.atlauncher.share-code", "Resolve an ATLauncher share code, including optional mod choices.",
        schema({ { "code", string("Share code.") }, { "offline", boolean() } }, { "code" }), "modpacks" },
        [&api](const QJsonObject& p, UserInteraction& ui) {
            if (!identifier(p.value("code").toString())) return OperationService::failure("Invalid share code.", 2);
            Fetch fetch{ api, ui, p.value("offline").toBool() };
            const auto result = fetch.json(BuildConfig.ATL_API_BASE_URL + "share-codes/" + p.value("code").toString());
            return fetch.error.isEmpty() ? OperationService::success(result.object()) : OperationService::failure(fetch.error);
        });
}
