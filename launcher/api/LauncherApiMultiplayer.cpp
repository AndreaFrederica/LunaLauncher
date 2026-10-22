// SPDX-License-Identifier: GPL-3.0-only

#include "LauncherApiMultiplayer.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <utility>

#include "LauncherApi.h"
#include "LauncherApiSupport.h"
#include "cli/OperationService.h"
#include "minecraft/online/Terracotta.h"
#include "minecraft/online/YukariConnect.h"

namespace {

QJsonObject stringProperty(const QString& description)
{
    return ApiSupport::string(description);
}

QJsonObject boolProperty(const QString& description)
{
    return { { "type", "boolean" }, { "description", description } };
}

QJsonObject objectSchema(QJsonObject properties, QJsonArray required = {})
{
    return ApiSupport::schema(std::move(properties), std::move(required));
}

QString stateName(TerracottaTypes::State state)
{
    switch (state) {
        case TerracottaTypes::State::Waiting: return "waiting";
        case TerracottaTypes::State::HostScanning: return "host-scanning";
        case TerracottaTypes::State::HostStarting: return "host-starting";
        case TerracottaTypes::State::HostOk: return "host-ok";
        case TerracottaTypes::State::GuestConnecting: return "guest-connecting";
        case TerracottaTypes::State::GuestStarting: return "guest-starting";
        case TerracottaTypes::State::GuestOk: return "guest-ok";
        case TerracottaTypes::State::Exception: return "exception";
    }
    return "unknown";
}

QString stateName(YukariConnectTypes::State state)
{
    switch (state) {
        case YukariConnectTypes::State::Waiting: return "waiting";
        case YukariConnectTypes::State::HostScanning: return "host-scanning";
        case YukariConnectTypes::State::HostStarting: return "host-starting";
        case YukariConnectTypes::State::HostOk: return "host-ok";
        case YukariConnectTypes::State::GuestConnecting: return "guest-connecting";
        case YukariConnectTypes::State::GuestStarting: return "guest-starting";
        case YukariConnectTypes::State::GuestOk: return "guest-ok";
        case YukariConnectTypes::State::Exception: return "exception";
    }
    return "unknown";
}

QString difficultyName(TerracottaTypes::Difficulty value)
{
    switch (value) {
        case TerracottaTypes::Difficulty::EASIEST: return "easiest";
        case TerracottaTypes::Difficulty::SIMPLE: return "simple";
        case TerracottaTypes::Difficulty::MEDIUM: return "medium";
        case TerracottaTypes::Difficulty::TOUGH: return "tough";
        case TerracottaTypes::Difficulty::UNKNOWN: break;
    }
    return "unknown";
}

QString difficultyName(YukariConnectTypes::Difficulty value)
{
    switch (value) {
        case YukariConnectTypes::Difficulty::EASIEST: return "easiest";
        case YukariConnectTypes::Difficulty::SIMPLE: return "simple";
        case YukariConnectTypes::Difficulty::MEDIUM: return "medium";
        case YukariConnectTypes::Difficulty::TOUGH: return "tough";
        case YukariConnectTypes::Difficulty::UNKNOWN: break;
    }
    return "unknown";
}

QString profileKindName(TerracottaTypes::ProfileKind kind)
{
    switch (kind) {
        case TerracottaTypes::ProfileKind::HOST: return "host";
        case TerracottaTypes::ProfileKind::LOCAL: return "local";
        case TerracottaTypes::ProfileKind::GUEST: return "guest";
    }
    return "unknown";
}

QString profileKindName(YukariConnectTypes::ProfileKind kind)
{
    switch (kind) {
        case YukariConnectTypes::ProfileKind::HOST: return "host";
        case YukariConnectTypes::ProfileKind::LOCAL: return "local";
        case YukariConnectTypes::ProfileKind::GUEST: return "guest";
    }
    return "unknown";
}

QJsonArray profileArray(const QList<TerracottaTypes::Profile>& profiles)
{
    QJsonArray result;
    for (const auto& profile : profiles) {
        result.append(QJsonObject{ { "machineId", profile.machine_id }, { "name", profile.name },
                                   { "vendor", profile.vendor }, { "kind", profileKindName(profile.kind) } });
    }
    return result;
}

QJsonArray profileArray(const QList<YukariConnectTypes::Profile>& profiles)
{
    QJsonArray result;
    for (const auto& profile : profiles) {
        result.append(QJsonObject{ { "machineId", profile.machine_id }, { "name", profile.name },
                                   { "vendor", profile.vendor }, { "kind", profileKindName(profile.kind) } });
    }
    return result;
}

QJsonObject terracottaState(const TerracottaTypes::StateResponse& state)
{
    return QJsonObject{ { "index", static_cast<double>(state.index) }, { "state", stateName(state.state) }, { "room", state.room },
                        { "url", state.url }, { "profiles", profileArray(state.profiles) },
                        { "difficulty", difficultyName(state.difficulty) },
                        { "exception", static_cast<int>(state.exception_type) } };
}

QJsonObject yukariState(const YukariConnectTypes::StateResponse& state)
{
    return QJsonObject{ { "index", static_cast<double>(state.index) }, { "state", stateName(state.state) }, { "room", state.room },
                        { "url", state.url }, { "profiles", profileArray(state.profiles) },
                        { "difficulty", difficultyName(state.difficulty) },
                        { "exception", static_cast<int>(state.exception_type) } };
}

QJsonObject terracottaMeta(const TerracottaTypes::MetaInfo& meta)
{
    return QJsonObject{ { "version", meta.version }, { "compileTimestamp", meta.compile_timestamp },
                        { "easytierVersion", meta.easytier_version }, { "targetArch", meta.target_arch },
                        { "targetEnv", meta.target_env }, { "targetOs", meta.target_os },
                        { "targetTuple", meta.target_tuple }, { "targetVendor", meta.target_vendor },
                        { "yggdrasilPort", meta.yggdrasil_port } };
}

QJsonObject yukariMeta(const YukariConnectTypes::MetaInfo& meta)
{
    return QJsonObject{ { "version", meta.version }, { "compileTimestamp", meta.compile_timestamp },
                        { "coreVersion", meta.core_version }, { "targetArch", meta.target_arch },
                        { "targetEnv", meta.target_env }, { "targetOs", meta.target_os },
                        { "targetTuple", meta.target_tuple }, { "targetVendor", meta.target_vendor },
                        { "yggdrasilPort", meta.yggdrasil_port } };
}

bool isTerracotta(const QString& backend)
{
    return backend == "terracotta";
}

bool isYukari(const QString& backend)
{
    return backend == "yukari" || backend == "yukari-connect" || backend == "yukariconnect";
}

QJsonObject backendSummary(const QString& backend)
{
    if (isTerracotta(backend)) {
        auto& service = Terracotta::instance();
        return QJsonObject{ { "id", "terracotta" }, { "name", "Terracotta" },
                            { "installed", service.checkCache() }, { "version", service.getVersion() },
                            { "processRunning", service.isProcessRunning() }, { "baseUrl", service.getBaseUrl() } };
    }
    if (isYukari(backend)) {
        auto& service = YukariConnect::instance();
        return QJsonObject{ { "id", "yukari" }, { "name", "YukariConnect" },
                            { "installed", service.checkCache() }, { "version", service.getVersion() },
                            { "processRunning", service.isProcessRunning() }, { "baseUrl", service.getBaseUrl() } };
    }
    return {};
}

QJsonObject allBackends()
{
    QJsonArray backends{ backendSummary("terracotta"), backendSummary("yukari") };
    return QJsonObject{ { "backends", backends } };
}

QJsonObject backendStatus(const QString& backend, bool fetchState)
{
    if (isTerracotta(backend)) {
        auto& service = Terracotta::instance();
        auto result = backendSummary(backend);
        const auto running = service.isProcessRunning();
        const auto available = running && service.isAvailable();
        result["available"] = available;
        if (available) {
            if (auto meta = service.fetchMeta(2500)) result["meta"] = terracottaMeta(*meta);
            if (fetchState) {
                if (auto state = service.fetchState()) result["state"] = terracottaState(*state);
            }
        }
        return result;
    }
    if (isYukari(backend)) {
        auto& service = YukariConnect::instance();
        auto result = backendSummary(backend);
        const auto running = service.isProcessRunning();
        const auto available = running && service.isAvailable();
        result["available"] = available;
        if (available) {
            if (auto meta = service.fetchMeta(2500)) result["meta"] = yukariMeta(*meta);
            if (fetchState) {
                if (auto state = service.fetchState()) result["state"] = yukariState(*state);
            }
        }
        return result;
    }
    return {};
}

QString ensureProcess(const QString& backend)
{
    if (isTerracotta(backend)) {
        auto& service = Terracotta::instance();
        if (!service.checkCache()) return QStringLiteral("Terracotta is not installed.");
        if (!service.isProcessRunning() && !service.startProcess()) return QStringLiteral("Could not start Terracotta.");
        return {};
    }
    if (isYukari(backend)) {
        auto& service = YukariConnect::instance();
        if (!service.checkCache()) return QStringLiteral("YukariConnect is not installed.");
        if (!service.isProcessRunning() && !service.startProcess()) return QStringLiteral("Could not start YukariConnect.");
        return {};
    }
    return QStringLiteral("Unknown multiplayer backend.");
}

}  // namespace

