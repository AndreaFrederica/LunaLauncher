// SPDX-License-Identifier: GPL-3.0-only

#include "LauncherApiServers.h"

#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QPointer>
#include <QTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include <exception>
#include <sstream>
#include <stdexcept>
#include <utility>

#include <io/stream_reader.h>
#include <tag_compound.h>
#include <tag_list.h>
#include <tag_primitive.h>
#include <tag_string.h>

#include "Application.h"
#include "BaseInstance.h"
#include "GZip.h"
#include "InstanceList.h"
#include "minecraft/launch/MinecraftTarget.h"
#include "ui/pages/instance/ServerPingTask.h"
#include "PSaveFile.h"
#include "LauncherApi.h"
#include "LauncherApiSupport.h"
#include "cli/OperationService.h"

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

BaseInstance* findInstance(const QString& reference)
{
    auto list = APPLICATION->instances();
    auto instance = list->getInstanceById(reference);
    if (!instance) instance = list->getInstanceByManagedName(reference);
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

QString iconString(const nbt::value& value)
{
    if (value.get_type() != nbt::tag_type::String) return {};
    return QString::fromUtf8(value.as<nbt::tag_string>().get());
}

QJsonArray readServerArray(const nbt::tag_compound& root)
{
    QJsonArray result;
    try {
        const auto& tag = root.at("servers");
        if (tag.get_type() != nbt::tag_type::List) return result;
        const auto& list = tag.as<nbt::tag_list>();
        for (size_t i = 0; i < list.size(); ++i) {
            const auto& item = list.at(i);
            if (item.get_type() != nbt::tag_type::Compound) continue;
            const auto& entry = item.as<nbt::tag_compound>();
            QString name, ip, icon;
            try { name = QString::fromUtf8(entry.at("name").as<nbt::tag_string>().get()); } catch (const std::exception&) {}
            try { ip = QString::fromUtf8(entry.at("ip").as<nbt::tag_string>().get()); } catch (const std::exception&) {}
            try { icon = iconString(entry.at("icon")); } catch (const std::exception&) {}
            bool acceptTextures = false;
            try { acceptTextures = entry.at("acceptTextures").as<nbt::tag_byte>().get() != 0; } catch (const std::exception&) {}
            result.append(QJsonObject{ { "name", name }, { "ip", ip }, { "icon", icon }, { "acceptTextures", acceptTextures } });
        }
    } catch (const std::exception&) {
    }
    return result;
}

QString readServers(const QJsonObject& parameters, UserInteraction&, QJsonObject& result)
{
    const auto reference = parameters.value("instance").toString();
    auto instance = findInstance(reference);
    if (!instance) return "The instance does not exist.";
    const auto path = QDir(instance->gameRoot()).filePath("servers.dat");
    result["instance"] = instance->id();
    result["path"] = path;
    if (!QFileInfo::exists(path)) {
        result["exists"] = false;
        result["servers"] = QJsonArray{};
        result["revision"] = "";
        return {};
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return file.errorString();
    const auto compressed = file.readAll();
    QByteArray data;
    const bool gzipped = GZip::unzip(compressed, data);
    // Some launchers and mods write servers.dat as plain NBT instead of the
    // vanilla gzip container. Accept both forms so the quick-connect panel can
    // still read the list.
    if (!gzipped) data = compressed;
    try {
        std::istringstream stream(std::string(data.constData(), data.size()));
        auto pair = nbt::io::read_compound(stream);
        if (!pair.second || !pair.first.empty()) return "servers.dat has an invalid NBT root.";
        result["exists"] = true;
        result["servers"] = readServerArray(*pair.second);
        result["revision"] = QString::fromLatin1(QCryptographicHash::hash(compressed, QCryptographicHash::Sha256).toHex());
    } catch (const std::exception& exception) {
        if (!gzipped) return "servers.dat is neither valid gzip nor NBT data.";
        return QString("Could not parse servers.dat: %1").arg(exception.what());
    }
    return {};
}

QString pingServer(const QJsonObject& parameters, QJsonObject& result)
{
    const auto address = parameters.value("address").toString().trimmed();
    if (address.isEmpty()) return "A server address is required.";
    const auto target = MinecraftTarget::parse(address, false);
    if (target.address.isEmpty() || target.port == 0) return "The server address is invalid.";
    QElapsedTimer elapsed;
    elapsed.start();
    QPointer<ServerPingTask> task = new ServerPingTask(target.address, target.port);
    task->setParent(APPLICATION);
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(task, &Task::finished, &loop, &QEventLoop::quit);
    QObject::connect(task, &Task::finished, task, &QObject::deleteLater);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(8000);
    task->start();
    if (!task->isFinished()) loop.exec(QEventLoop::ExcludeUserInputEvents);
    if (!task || !task->isFinished()) return "The server did not respond within 8 seconds.";
    if (!task->wasSuccessful()) return task->failReason().isEmpty() ? "The server did not respond." : task->failReason();
    result = task->m_outputData;
    result["address"] = target.address;
    result["port"] = target.port;
    result["latencyMs"] = static_cast<int>(elapsed.elapsed());
    result["online"] = true;
    result["onlinePlayers"] = task->m_outputOnlinePlayers;
    return {};
}

QString writeServers(const QJsonObject& parameters, UserInteraction&, QJsonObject& result)
{
    const auto reference = parameters.value("instance").toString();
    auto instance = findInstance(reference);
    if (!instance) return "The instance does not exist.";
    if (!parameters.value("servers").isArray()) return "servers must be an array.";
    const auto path = QDir(instance->gameRoot()).filePath("servers.dat");
    QFile existing(path);
    QByteArray oldCompressed;
    if (existing.exists()) {
        if (!existing.open(QIODevice::ReadOnly)) return existing.errorString();
        oldCompressed = existing.readAll();
        if (parameters.contains("ifRevision")) {
            const auto revision = QString::fromLatin1(QCryptographicHash::hash(oldCompressed, QCryptographicHash::Sha256).toHex());
            if (parameters.value("ifRevision").toString() != revision)
                return "servers.dat changed since it was read. Read it again before saving.";
        }
    }

    nbt::tag_compound root;
    nbt::tag_list list(nbt::tag_type::Compound);
    for (const auto& item : parameters.value("servers").toArray()) {
        if (!item.isObject()) return "Each server must be an object.";
        const auto object = item.toObject();
        const auto ip = object.value("ip").toString().trimmed();
        if (ip.isEmpty()) return "Server address must not be empty.";
        nbt::tag_compound entry;
        entry.put("name", nbt::tag_string(object.value("name").toString().toUtf8().toStdString()));
        entry.put("ip", nbt::tag_string(ip.toUtf8().toStdString()));
        const auto icon = object.value("icon").toString();
        if (!icon.isEmpty()) entry.put("icon", nbt::tag_string(icon.toUtf8().toStdString()));
        entry.put("acceptTextures", nbt::tag_byte(object.value("acceptTextures").toBool() ? 1 : 0));
        list.push_back(std::move(entry));
    }
    root.put("servers", std::move(list));

    std::ostringstream serialized;
    nbt::io::write_tag("", root, serialized);
    const auto raw = QByteArray(serialized.str().data(), static_cast<int>(serialized.str().size()));
    QByteArray compressed;
    if (!GZip::zip(raw, compressed)) return "Could not compress servers.dat.";
    PSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly)) return output.errorString();
    if (output.write(compressed) != compressed.size()) {
        output.cancelWriting();
        return output.errorString();
    }
    if (!output.commit()) return output.errorString();
    result["instance"] = instance->id();
    result["path"] = path;
    result["servers"] = parameters.value("servers");
    result["revision"] = QString::fromLatin1(QCryptographicHash::hash(compressed, QCryptographicHash::Sha256).toHex());
    return {};
}

}  // namespace

