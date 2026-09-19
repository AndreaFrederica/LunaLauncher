// SPDX-License-Identifier: GPL-3.0-only
#include "LauncherApiFiles.h"
#include "api/LauncherApiSupport.h"
#include "Application.h"
#include "InstanceList.h"
#include "FileSystem.h"
#include "CustomUiRuntime.h"
#include <QUuid>
#include "cli/OperationService.h"
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QCryptographicHash>

namespace {
using namespace ApiSupport;
QString boundedPath(BaseInstance* instance, QString relative)
{
    relative = QDir::fromNativeSeparators(relative);
    if (relative.isEmpty()) relative = ".";
    if (QDir::isAbsolutePath(relative) || relative.contains(':') || relative.contains(QChar(0))) return {};
    const auto root = QFileInfo(instance->instanceRoot()).canonicalFilePath();
    auto current = root;
    if (root.isEmpty()) return {};
    for (const auto& part : relative.split('/')) {
        if (part == "..") return {};
        if (part.isEmpty() || part == ".") continue;
        if (part.endsWith('.') || part.endsWith(' ')) return {};
        current = QDir(current).filePath(part);
        const QFileInfo info(current);
        if (info.isSymLink()) return {};
        if (info.exists() && info.canonicalFilePath() != root && !info.canonicalFilePath().startsWith(root + '/')) return {};
    }
    return current;
}
QString revision(const QByteArray& bytes) { return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()); }
QJsonObject fileOperation(const QString& action, const QJsonObject& p)
{
    const auto instance = APPLICATION->instances()->getInstanceById(p.value("instance").toString());
    if (!instance) return OperationService::failure("Instance ID not found.", 2);
    const auto path = boundedPath(instance, p.value("path").toString());
    if (path.isEmpty()) return OperationService::failure("Path must stay inside the instance, without symbolic links.", 2);
    const QFileInfo info(path);
    const auto encoding = p.value("encoding").toString("utf8");
    if (encoding != "utf8" && encoding != "base64") return OperationService::failure("encoding must be utf8 or base64.", 2);
    if (action == "stat") return OperationService::success(QJsonObject{ { "exists", info.exists() }, { "isFile", info.isFile() },
        { "isDir", info.isDir() }, { "size", info.size() }, { "path", path } });
    if (action == "list") {
        if (!info.isDir()) return OperationService::failure("Directory not found.", 2);
        QJsonArray entries;
        for (const auto& entry : QDir(path).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden, QDir::Name)) {
            if (entries.size() >= 4096) return OperationService::failure("Directory exceeds 4096 entries.", 2);
            entries.append(QJsonObject{ { "name", entry.fileName() }, { "isDir", entry.isDir() }, { "isFile", entry.isFile() },
                { "isLink", entry.isSymLink() }, { "size", entry.size() } });
        }
        return OperationService::success(entries);
    }
    if (action == "read") {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return OperationService::failure(file.errorString());
        const auto bytes = file.read(4 * 1024 * 1024 + 1);
        if (bytes.size() > 4 * 1024 * 1024) return OperationService::failure("File exceeds 4 MiB.", 2);
        return OperationService::success(QJsonObject{ { "content", encoding == "base64" ? QString::fromLatin1(bytes.toBase64()) : QString::fromUtf8(bytes) },
            { "encoding", encoding }, { "revision", revision(bytes) }, { "path", path } });
    }
    if (instance->isRunning()) return OperationService::failure("Stop the instance before editing files.", 2);
    if (path == QFileInfo(instance->instanceRoot()).canonicalFilePath()) return OperationService::failure("Cannot modify the instance root.", 2);
    if (action == "mkdir") return QDir().mkpath(path) ? OperationService::success() : OperationService::failure("Could not create directory.");
    if (action == "rename") {
        const auto destination = boundedPath(instance, p.value("destination").toString());
        if (destination.isEmpty() || destination == QFileInfo(instance->instanceRoot()).canonicalFilePath() || QFileInfo::exists(destination))
            return OperationService::failure("Destination is invalid or already exists.", 2);
        if (!info.exists() || !QDir().rename(path, destination)) return OperationService::failure("Could not rename file or directory.");
        return OperationService::success(QJsonObject{ { "path", destination } });
    }
    if (action == "remove") {
        if (!p.value("confirm").toBool()) return OperationService::failure("Removal requires confirm=true.", 2);
        // Empty directories only; bulk deletion must enumerate explicit files.
        const bool removed = info.isDir() ? QDir().rmdir(path) : QFile::remove(path);
        return removed ? OperationService::success() : OperationService::failure("Could not remove file or empty directory.");
    }
    auto bytes = p.value("content").toString().toUtf8();
    if (encoding == "base64") {
        const auto decoded = QByteArray::fromBase64Encoding(bytes, QByteArray::AbortOnBase64DecodingErrors);
        if (!decoded) return OperationService::failure("Invalid base64 content.", 2);
        bytes = decoded.decoded;
    }
    if (bytes.size() > 4 * 1024 * 1024) return OperationService::failure("Content exceeds 4 MiB.", 2);
    if (info.exists()) {
        QFile existing(path);
        if (!existing.open(QIODevice::ReadOnly) || existing.size() > 4 * 1024 * 1024) return OperationService::failure("Cannot read existing file.");
        if (p.value("revision").toString() != revision(existing.readAll())) return OperationService::failure("File changed; read its current revision before writing.", 2);
    } else if (!p.value("revision").toString().isEmpty()) return OperationService::failure("File no longer exists.", 2);
    if (!QDir().mkpath(info.absolutePath())) return OperationService::failure("Could not create parent directory.");
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) return OperationService::failure(file.errorString());
    return OperationService::success(QJsonObject{ { "path", path }, { "revision", revision(bytes) } });
}
}

