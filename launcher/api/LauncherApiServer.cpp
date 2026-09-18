// SPDX-License-Identifier: GPL-3.0-only

#include "LauncherApiServer.h"

#include "Application.h"
#include "InstanceList.h"
#include "api/LauncherApi.h"
#include "cli/OperationService.h"
#include "server/PropertiesFile.h"
#include "server/ServerInstance.h"
#include "server/ServerLaunchTask.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTextStream>

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

ServerInstance* findServer(const QString& reference)
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
    return dynamic_cast<ServerInstance*>(instance);
}

QJsonObject missingServer(const QString& reference)
{
    return OperationService::failure(QObject::tr("Server instance not found: %1").arg(reference), 2);
}

bool writable(ServerInstance* instance, QString* error)
{
    if (!instance)
        return false;
    if (instance->isRunning()) {
        if (error)
            *error = QObject::tr("Server files cannot be modified while the server is running.");
        return false;
    }
    return true;
}

QString listFileName(QString kind)
{
    kind = kind.trimmed().toLower();
    if (kind == "whitelist" || kind == "white-list")
        return "whitelist.json";
    if (kind == "ops" || kind == "operators" || kind == "op")
        return "ops.json";
    if (kind == "banned-players" || kind == "bannedplayers" || kind == "players")
        return "banned-players.json";
    if (kind == "banned-ips" || kind == "bannedips" || kind == "ips")
        return "banned-ips.json";
    return {};
}

QString listEntryKey(const QJsonObject& entry, const QString& fileName)
{
    return fileName == "banned-ips.json" ? entry.value("ip").toString() : entry.value("name").toString();
}

QJsonArray loadList(const QString& path, bool* exists, QString* error)
{
    const QFileInfo info(path);
    if (exists)
        *exists = info.exists();
    if (!info.exists())
        return {};
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return {};
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (!document.isArray()) {
        if (error)
            *error = parseError.error == QJsonParseError::NoError ? QObject::tr("Expected a JSON array.")
                                                                   : parseError.errorString();
        return {};
    }
    return document.array();
}

