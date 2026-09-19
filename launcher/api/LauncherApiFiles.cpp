// SPDX-License-Identifier: GPL-3.0-only
#include "LauncherApiFiles.h"
#include "api/LauncherApiSupport.h"
#include "Application.h"
#include "InstanceList.h"
#include "FileSystem.h"
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
        return OperationService::success(QJsonObject{ { "content", QString::fromUtf8(bytes) }, { "revision", revision(bytes) }, { "path", path } });
    }
    if (instance->isRunning()) return OperationService::failure("Stop the instance before editing files.", 2);
    if (path == QFileInfo(instance->instanceRoot()).canonicalFilePath()) return OperationService::failure("Cannot modify the instance root.", 2);
    if (action == "mkdir") return QDir().mkpath(path) ? OperationService::success() : OperationService::failure("Could not create directory.");
    if (action == "remove") {
        if (!p.value("confirm").toBool()) return OperationService::failure("Removal requires confirm=true.", 2);
        // Empty directories only; bulk deletion must enumerate explicit files.
        const bool removed = info.isDir() ? QDir().rmdir(path) : QFile::remove(path);
        return removed ? OperationService::success() : OperationService::failure("Could not remove file or empty directory.");
    }
    const auto bytes = p.value("content").toString().toUtf8();
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
    for (const QString action : { "stat", "list", "read", "write", "mkdir", "remove" }) {
        QJsonObject properties{ { "instance", string("Installed instance ID.") }, { "path", string("Path relative to the instance root.") } };
        QJsonArray required{ "instance", "path" };
        if (action == "write") {
            properties.insert("content", string("UTF-8 file content, at most 4 MiB."));
            properties.insert("revision", string("SHA256 revision returned by read; required when overwriting.")); required.append("content");
        }
        if (action == "remove") { properties.insert("confirm", boolean()); required.append("confirm"); }
        api.registerOperation({ "instance.file." + action, "Instance file operation: " + action + ".", schema(properties, required), "files",
            action == "write" || action == "remove" || action == "mkdir" },
            [action](const QJsonObject& p, UserInteraction&) { return fileOperation(action, p); });
    }
}
