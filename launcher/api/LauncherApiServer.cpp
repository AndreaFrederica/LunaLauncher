// SPDX-License-Identifier: GPL-3.0-only

#include "LauncherApiServer.h"

#include "Application.h"
#include "InstanceList.h"
#include "api/LauncherApi.h"
#include "api/LauncherApiSupport.h"
#include "cli/OperationService.h"
#include "server/PropertiesFile.h"
#include "server/ServerInstance.h"
#include "server/ServerLaunchTask.h"
#include "net/ApiDownload.h"
#include "net/NetJob.h"

#include <QDateTime>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTextStream>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QUrl>
#include <QRegularExpression>

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

QJsonObject installDistribution(LauncherApi& api, const QJsonObject& p, UserInteraction& interaction)
{
    auto instance = findServer(p.value("instance").toString());
    if (!instance) return missingServer(p.value("instance").toString());
    QString error;
    if (!writable(instance, &error)) return OperationService::failure(error, 2);
    const QUrl url(p.value("url").toString());
    const auto fileName = p.value("fileName").toString().trimmed();
    if (!url.isValid() || (url.scheme() != "https" && url.scheme() != "http")) return OperationService::failure("url must be HTTP(S).", 2);
    static const QRegularExpression unsafe(R"([<>:"/\\|?*\x00-\x1f])");
    if (fileName.isEmpty() || fileName == "." || fileName == ".." || unsafe.match(fileName).hasMatch() || fileName.endsWith('.'))
        return OperationService::failure("fileName is unsafe.", 2);
    const auto destination = QDir(instance->instanceRoot()).filePath(fileName);
    if (QFileInfo::exists(destination) && !p.value("replace").toBool()) return OperationService::failure("The server file exists; set replace=true.", 2);
    QTemporaryDir staging(QDir(instance->instanceRoot()).filePath(".api-server-XXXXXX"));
    if (!staging.isValid()) return OperationService::failure("Could not create a staging directory.");
    const auto staged = QDir(staging.path()).filePath(fileName);
    auto job = makeShared<NetJob>("Server distribution download", APPLICATION->network());
    job->addNetAction(Net::ApiDownload::makeFile(url, staged));
    if (!ApiSupport::wait(api, job, interaction, error)) return OperationService::failure(error);
    if (!QFileInfo(staged).isFile() || QFileInfo(staged).size() == 0) return OperationService::failure("The downloaded server file is empty.");
    if (QFileInfo::exists(destination) && !QFile::remove(destination)) return OperationService::failure("The existing server file could not be replaced.");
    if (!QFile::rename(staged, destination)) return OperationService::failure("The downloaded server file could not be installed.");
    const auto args = p.value("arguments").toArray();
    QStringList launchArgs;
    for (const auto& arg : args) launchArgs.append(arg.toString());
    if (fileName.endsWith(".jar", Qt::CaseInsensitive)) {
        launchArgs.prepend(fileName);
        launchArgs.prepend("-jar");
        instance->setExecutablePath("$java");
        instance->setArguments(launchArgs);
    } else {
        instance->setExecutablePath(fileName);
        instance->setArguments(launchArgs);
    }
    if (p.contains("minecraftVersion")) instance->setMinecraftVersion(p.value("minecraftVersion").toString());
    instance->saveNow();
    return OperationService::success(QJsonObject{ { "instance", instance->id() }, { "path", destination },
        { "fileName", fileName }, { "url", url.toString() }, { "configured", true }, { "changed", true } });
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

QJsonObject yamlFile(const QJsonObject& p, bool write)
{
    const auto instance = findServer(p.value("instance").toString());
    if (!instance) return missingServer(p.value("instance").toString());
    const auto name = p.value("file").toString();
    if (name != "bukkit.yml" && name != "spigot.yml")
        return OperationService::failure("file must be bukkit.yml or spigot.yml.", 2);
    const auto path = QDir(instance->instanceRoot()).filePath(name);
    if (QFileInfo(path).isSymLink()) return OperationService::failure("Symbolic config files are not supported.", 2);
    QFile file(path);
    constexpr qint64 maxSize = 1024 * 1024;
    QByteArray content;
    const bool exists = file.exists();
    if (exists) {
        if (!file.open(QIODevice::ReadOnly)) return OperationService::failure(file.errorString());
        if (file.size() > maxSize) return OperationService::failure("YAML file exceeds 1 MiB.", 2);
        content = file.readAll();
        if (file.error() != QFileDevice::NoError) return OperationService::failure(file.errorString());
        file.close();
    }
    auto revision = QString::fromLatin1(QCryptographicHash::hash(content, QCryptographicHash::Sha256).toHex());
    if (write) {
        QString error;
        if (!writable(instance, &error)) return OperationService::failure(error, 2);
        if (!p.value("content").isString()) return OperationService::failure("content must be a string.", 2);
        if (p.contains("ifRevision") && p.value("ifRevision").toString() != revision)
            return OperationService::failure("The config changed since it was read. Read it again before saving.", 2);
        content = p.value("content").toString().toUtf8();
        if (content.size() > maxSize || content.contains('\0')) return OperationService::failure("Invalid YAML content or size exceeds 1 MiB.", 2);
        QSaveFile output(path);
        if (!output.open(QIODevice::WriteOnly) || output.write(content) != content.size() || !output.commit())
            return OperationService::failure(output.errorString());
        revision = QString::fromLatin1(QCryptographicHash::hash(content, QCryptographicHash::Sha256).toHex());
    }
    return OperationService::success(QJsonObject{ { "instance", instance->id() }, { "file", name }, { "exists", write || exists },
        { "content", QString::fromUtf8(content) }, { "revision", revision } });
}