bool saveList(const QString& path, const QJsonArray& entries, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    const auto bytes = QJsonDocument(entries).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

QJsonObject readProperties(const QJsonObject& parameters)
{
    const auto reference = parameters.value("instance").toString();
    auto instance = findServer(reference);
    if (!instance)
        return missingServer(reference);
    const auto path = QDir(instance->instanceRoot()).filePath("server.properties");
    PropertiesFile properties;
    const bool exists = QFileInfo::exists(path);
    if (exists && !properties.loadFile(path))
        return OperationService::failure(QObject::tr("Could not read server.properties."));
    QJsonObject values;
    for (auto it = properties.constBegin(); it != properties.constEnd(); ++it)
        values.insert(it.key(), it.value());
    return OperationService::success(QJsonObject{ { "instance", instance->id() },
                                                  { "path", path },
                                                  { "exists", exists },
                                                  { "properties", values } });
}

QJsonObject writeProperties(const QJsonObject& parameters)
{
    const auto reference = parameters.value("instance").toString();
    auto instance = findServer(reference);
    if (!instance)
        return missingServer(reference);
    QString error;
    if (!writable(instance, &error))
        return OperationService::failure(error, 2);
    const auto values = parameters.value("properties");
    if (!values.isObject())
        return OperationService::failure(QObject::tr("properties must be a JSON object."), 2);
    PropertiesFile properties;
    const auto valueObject = values.toObject();
    for (auto it = valueObject.constBegin(); it != valueObject.constEnd(); ++it) {
        if (!it.value().isString())
            return OperationService::failure(QObject::tr("Property values must be strings: %1").arg(it.key()), 2);
        properties.insert(it.key(), it.value().toString());
    }
    const auto path = QDir(instance->instanceRoot()).filePath("server.properties");
    if (!properties.saveFile(path))
        return OperationService::failure(QObject::tr("Could not write server.properties."));
    return OperationService::success(QJsonObject{ { "instance", instance->id() }, { "path", path }, { "changed", true } });
}

QJsonObject readEula(const QJsonObject& parameters)
{
    const auto reference = parameters.value("instance").toString();
    auto instance = findServer(reference);
    if (!instance)
        return missingServer(reference);
    const auto path = QDir(instance->instanceRoot()).filePath("eula.txt");
    QFile file(path);
    QString content;
    if (file.exists()) {
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return OperationService::failure(QObject::tr("Could not read eula.txt."));
        content = QString::fromUtf8(file.readAll());
    }
    bool agreed = false;
    for (const auto& line : content.split('\n')) {
        const auto trimmed = line.trimmed();
        if (trimmed.startsWith("eula=", Qt::CaseInsensitive))
            agreed = trimmed.mid(5).trimmed().compare("true", Qt::CaseInsensitive) == 0;
    }
    return OperationService::success(QJsonObject{ { "instance", instance->id() },
                                                  { "path", path },
                                                  { "exists", file.exists() },
                                                  { "agreed", agreed },
                                                  { "content", content } });
}

QJsonObject writeEula(const QJsonObject& parameters)
{
    const auto reference = parameters.value("instance").toString();
    auto instance = findServer(reference);
    if (!instance)
        return missingServer(reference);
    QString error;
    if (!writable(instance, &error))
        return OperationService::failure(error, 2);
    if (!parameters.value("agreed").isBool())
        return OperationService::failure(QObject::tr("agreed must be a boolean."), 2);
    const auto path = QDir(instance->instanceRoot()).filePath("eula.txt");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return OperationService::failure(QObject::tr("Could not write eula.txt: %1").arg(file.errorString()));
    QTextStream out(&file);
    out << "#By changing the setting below to TRUE you are indicating your agreement to our EULA (https://aka.ms/MinecraftEULA).\n";
    out << "#Generated by Prism Launcher\n";
    out << "eula=" << (parameters.value("agreed").toBool() ? "true" : "false") << "\n";
    return OperationService::success(QJsonObject{ { "instance", instance->id() }, { "path", path }, { "agreed", parameters.value("agreed").toBool() }, { "changed", true } });
}

QJsonObject listRead(const QJsonObject& parameters)
{
    const auto reference = parameters.value("instance").toString();
    auto instance = findServer(reference);
    if (!instance)
        return missingServer(reference);
    const auto kind = parameters.value("kind").toString().trimmed().toLower();
    const auto fileName = listFileName(kind);
    if (fileName.isEmpty())
        return OperationService::failure(QObject::tr("Unknown server list: %1").arg(kind), 2);
    const auto path = QDir(instance->instanceRoot()).filePath(fileName);
    bool exists = false;
    QString error;
    const auto entries = loadList(path, &exists, &error);
    if (!error.isEmpty())
        return OperationService::failure(error);
    return OperationService::success(QJsonObject{ { "instance", instance->id() },
                                                  { "kind", kind },
                                                  { "path", path },
                                                  { "exists", exists },
                                                  { "entries", entries } });
}

QJsonObject listWrite(const QJsonObject& parameters)
{
    const auto reference = parameters.value("instance").toString();
    auto instance = findServer(reference);
    if (!instance)
        return missingServer(reference);
    QString error;
    if (!writable(instance, &error))
        return OperationService::failure(error, 2);
    const auto kind = parameters.value("kind").toString().trimmed().toLower();
    const auto fileName = listFileName(kind);
    if (fileName.isEmpty())
        return OperationService::failure(QObject::tr("Unknown server list: %1").arg(kind), 2);
    if (!parameters.value("entries").isArray())
        return OperationService::failure(QObject::tr("entries must be a JSON array."), 2);
    const auto entries = parameters.value("entries").toArray();
    for (const auto& value : entries) {
        if (!value.isObject() || listEntryKey(value.toObject(), fileName).trimmed().isEmpty())
            return OperationService::failure(QObject::tr("Each list entry must be an object with a non-empty name or ip."), 2);
    }
    const auto path = QDir(instance->instanceRoot()).filePath(fileName);
    if (!saveList(path, entries, &error))
        return OperationService::failure(QObject::tr("Could not write %1: %2").arg(fileName, error));
    return OperationService::success(QJsonObject{ { "instance", instance->id() }, { "kind", kind }, { "path", path }, { "changed", true } });
}

QJsonObject listAdd(const QJsonObject& parameters)
{
    const auto reference = parameters.value("instance").toString();
    auto instance = findServer(reference);
    if (!instance)
        return missingServer(reference);
    QString error;
    if (!writable(instance, &error))
        return OperationService::failure(error, 2);
    auto kind = parameters.value("kind").toString().trimmed().toLower();
    const auto fileName = listFileName(kind);
    if (fileName.isEmpty())
        return OperationService::failure(QObject::tr("Unknown server list: %1").arg(kind), 2);
    const auto entryValue = parameters.value("entry").toString().trimmed();
    if (entryValue.isEmpty())
        return OperationService::failure(QObject::tr("entry must not be empty."), 2);
    const auto path = QDir(instance->instanceRoot()).filePath(fileName);
    bool exists = false;
    auto entries = loadList(path, &exists, &error);
    if (!error.isEmpty())
        return OperationService::failure(error);
    for (const auto& value : entries) {
        if (value.isObject() && listEntryKey(value.toObject(), fileName).compare(entryValue, Qt::CaseInsensitive) == 0)
            return OperationService::success(QJsonObject{ { "added", false }, { "alreadyPresent", true }, { "entry", entryValue } });
    }
    QJsonObject object;
    if (kind == "banned-ips" || kind == "bannedips" || kind == "ips") {
        object = { { "ip", entryValue }, { "created", QDateTime::currentDateTimeUtc().toString(Qt::ISODate) },
                   { "source", "Prism Launcher" }, { "expires", "forever" }, { "reason", "Banned by an operator" } };
    } else {
        object = { { "name", entryValue }, { "uuid", "00000000-0000-0000-0000-000000000000" } };
        if (kind == "ops" || kind == "operators" || kind == "op")
            object.insert("level", parameters.value("level").toInt(4)), object.insert("bypassesPlayerLimit", parameters.value("bypassesPlayerLimit").toBool(false));
        else if (kind == "banned-players" || kind == "bannedplayers" || kind == "players")
            object.insert("created", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)), object.insert("source", "Prism Launcher"), object.insert("expires", "forever"), object.insert("reason", "Banned by an operator");
    }
    entries.append(object);
    if (!saveList(path, entries, &error))
        return OperationService::failure(error);
    return OperationService::success(QJsonObject{ { "added", true }, { "entry", object } });
}