void registerLauncherApiServerListOperations(LauncherApi& api)
{
    const auto instance = stringProperty("Instance id, managed name, or display name.");
    const QJsonObject serverObject{ { "type", "object" }, { "properties", QJsonObject{
        { "name", stringProperty("Display name.") }, { "ip", stringProperty("Host or host:port.") },
        { "icon", stringProperty("Optional base64 data URI icon.") },
        { "acceptTextures", boolProperty("Whether the client accepts server resource packs.") } } },
        { "required", QJsonArray{ "ip" } } };

    api.registerOperation({ "instance.servers.read",
        "Read the vanilla servers.dat list from the instance game directory.",
        objectSchema({ { "instance", instance } }, { "instance" }), "instances" },
        [](const QJsonObject& p, UserInteraction& i) {
            QJsonObject result;
            const auto error = readServers(p, i, result);
            return error.isEmpty() ? OperationService::success(result) : OperationService::failure(error, 2);
        });

    api.registerOperation({ "server.ping", "Ping a Minecraft server and return its MOTD, version, player counts, and latency.",
        objectSchema({ { "address", stringProperty("Host or host:port.") } }, { "address" }), "instances" },
        [](const QJsonObject& p, UserInteraction&) {
            QJsonObject result;
            const auto error = pingServer(p, result);
            return error.isEmpty() ? OperationService::success(result) : OperationService::failure(error);
        });

    api.registerOperation({ "instance.servers.write",
        "Atomically replace servers.dat. Optionally reject the write if its revision changed.",
        objectSchema({ { "instance", instance }, { "servers", QJsonObject{ { "type", "array" }, { "items", serverObject } } },
                       { "ifRevision", stringProperty("Revision returned by instance.servers.read.") } },
                     { "instance", "servers" }), "instances", true },
        [](const QJsonObject& p, UserInteraction& i) {
            QJsonObject result;
            const auto error = writeServers(p, i, result);
            return error.isEmpty() ? OperationService::success(result) : OperationService::failure(error, 2);
        });
}