QJsonObject loaderConfig(const QJsonObject& p, bool write)
{
    const auto instance = findServer(p.value("instance").toString());
    if (!instance) return missingServer(p.value("instance").toString());
    using namespace ApiSupport;
    auto mods = instance->getModLoaderTypes();
    auto plugins = instance->getPluginLoaderTypes();
    auto version = instance->getMinecraftVersion();
    if (write) {
        QString error;
        if (!writable(instance, &error)) return OperationService::failure(error, 2);
        if (p.contains("minecraftVersion")) {
            version = p.value("minecraftVersion").toString();
            if (!identifier(version)) return OperationService::failure("Invalid Minecraft version.", 2);
        }
        if (p.contains("loaders") && !parseFlags(p.value("loaders"), modLoaders(), mods))
            return OperationService::failure("Unknown mod loader or invalid loaders array.", 2);
        if (p.contains("pluginLoaders") && !parseFlags(p.value("pluginLoaders"), pluginLoaders(), plugins))
            return OperationService::failure("Unknown plugin loader or invalid pluginLoaders array.", 2);
        instance->setMinecraftVersion(version);
        instance->setModLoaderTypes(mods);
        instance->setPluginLoaderTypes(plugins);
        instance->saveNow();
    }
    return OperationService::success(QJsonObject{ { "instance", instance->id() }, { "minecraftVersion", version },
        { "loaders", flagNames(mods, modLoaders()) }, { "pluginLoaders", flagNames(plugins, pluginLoaders()) },
        { "installsServerSoftware", false } });
}

}  // namespace