QJsonObject listRemove(const QJsonObject& parameters)
{
    if (!parameters.value("confirm").toBool())
        return OperationService::failure(QObject::tr("Removing a server list entry requires confirm=true."), 2);
    const auto reference = parameters.value("instance").toString();
    auto instance = findServer(reference);
    if (!instance)
        return missingServer(reference);
    QString error;
    if (!writable(instance, &error))
        return OperationService::failure(error, 2);
    const auto kind = parameters.value("kind").toString().trimmed().toLower();
    const auto fileName = listFileName(kind);
    if (fileName.isEmpty())
        return OperationService::failure(QObject::tr("Unknown server list: %1").arg(kind), 2);
    const auto entryValue = parameters.value("entry").toString().trimmed();
    const auto path = QDir(instance->instanceRoot()).filePath(fileName);
    bool exists = false;
    auto entries = loadList(path, &exists, &error);
    if (!error.isEmpty())
        return OperationService::failure(error);
    QJsonArray remaining;
    bool removed = false;
    for (const auto& value : entries) {
        if (value.isObject() && listEntryKey(value.toObject(), fileName).compare(entryValue, Qt::CaseInsensitive) == 0)
            removed = true;
        else
            remaining.append(value);
    }
    if (removed && !saveList(path, remaining, &error))
        return OperationService::failure(error);
    return OperationService::success(QJsonObject{ { "removed", removed }, { "entry", entryValue } });
}

