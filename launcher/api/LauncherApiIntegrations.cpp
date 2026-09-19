// SPDX-License-Identifier: GPL-3.0-only
#include "LauncherApiIntegrations.h"

#include "api/LauncherApiSupport.h"
#include "cli/OperationService.h"
#include "minecraft/online/Terracotta.h"
#include "minecraft/online/TerracottaDownload.h"
#include "minecraft/online/YukariConnect.h"
#include "minecraft/online/YukariConnectDownload.h"
#include "net/Aria2Manager.h"
#include "Application.h"
#include "InstanceList.h"
#include "minecraft/auth/AuthlibInjector.h"
#include "minecraft/auth/AuthlibInjectorDownload.h"
#include "minecraft/auth/Nide8Auth.h"
#include "minecraft/auth/Nide8AuthDownload.h"
#include "tools/BaseProfiler.h"
#include "tools/MCEditTool.h"
#include "modplatform/flame/CurseForgeDownloadPageService.h"

namespace {
using namespace ApiSupport;

template <typename Service>
QJsonObject localStatus(const QString& name, Service& service)
{
    return { { "id", name }, { "installed", service.checkCache() }, { "running", service.isProcessRunning() },
             { "version", service.getVersion() }, { "path", service.getLocalPath() } };
}

QJsonObject ariaStatus()
{
    const auto service = Net::Aria2Manager::instance();
    QString reason;
    const auto canInstall = service->canInstallManagedExecutable(&reason);
    return { { "id", "aria2" }, { "installed", !service->findExecutable().isEmpty() }, { "running", service->isRunning() },
             { "enabled", service->isEnabledBySettings() }, { "fallbackToQt", service->fallbackToQtEnabled() },
             { "path", service->findExecutable() }, { "managedPath", service->managedExecutablePath() },
             { "status", service->statusText() }, { "canInstall", canInstall }, { "installUnavailableReason", reason },
             { "maxConcurrentDownloads", service->maxConcurrentDownloads() } };
}

template <typename Service>
QJsonObject roomOperation(Service& service, const QString& action, const QJsonObject& p)
{
    if (action == "start") {
        if (!service.startProcess()) return OperationService::failure("The integration could not be started.");
        return OperationService::success(QJsonObject{ { "running", service.isProcessRunning() } });
    }
    if (action == "stop") {
        if (!service.shutdownAndWait()) return OperationService::failure("The integration did not stop within its shutdown timeout.");
        return OperationService::success(QJsonObject{ { "running", service.isProcessRunning() } });
    }
    if (!service.isProcessRunning()) return OperationService::failure("Start the integration process first.", 2);
    if (action == "state") {
        const auto state = service.fetchState();
        if (!state) return OperationService::failure("The integration did not return a room state.");
        const QStringList states{ "waiting", "host-scanning", "host-starting", "host-ok", "guest-connecting", "guest-starting", "guest-ok", "exception" };
        const auto code = static_cast<int>(state->state);
        QJsonArray profiles;
        for (const auto& profile : state->profiles) profiles.append(profile.toJson());
        QJsonObject data{ { "state", states.value(code, "unknown") }, { "index", state->index }, { "room", state->room },
            { "url", state->url }, { "profileIndex", state->profile_index }, { "profiles", profiles } };
        if (code == 5) data.insert("difficulty", static_cast<int>(state->difficulty));
        if (code == 7) data.insert("exceptionType", static_cast<int>(state->exception_type));
        return OperationService::success(data);
    }
    if (action == "log") {
        const auto maxBytes = qBound<qint64>(qint64(1), p.value("maxBytes").toInteger(256 * 1024), qint64(1024 * 1024));
        const auto content = service.fetchLog();
        return OperationService::success(QJsonObject{ { "content", QString::fromUtf8(content.right(maxBytes)) },
            { "truncated", content.size() > maxBytes } });
    }
    bool ok = false;
    if (action == "leave") {
        ok = service.cancelState();
    } else {
        const auto player = p.value("player").toString().trimmed();
        if (player.isEmpty() || player.size() > 128) return OperationService::failure("player must contain 1 to 128 characters.", 2);
        if (action == "host") {
            ok = service.startScanning(player);
        } else {
            const auto room = p.value("room").toString().trimmed();
            if (room.isEmpty() || room.size() > 4096) return OperationService::failure("Invalid room code.", 2);
            ok = service.joinRoom(room, player);
        }
    }
    return ok ? OperationService::success(QJsonObject{ { "accepted", true } }) : OperationService::failure("The room operation failed.");
}

QJsonObject control(LauncherApi& api, const QString& action, const QJsonObject& p, UserInteraction& interaction)
{
    const auto id = p.value("integration").toString();
    if (id != "aria2" && id != "terracotta" && id != "yukari")
        return OperationService::failure("integration must be aria2, terracotta, or yukari.", 2);
    if (action == "install") {
        Task::Ptr task;
        if (id == "aria2") {
            QString reason;
            if (!Net::Aria2Manager::instance()->canInstallManagedExecutable(&reason)) return OperationService::failure(reason, 2);
            task = Net::Aria2Manager::instance()->createInstallTask();
        } else if (id == "terracotta") {
            if (Terracotta::instance().isProcessRunning()) return OperationService::failure("Stop Terracotta before updating it.", 2);
            task = makeShared<TerracottaDownload>(p.value("mirror").toBool());
        } else {
            if (YukariConnect::instance().isProcessRunning()) return OperationService::failure("Stop Yukari before updating it.", 2);
            task = makeShared<YukariConnectDownload>(p.value("mirror").toBool());
        }
        QString error;
        if (!wait(api, task, interaction, error)) return OperationService::failure(error);
        return OperationService::success(QJsonObject{ { "integration", id }, { "installed", true } });
    }
    if (id == "aria2") {
        auto service = Net::Aria2Manager::instance();
        QString error;
        if (action == "start") {
            if (!service->ensureStarted(&error)) return OperationService::failure(error);
        } else if (action == "stop") {
            service->shutdown();
        } else {
            return OperationService::failure("This room operation is not supported by Aria2.", 2);
        }
        return OperationService::success(ariaStatus());
    }
    if (id == "terracotta") return roomOperation(Terracotta::instance(), action, p);
    return roomOperation(YukariConnect::instance(), action, p);
}
}  // namespace