void registerLauncherApiServerOperations(LauncherApi& api)
{
    const auto instance = stringProperty("Server instance ID, managed name, or display name.");
    const auto kind = stringProperty("whitelist, ops, banned-players, or banned-ips.");
    const QJsonObject propertiesObject{ { "type", "object" }, { "additionalProperties", QJsonObject{ { "type", "string" } } } };
    const QJsonObject entriesArray{ { "type", "array" }, { "items", QJsonObject{ { "type", "object" } } } };

    api.registerOperation({ "server.distribution.catalog", "Describe server distribution providers and stable catalog endpoints for a replacement UI.",
        objectSchema({ { "provider", stringProperty("Optional provider filter: vanilla, paper, purpur, fabric, forge.") } }), "server" },
        [](const QJsonObject& p, UserInteraction&) {
            const auto requested = p.value("provider").toString().trimmed().toLower();
            QJsonArray providers;
            const auto add = [&providers, &requested](const QString& id, const QString& name, const QString& manifest, const QString& download) {
                if (!requested.isEmpty() && requested != id) return;
                providers.append(QJsonObject{ { "id", id }, { "name", name }, { "manifest", manifest }, { "downloadTemplate", download } });
            };
            add("vanilla", "Minecraft Vanilla", "https://piston-meta.mojang.com/mc/game/version_manifest_v2.json", "version.server.url");
            add("paper", "Paper", "https://api.papermc.io/v2/projects/paper", "https://api.papermc.io/v2/projects/paper/versions/{version}/builds/{build}/downloads/{name}");
            add("purpur", "Purpur", "https://api.purpurmc.org/v2/purpur", "https://api.purpurmc.org/v2/purpur/{version}/{build}/download");
            add("fabric", "Fabric Loader", "https://meta.fabricmc.net/v2/versions/loader", "https://meta.fabricmc.net/v2/versions/loader/{mc}/{loader}/1.0.0/server/jar");
            add("forge", "Forge", "https://files.minecraftforge.net/net/minecraftforge/forge/promotions_slim.json", "installer-url-from-manifest");
            if (providers.isEmpty()) return OperationService::failure("Unknown server distribution provider.", 2);
            return OperationService::success(QJsonObject{ { "providers", providers }, { "stable", true } });
        });

    api.registerOperation({ "server.distribution.install", "Download and configure a server distribution from a verified HTTP(S) URL. The file is staged before replacement.",
        objectSchema({ { "instance", instance }, { "url", stringProperty("HTTP(S) distribution URL.") },
            { "fileName", stringProperty("Destination filename inside the server instance.") }, { "replace", boolProperty("Replace an existing file.") },
            { "arguments", QJsonObject{ { "type", "array" }, { "items", stringProperty("Additional server arguments.") } } },
            { "minecraftVersion", stringProperty("Optional Minecraft version recorded on the server instance.") } }, { "instance", "url", "fileName" }), "server", true },
        [&api](const QJsonObject& p, UserInteraction& i) { return installDistribution(api, p, i); });

    api.registerOperation({ "server.start", "Start a configured server process without requiring a Minecraft account.",
        objectSchema({ { "instance", instance } }, { "instance" }), "server", true },
        [](const QJsonObject& p, UserInteraction&) {
            const auto server = findServer(p.value("instance").toString());
            if (!server) return missingServer(p.value("instance").toString());
            if (!server->isRunning() && !server->startServer()) return OperationService::failure("The server could not be started.");
            return OperationService::success(QJsonObject{ { "instance", server->id() }, { "running", server->isRunning() } });
        });
    api.registerOperation({ "server.console.write", "Write UTF-8 terminal input, including control characters, to the server PTY.",
        objectSchema({ { "instance", instance }, { "text", stringProperty("Terminal input of at most 32 KiB UTF-8; no newline is added.") } }, { "instance", "text" }), "server" },
        [](const QJsonObject& p, UserInteraction&) {
            const auto server = findServer(p.value("instance").toString());
            if (!server) return missingServer(p.value("instance").toString());
            const auto task = dynamic_cast<ServerLaunchTask*>(server->getLaunchTask());
            if (!task || !task->canStop()) return OperationService::failure("The server terminal is not available.", 2);
            const auto text = p.value("text").toString().toUtf8();
            if (text.size() > 32768) return OperationService::failure("Terminal input exceeds 32 KiB.", 2);
            task->writeToStdin(text);
            return OperationService::success(QJsonObject{ { "bytesWritten", text.size() } });
        });
    api.registerOperation({ "server.console.resize", "Resize the running server terminal.",
        objectSchema({ { "instance", instance }, { "columns", QJsonObject{ { "type", "integer" }, { "minimum", 1 }, { "maximum", 1000 } } },
            { "rows", QJsonObject{ { "type", "integer" }, { "minimum", 1 }, { "maximum", 1000 } } } }, { "instance", "columns", "rows" }), "server" },
        [](const QJsonObject& p, UserInteraction&) {
            const auto server = findServer(p.value("instance").toString());
            if (!server) return missingServer(p.value("instance").toString());
            const auto task = dynamic_cast<ServerLaunchTask*>(server->getLaunchTask());
            if (!task || !task->canStop()) return OperationService::failure("The server terminal is not available.", 2);
            task->resizePty(p.value("columns").toInt(), p.value("rows").toInt());
            return OperationService::success();
        });

    auto yaml = QJsonObject{ { "instance", instance }, { "file", stringProperty("bukkit.yml or spigot.yml.") } };
    api.registerOperation({ "server.yaml.read", "Read server YAML text and its revision.", objectSchema(yaml, { "instance", "file" }), "server" },
        [](const QJsonObject& p, UserInteraction&) { return yamlFile(p, false); });
    yaml.insert("content", stringProperty("Full UTF-8 YAML text, at most 1 MiB. Syntax is not validated."));
    yaml.insert("ifRevision", stringProperty("Optional revision from read; rejects a stale write."));
    api.registerOperation({ "server.yaml.write", "Atomically save server YAML text while the server is stopped.", objectSchema(yaml, { "instance", "file", "content" }), "server", true },
        [](const QJsonObject& p, UserInteraction&) { return yamlFile(p, true); });
    api.registerOperation({ "server.loader.read", "Read the server resource compatibility configuration.", objectSchema({ { "instance", instance } }, { "instance" }), "server" },
        [](const QJsonObject& p, UserInteraction&) { return loaderConfig(p, false); });
    api.registerOperation({ "server.loader.write", "Set Minecraft, mod and plugin loader filters. This does not install server software.",
        objectSchema({ { "instance", instance }, { "minecraftVersion", stringProperty("Minecraft version.") },
            { "loaders", ApiSupport::strings() }, { "pluginLoaders", ApiSupport::strings() } }, { "instance" }), "server", true },
        [](const QJsonObject& p, UserInteraction&) { return loaderConfig(p, true); });

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