QJsonObject consoleCommand(const QJsonObject& parameters)
{
    const auto reference = parameters.value("instance").toString();
    auto instance = findServer(reference);
    if (!instance)
        return missingServer(reference);
    const auto command = parameters.value("command").toString();
    if (command.trimmed().isEmpty())
        return OperationService::failure(QObject::tr("command must not be empty."), 2);
    if (!instance->isRunning())
        return OperationService::failure(QObject::tr("The server is not running."), 2);
    auto task = dynamic_cast<ServerLaunchTask*>(instance->launchTask());
    if (!task || !task->canStop())
        return OperationService::failure(QObject::tr("The server console is not available."), 2);
    auto payload = command.toUtf8();
    payload.append('\n');
    task->writeToStdin(payload);
    return OperationService::success(QJsonObject{ { "instance", instance->id() }, { "sent", true }, { "command", command } });
}

}  // namespace

void registerLauncherApiServerOperations(LauncherApi& api)
{
    const auto instance = stringProperty("Server instance ID, managed name, or display name.");
    const auto kind = stringProperty("whitelist, ops, banned-players, or banned-ips.");
    const QJsonObject propertiesObject{ { "type", "object" }, { "additionalProperties", QJsonObject{ { "type", "string" } } } };
    const QJsonObject entriesArray{ { "type", "array" }, { "items", QJsonObject{ { "type", "object" } } } };

    api.registerOperation({ "server.properties.read", "Read server.properties from a server instance.", objectSchema({ { "instance", instance } }, { "instance" }), "server" },
                           [](const QJsonObject& p, UserInteraction&) { return readProperties(p); });
    api.registerOperation({ "server.properties.write", "Replace server.properties values.", objectSchema({ { "instance", instance }, { "properties", propertiesObject } }, { "instance", "properties" }), "server", true },
                           [](const QJsonObject& p, UserInteraction&) { return writeProperties(p); });
    api.registerOperation({ "server.eula.read", "Read the server EULA agreement state and file content.", objectSchema({ { "instance", instance } }, { "instance" }), "server" },
                           [](const QJsonObject& p, UserInteraction&) { return readEula(p); });
    api.registerOperation({ "server.eula.write", "Set the server EULA agreement state.", objectSchema({ { "instance", instance }, { "agreed", boolProperty("Whether eula=true should be written.") } }, { "instance", "agreed" }), "server", true },
                           [](const QJsonObject& p, UserInteraction&) { return writeEula(p); });
    api.registerOperation({ "server.list.read", "Read a server operator, whitelist, or ban list.", objectSchema({ { "instance", instance }, { "kind", kind } }, { "instance", "kind" }), "server" },
                           [](const QJsonObject& p, UserInteraction&) { return listRead(p); });
    api.registerOperation({ "server.list.write", "Replace a server operator, whitelist, or ban list.", objectSchema({ { "instance", instance }, { "kind", kind }, { "entries", entriesArray } }, { "instance", "kind", "entries" }), "server", true },
                           [](const QJsonObject& p, UserInteraction&) { return listWrite(p); });
    api.registerOperation({ "server.list.add", "Add a player, operator, or IP to a server list.", objectSchema({ { "instance", instance }, { "kind", kind }, { "entry", stringProperty("Player name or IP address.") }, { "level", QJsonObject{ { "type", "integer" }, { "default", 4 } } }, { "bypassesPlayerLimit", boolProperty("Operator bypasses the player limit.") } }, { "instance", "kind", "entry" }), "server", true },
                           [](const QJsonObject& p, UserInteraction&) { return listAdd(p); });
    api.registerOperation({ "server.list.remove", "Remove a player, operator, or IP from a server list.", objectSchema({ { "instance", instance }, { "kind", kind }, { "entry", stringProperty("Player name or IP address.") }, { "confirm", boolProperty("Confirm removal.") } }, { "instance", "kind", "entry", "confirm" }), "server", true },
                           [](const QJsonObject& p, UserInteraction&) { return listRemove(p); });
    api.registerOperation({ "server.console.command", "Send one command to a running server console.", objectSchema({ { "instance", instance }, { "command", stringProperty("Command without the trailing newline.") } }, { "instance", "command" }), "server" },
                           [](const QJsonObject& p, UserInteraction&) { return consoleCommand(p); });
}