void registerLauncherApiIntegrationOperations(LauncherApi& api)
{
    api.registerOperation({ "external-tool.check", "Check a configured profiler, MCEdit, or CurseForge download tool using its existing validator.",
        schema({ { "tool", string("jprofiler, jvisualvm, mcedit, or curseforge.") }, { "path", string("Installation path to validate.") } }, { "tool", "path" }), "integrations" },
        [](const QJsonObject& p, UserInteraction&) {
            const auto tool = p.value("tool").toString(), path = p.value("path").toString();
            QString error; bool valid = false, headless = false;
            if (tool == "mcedit") valid = APPLICATION->mcedit()->check(path, error);
            else if (tool == "curseforge") valid = CurseForgeDownloadPageService::probeExternalTool(path, &error, &headless);
            else {
                const auto factory = APPLICATION->profilers().value(tool);
                if (!factory) return OperationService::failure("Unknown external tool.", 2);
                valid = factory->check(path, &error);
            }
            return OperationService::success(QJsonObject{ { "valid", valid }, { "error", error }, { "supportsHeadless", headless } });
        });
    for (const QString action : { "status", "install", "remove" }) {
        api.registerOperation({ "authentication.helper." + action, "Manage the authlib-injector or nide8auth helper: " + action + ".",
            schema({ { "helper", string("authlib-injector or nide8auth.") }, { "confirm", boolean() } }, { "helper" }), "integrations", action != "status" },
            [&api, action](const QJsonObject& p, UserInteraction& interaction) {
                const auto helper = p.value("helper").toString();
                const bool authlib = helper == "authlib-injector";
                if (!authlib && helper != "nide8auth") return OperationService::failure("Unknown authentication helper.", 2);
                const auto path = authlib ? AuthlibInjector::instance().getLocalPath() : Nide8Auth::instance().getLocalPath();
                if (action == "install") {
                    Task::Ptr task;
                    if (authlib) task = makeShared<AuthlibInjectorDownload>(); else task = makeShared<Nide8AuthDownload>();
                    QString error;
                    if (!wait(api, task, interaction, error)) return OperationService::failure(error);
                } else if (action == "remove") {
                    if (!p.value("confirm").toBool()) return OperationService::failure("Removal requires confirm=true.", 2);
                    for (int row = 0; row < APPLICATION->instances()->count(); ++row)
                        if (APPLICATION->instances()->at(row)->isRunning()) return OperationService::failure("Stop running instances before removing authentication helpers.", 2);
                    if (QFileInfo::exists(path) && !QFile::remove(path)) return OperationService::failure("Could not remove authentication helper.");
                    if (authlib) {
                        const auto metadata = AuthlibInjector::instance().getMetadataPath();
                        if (QFileInfo::exists(metadata) && !QFile::remove(metadata)) return OperationService::failure("Helper removed, but metadata removal failed.");
                    }
                }
                const QFileInfo file(path);
                return OperationService::success(QJsonObject{ { "helper", helper }, { "path", path }, { "installed", file.isFile() },
                    { "size", file.size() }, { "valid", authlib ? AuthlibInjector::instance().checkCache() : Nide8Auth::instance().checkCache() },
                    { "version", authlib && file.isFile() ? AuthlibInjector::instance().getVersion() : QString() } });
            });
    }
    using namespace ApiSupport;
    api.registerOperation({ "integration.status", "Read installation and process state for optional integrations.", schema({}), "integrations" },
        [](const QJsonObject&, UserInteraction&) { return OperationService::success(QJsonArray{
            ariaStatus(), localStatus("terracotta", Terracotta::instance()), localStatus("yukari", YukariConnect::instance()) }); });
    const auto integration = string("aria2, terracotta, or yukari; room operations require terracotta or yukari.");
    for (const auto& action : { QString("install"), QString("start"), QString("stop"), QString("state"), QString("log"),
                               QString("host"), QString("join"), QString("leave") }) {
        QJsonObject properties{ { "integration", integration } };
        QJsonArray required{ "integration" };
        if (action == "install") properties.insert("mirror", boolean());
        if (action == "log") properties.insert("maxBytes", QJsonObject{ { "type", "integer" }, { "minimum", 1 }, { "maximum", 1048576 } });
        if (action == "host" || action == "join") { properties.insert("player", string("Player display name.")); required.append("player"); }
        if (action == "join") { properties.insert("room", string("Room code.")); required.append("room"); }
        api.registerOperation({ "integration." + action, "Optional integration operation: " + action + ".",
            schema(properties, required), "integrations", action != "state" && action != "log" },
            [&api, action](const QJsonObject& p, UserInteraction& i) { return control(api, action, p, i); });
    }
    api.registerOperation({ "aria2.downloads", "Read the Aria2 download queue.", schema({}), "integrations" },
        [](const QJsonObject&, UserInteraction&) {
            QJsonArray entries;
            for (const auto& d : Net::Aria2Manager::instance()->downloads()) entries.append(QJsonObject{
                { "gid", d.gid }, { "url", d.url }, { "path", d.path }, { "status", d.status }, { "errorCode", d.errorCode },
                { "errorMessage", d.errorMessage }, { "completedLength", d.completedLength }, { "totalLength", d.totalLength }, { "downloadSpeed", d.downloadSpeed } });
            return OperationService::success(entries);
        });
    api.registerOperation({ "aria2.cancel", "Request cancellation of one Aria2 download.", schema({ { "gid", string("Download GID.") } }, { "gid" }), "integrations", true },
        [](const QJsonObject& p, UserInteraction&) {
            const auto gid = p.value("gid").toString();
            auto service = Net::Aria2Manager::instance();
            for (const auto& d : service->downloads()) {
                if (d.gid != gid) continue;
                service->removeDownload(gid);
                return OperationService::success(QJsonObject{ { "gid", gid }, { "requested", true } });
            }
            return OperationService::failure("Download GID not found.", 2);
        });
    api.registerOperation({ "aria2.clear-finished", "Clear finished Aria2 download records.", schema({}), "integrations", true },
        [](const QJsonObject&, UserInteraction&) { Net::Aria2Manager::instance()->clearFinished(); return OperationService::success(); });
    api.registerOperation({ "aria2.remove", "Remove the managed Aria2 executable.", schema({ { "confirm", boolean() } }, { "confirm" }), "integrations", true },
        [](const QJsonObject& p, UserInteraction&) {
            if (!p.value("confirm").toBool()) return OperationService::failure("Removal requires confirm=true.", 2);
            QString error;
            if (!Net::Aria2Manager::instance()->removeManagedExecutable(&error)) return OperationService::failure(error);
            return OperationService::success();
        });
    api.registerOperation({ "integration.retry", "Retry a failed Yukari room connection.", schema({ { "integration", integration } }, { "integration" }), "integrations" },
        [](const QJsonObject& p, UserInteraction&) {
            if (p.value("integration").toString() != "yukari") return OperationService::failure("Retry is only available for Yukari.", 2);
            if (!YukariConnect::instance().isProcessRunning() || !YukariConnect::instance().retryRoom()) return OperationService::failure("Yukari room retry failed.");
            return OperationService::success(QJsonObject{ { "accepted", true } });
        });
}