void registerLauncherApiMultiplayerOperations(LauncherApi& api)
{
    const auto backend = stringProperty("Backend id: terracotta or yukari.");
    const auto player = stringProperty("Player name shown to the other side.");
    const auto room = stringProperty("Invite code for guest mode.");
    const auto mode = stringProperty("host or guest.");
    const auto mirror = boolProperty("Use the configured mirror for downloads.");
    const auto fetchState = boolProperty("Also fetch the current P2P state. Defaults to true.");

    api.registerOperation({ "multiplayer.backends",
        "List the two bundled P2P multiplayer backends, their installed versions and process state.",
        objectSchema({}), "multiplayer" },
        [](const QJsonObject&, UserInteraction&) { return OperationService::success(allBackends()); });

    api.registerOperation({ "multiplayer.status",
        "Read backend availability, metadata and the current host/guest state.",
        objectSchema({ { "backend", backend }, { "fetchState", fetchState } }, { "backend" }), "multiplayer" },
        [](const QJsonObject& p, UserInteraction&) {
            const auto id = p.value("backend").toString();
            if (!isTerracotta(id) && !isYukari(id)) return OperationService::failure("Unknown multiplayer backend.", 2);
            return OperationService::success(backendStatus(id, p.value("fetchState").toBool(true)));
        });

    api.registerOperation({ "multiplayer.install",
        "Download the selected backend into the launcher-managed cache.",
        objectSchema({ { "backend", backend }, { "mirror", mirror } }, { "backend" }), "multiplayer", true },
        [](const QJsonObject& p, UserInteraction&) {
            const auto id = p.value("backend").toString();
            if (!isTerracotta(id) && !isYukari(id)) return OperationService::failure("Unknown multiplayer backend.", 2);
            QString path;
            if (isTerracotta(id)) {
                auto& service = Terracotta::instance();
                if (!service.checkCache()) path = service.downloadLatest(p.value("mirror").toBool());
            } else {
                auto& service = YukariConnect::instance();
                if (!service.checkCache()) path = service.downloadLatest(p.value("mirror").toBool());
            }
            auto result = backendSummary(id);
            if (!result.value("installed").toBool()) return OperationService::failure("The multiplayer backend download failed.");
            if (!path.isEmpty()) result["path"] = path;
            return OperationService::success(result);
        });

    api.registerOperation({ "multiplayer.start",
        "Start a host room or join a guest room with the selected backend.",
        objectSchema({ { "backend", backend }, { "mode", mode }, { "player", player }, { "room", room },
                       { "scaffoldingPort", QJsonObject{ { "type", "integer" } } } },
                     { "backend", "mode", "player" }), "multiplayer", true },
        [](const QJsonObject& p, UserInteraction&) {
            const auto id = p.value("backend").toString();
            const auto selectedMode = p.value("mode").toString();
            const auto playerName = p.value("player").toString().trimmed();
            if (!isTerracotta(id) && !isYukari(id)) return OperationService::failure("Unknown multiplayer backend.", 2);
            if (playerName.isEmpty()) return OperationService::failure("player must not be empty.", 2);
            if (selectedMode != "host" && selectedMode != "guest") return OperationService::failure("mode must be host or guest.", 2);
            if (selectedMode == "guest" && p.value("room").toString().trimmed().isEmpty())
                return OperationService::failure("room is required in guest mode.", 2);
            const auto processError = ensureProcess(id);
            if (!processError.isEmpty()) return OperationService::failure(processError);
            bool started = false;
            if (isTerracotta(id)) {
                auto& service = Terracotta::instance();
                started = selectedMode == "host" ? service.startScanning(playerName) : service.joinRoom(p.value("room").toString(), playerName);
            } else {
                auto& service = YukariConnect::instance();
                started = selectedMode == "host" ? service.httpStartHost(playerName, p.value("scaffoldingPort").toInt())
                                                 : service.httpStartGuest(p.value("room").toString(), playerName);
            }
            if (!started) return OperationService::failure("The backend rejected the host/guest request.");
            auto result = backendStatus(id, true);
            result["mode"] = selectedMode;
            return OperationService::success(result);
        });

    api.registerOperation({ "multiplayer.stop",
        "Stop the current room, or panic the whole backend process.",
        objectSchema({ { "backend", backend }, { "panic", boolProperty("Stop the backend process itself.") } }, { "backend" }),
        "multiplayer", true },
        [](const QJsonObject& p, UserInteraction&) {
            const auto id = p.value("backend").toString();
            if (!isTerracotta(id) && !isYukari(id)) return OperationService::failure("Unknown multiplayer backend.", 2);
            if (p.value("panic").toBool()) {
                if (isTerracotta(id)) Terracotta::instance().panic(true);
                else YukariConnect::instance().panic(true);
            } else if (isTerracotta(id)) {
                if (!Terracotta::instance().cancelState()) return OperationService::failure("Could not stop the Terracotta room.");
            } else {
                if (!YukariConnect::instance().httpStopRoom()) return OperationService::failure("Could not stop the YukariConnect room.");
            }
            return OperationService::success(backendStatus(id, true));
        });

    api.registerOperation({ "multiplayer.retry",
        "Retry the current room after an exception. YukariConnect only.",
        objectSchema({ { "backend", backend } }, { "backend" }), "multiplayer", true },
        [](const QJsonObject& p, UserInteraction&) {
            const auto id = p.value("backend").toString();
            if (!isYukari(id)) return OperationService::failure("retry is only supported by YukariConnect.", 2);
            if (!YukariConnect::instance().httpRetryRoom()) return OperationService::failure("The backend rejected the retry request.");
            return OperationService::success(backendStatus(id, true));
        });

    api.registerOperation({ "multiplayer.log",
        "Fetch the selected backend's own log buffer.",
        objectSchema({ { "backend", backend } }, { "backend" }), "multiplayer" },
        [](const QJsonObject& p, UserInteraction&) {
            const auto id = p.value("backend").toString();
            if (isTerracotta(id)) return OperationService::success(QJsonObject{ { "log", QString::fromUtf8(Terracotta::instance().fetchLog()) } });
            if (isYukari(id)) return OperationService::success(QJsonObject{ { "log", QString::fromUtf8(YukariConnect::instance().fetchLog()) } });
            return OperationService::failure("Unknown multiplayer backend.", 2);
        });
}
