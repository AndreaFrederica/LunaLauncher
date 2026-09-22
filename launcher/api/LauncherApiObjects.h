// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QJsonObject>
#include <QPointer>

#include "Application.h"
#include "BaseInstance.h"
#include "InstanceList.h"
#include "minecraft/MinecraftInstance.h"

/**
 * Object handles used by the transport-neutral API.
 *
 * The GUI and the API both operate on the same BaseInstance objects.  Keeping
 * a guarded handle here prevents each API domain from resolving references and
 * rebuilding the public instance shape independently.
 */
namespace LauncherApiModel {

class InstanceObject {
   public:
    InstanceObject() = default;
    explicit InstanceObject(BaseInstance* instance) : m_instance(instance) {}

    static InstanceObject resolve(const QString& reference)
    {
        auto* list = APPLICATION->instances();
        auto* instance = list->getInstanceById(reference);
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
        return InstanceObject(instance);
    }

    BaseInstance* get() const { return m_instance.data(); }
    explicit operator bool() const { return !m_instance.isNull(); }

    QJsonObject snapshot() const
    {
        auto* instance = get();
        if (!instance)
            return {};
        const auto id = instance->id();
        QJsonObject result{
            { "id", id },
            { "name", instance->name() },
            { "type", instance->typeName() },
            { "group", APPLICATION->instances()->getInstanceGroup(id) },
            { "icon", instance->iconKey() },
            { "notes", instance->notes() },
            { "paths", QJsonObject{ { "root", instance->instanceRoot() },
                                    { "game", instance->gameRoot() },
                                    { "mods", instance->modsRoot() } } },
            { "lifecycle", QJsonObject{ { "running", instance->isRunning() },
                                         { "lastLaunch", instance->lastLaunch() },
                                         { "playTimeSeconds", static_cast<qint64>(instance->totalTimePlayed()) } } },
            { "managed", QJsonObject{ { "enabled", instance->isManagedPack() },
                                       { "type", instance->getManagedPackType() },
                                       { "projectId", instance->getManagedPackID() },
                                       { "versionId", instance->getManagedPackVersionID() } } },
            { "capabilities", QJsonObject{ { "launch", instance->canLaunch() },
                                            { "stop", instance->isRunning() },
                                            { "resources", true },
                                            { "settings", true } } } };
        if (const auto minecraft = dynamic_cast<MinecraftInstance*>(instance))
            result.insert("runtime", QJsonObject{ { "minecraftVersion", minecraft->getPackProfile()->getComponentVersion("net.minecraft") } });
        else
            result.insert("runtime", QJsonObject{});
        return result;
    }

   private:
    QPointer<BaseInstance> m_instance;
};

}  // namespace LauncherApiModel
