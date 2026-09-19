// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QEventLoop>
#include <QMap>
#include <QRegularExpression>
#include "api/LauncherApi.h"
#include "cli/UserInteraction.h"
#include "modplatform/ModIndex.h"
#include "tasks/Task.h"

namespace ApiSupport {
inline QJsonObject string(const QString& description) { return { { "type", "string" }, { "description", description } }; }
inline QJsonObject boolean() { return { { "type", "boolean" } }; }
inline QJsonObject strings() { return { { "type", "array" }, { "items", QJsonObject{ { "type", "string" } } } }; }
inline QJsonObject schema(QJsonObject properties, QJsonArray required = {})
{
    return { { "type", "object" }, { "properties", properties }, { "required", required }, { "additionalProperties", false } };
}

inline bool wait(LauncherApi& api, const Task::Ptr& task, UserInteraction& interaction, QString& error)
{
    if (!task) {
        error = QStringLiteral("The operation did not create a task.");
        return false;
    }
    QEventLoop loop;
    QObject::connect(task.get(), &Task::status, &loop, [&interaction](const QString& status) { interaction.status(status); });
    QObject::connect(task.get(), &Task::finished, &loop, &QEventLoop::quit);
    api.trackTask(task.get());
    if (!task->isFinished()) {
        if (!task->isRunning())
            task->start();
        if (!task->isFinished())
            loop.exec();
    }
    api.clearTrackedTask(task.get());
    if (!task->wasSuccessful())
        error = task->failReason().isEmpty() ? QStringLiteral("The task was cancelled.") : task->failReason();
    return task->wasSuccessful();
}

inline bool identifier(const QString& value)
{
    static const QRegularExpression valid(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9_.+\\-]*$"));
    return value.size() <= 256 && valid.match(value).hasMatch() && !value.contains("..");
}

inline const QMap<QString, ModPlatform::ModLoaderType>& modLoaders()
{
    static const QMap<QString, ModPlatform::ModLoaderType> values{
        { "neoforge", ModPlatform::NeoForge }, { "forge", ModPlatform::Forge }, { "cauldron", ModPlatform::Cauldron },
        { "liteloader", ModPlatform::LiteLoader }, { "fabric", ModPlatform::Fabric }, { "quilt", ModPlatform::Quilt },
        { "datapack", ModPlatform::DataPack }, { "babric", ModPlatform::Babric }, { "bta", ModPlatform::BTA },
        { "legacyfabric", ModPlatform::LegacyFabric }, { "ornithe", ModPlatform::Ornithe }, { "rift", ModPlatform::Rift },
        { "cleanroom", ModPlatform::Cleanroom } };
    return values;
}
inline const QMap<QString, ModPlatform::PluginLoaderType>& pluginLoaders()
{
    static const QMap<QString, ModPlatform::PluginLoaderType> values{
        { "paper", ModPlatform::Paper }, { "spigot", ModPlatform::Spigot }, { "bukkit", ModPlatform::Bukkit },
        { "purpur", ModPlatform::Purpur }, { "sponge", ModPlatform::Sponge }, { "velocity", ModPlatform::Velocity },
        { "waterfall", ModPlatform::Waterfall }, { "bungeecord", ModPlatform::BungeeCord } };
    return values;
}
template <typename Flags, typename Enum>
bool parseFlags(const QJsonValue& input, const QMap<QString, Enum>& known, Flags& flags)
{
    flags = {};
    if (!input.isArray())
        return false;
    for (const auto& item : input.toArray()) {
        const auto key = item.toString().toLower();
        if (!known.contains(key))
            return false;
        flags |= known.value(key);
    }
    return true;
}
template <typename Flags, typename Enum>
QJsonArray flagNames(Flags flags, const QMap<QString, Enum>& known)
{
    QJsonArray result;
    for (auto it = known.begin(); it != known.end(); ++it)
        if (flags.testFlag(it.value()))
            result.append(it.key());
    return result;
}
}  // namespace ApiSupport