void registerLauncherApiFiles(LauncherApi& api)
{
    using namespace ApiSupport;
    auto panels = std::make_shared<QHash<QString, std::shared_ptr<CustomUiRuntime>>>();
    api.registerOperation({ "instance.custom-ui.open", "Load JSON and bounded QuickJS panels into a session without creating widgets. Scripts can modify instance files/settings.",
        schema({ { "instance", string("Installed Minecraft instance ID.") } }, { "instance" }), "files", true },
        [panels](const QJsonObject& p, UserInteraction&) {
            auto instance = dynamic_cast<MinecraftInstance*>(APPLICATION->instances()->getInstanceById(p.value("instance").toString()));
            if (!instance) return OperationService::failure("Minecraft instance not found.", 2);
            if (instance->isRunning()) return OperationService::failure("Stop the instance before loading panel scripts.", 2);
            for (auto it = panels->begin(); it != panels->end(); ++it) {
                if (it.value()->instance() == instance) {
                    auto result = it.value()->snapshot(); result.insert("sessionId", it.key());
                    return OperationService::success(result);
                }
            }
            if (panels->size() >= 16) return OperationService::failure("Close an existing custom UI session first.", 2);
            auto runtime = std::make_shared<CustomUiRuntime>(instance);
            runtime->load();
            const auto id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            panels->insert(id, runtime);
            auto result = runtime->snapshot(); result.insert("sessionId", id);
            return OperationService::success(result);
        });
    for (const QString action : { "snapshot", "trigger", "call", "activate-variant", "save", "reload", "close" }) {
        QJsonObject properties{ { "sessionId", string("Session returned by instance.custom-ui.open.") } };
        QJsonArray required{ "sessionId" };
        if (action == "call") {
            properties.insert("method", string("Existing launcher bridge method, such as listMods or fs.readFile."));
            properties.insert("arguments", QJsonObject{ { "type", "array" }, { "maxItems", 16 } });
            required.append("method"); required.append("arguments");
        }
        if (action == "trigger") {
            properties.insert("tab", QJsonObject{ { "type", "integer" }, { "minimum", 0 } });
            properties.insert("control", QJsonObject{ { "type", "integer" }, { "minimum", 0 } });
            properties.insert("value", QJsonObject{});
            required.append("tab"); required.append("control"); required.append("value");
        }
        if (action == "activate-variant") {
            properties.insert("group", string("Variant group name.")); properties.insert("option", string("Variant option name."));
            required.append("group"); required.append("option");
        }
        api.registerOperation({ "instance.custom-ui." + action, "Custom panel session: " + action + ".", schema(properties, required), "files", action != "snapshot" },
            [panels, action](const QJsonObject& p, UserInteraction&) {
                const auto id = p.value("sessionId").toString();
                const auto runtime = panels->value(id);
                if (!runtime) return OperationService::failure("Custom UI session not found.", 2);
                if (action == "close") { panels->remove(id); return OperationService::success(); }
                if (!runtime->valid()) { panels->remove(id); return OperationService::failure("The instance was removed.", 2); }
                bool ok = true;
                if (action == "call") {
                    const auto value = runtime->bridge(p.value("method").toString(), p.value("arguments").toArray(), ok);
                    if (!ok) return OperationService::failure("Custom UI bridge call failed or method is unavailable.", 2);
                    return OperationService::success(QJsonObject{ { "value", value }, { "snapshot", runtime->snapshot() } });
                }
                if (action == "trigger") {
                    const auto tabs = runtime->snapshot().value("tabs").toArray();
                    const int tab = p.value("tab").toInt(-1), control = p.value("control").toInt(-1);
                    if (tab < 0 || tab >= tabs.size()) return OperationService::failure("Tab index is out of range.", 2);
                    const auto controls = tabs[tab].toObject().value("controls").toArray();
                    if (control < 0 || control >= controls.size()) return OperationService::failure("Control index is out of range.", 2);
                    ok = runtime->trigger(controls[control].toObject(), p.value("value"));
                } else if (action == "activate-variant") ok = runtime->activate(p.value("group").toString(), p.value("option").toString());
                else if (action == "save") ok = runtime->persist();
                else if (action == "reload") ok = runtime->load();
                auto result = runtime->snapshot(); result.insert("sessionId", id); result.insert("complete", ok);
                return OperationService::success(result);
            });
    }
    api.registerOperation({ "instance.custom-ui.state", "Read persisted lunaui state and its revision.",
        schema({ { "instance", string("Installed instance ID.") } }, { "instance" }), "files" },
        [](const QJsonObject& p, UserInteraction&) {
            QJsonObject request{ { "instance", p.value("instance") }, { "path", "lunaui/state.json" } };
            const auto stat = fileOperation("stat", request);
            if (!stat.value("ok").toBool()) return stat;
            if (!stat.value("data").toObject().value("exists").toBool())
                return OperationService::success(QJsonObject{ { "state", QJsonObject{} }, { "revision", "" } });
            const auto read = fileOperation("read", request);
            if (!read.value("ok").toBool()) return read;
            const auto data = read.value("data").toObject();
            const auto document = QJsonDocument::fromJson(data.value("content").toString().toUtf8());
            if (!document.isObject()) return OperationService::failure("Custom UI state is not a JSON object.", 2);
            return OperationService::success(QJsonObject{ { "state", document.object() }, { "revision", data.value("revision") } });
        });
    api.registerOperation({ "instance.custom-ui.save-state", "Atomically save lunaui state after checking its revision.",
        schema({ { "instance", string("Installed instance ID.") }, { "state", QJsonObject{ { "type", "object" } } },
            { "revision", string("Revision from instance.custom-ui.state.") } }, { "instance", "state", "revision" }), "files", true },
        [](const QJsonObject& p, UserInteraction&) {
            return fileOperation("write", { { "instance", p.value("instance") }, { "path", "lunaui/state.json" },
                { "revision", p.value("revision") }, { "content", QString::fromUtf8(QJsonDocument(p.value("state").toObject()).toJson()) } });
        });
    for (const QString action : { "stat", "list", "read", "write", "mkdir", "remove", "rename" }) {
        QJsonObject properties{ { "instance", string("Installed instance ID.") }, { "path", string("Path relative to the instance root.") } };
        QJsonArray required{ "instance", "path" };
        if (action == "read" || action == "write") properties.insert("encoding", string("utf8 (default) or base64 for binary files."));
        if (action == "rename") { properties.insert("destination", string("New path relative to the instance root; must not exist.")); required.append("destination"); }
        if (action == "write") {
            properties.insert("content", string("UTF-8 file content, at most 4 MiB."));
            properties.insert("revision", string("SHA256 revision returned by read; required when overwriting.")); required.append("content");
        }
        if (action == "remove") { properties.insert("confirm", boolean()); required.append("confirm"); }
        api.registerOperation({ "instance.file." + action, "Instance file operation: " + action + ".", schema(properties, required), "files",
            action == "write" || action == "remove" || action == "mkdir" || action == "rename" },
            [action](const QJsonObject& p, UserInteraction&) { return fileOperation(action, p); });
    }
}
