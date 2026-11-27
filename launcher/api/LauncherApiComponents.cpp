/* SPDX-License-Identifier: MPL-2.0 */

#include "LauncherApiComponents.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QUuid>

#include <exception>

#include "Application.h"
#include "InstanceList.h"
#include "api/LauncherApi.h"
#include "cli/OperationService.h"
#include "cli/UserInteraction.h"
#include "minecraft/Component.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "minecraft/OneSixVersionFormat.h"
#include "net/Mode.h"
#include "tasks/Task.h"

namespace {

QJsonObject stringProperty(const QString& description)
{
    return { { "type", "string" }, { "description", description } };
}

QJsonObject boolProperty(const QString& description, bool defaultValue = false)
{
    return { { "type", "boolean" }, { "description", description }, { "default", defaultValue } };
}

QJsonObject objectSchema(QJsonObject properties, QJsonArray required = {})
{
    QJsonObject schema{ { "type", "object" }, { "properties", properties }, { "additionalProperties", false } };
    if (!required.isEmpty())
        schema.insert("required", required);
    return schema;
}

BaseInstance* findInstance(const QString& reference)
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
    return instance;
}

MinecraftInstance* findMinecraftInstance(const QJsonObject& parameters, QString* error = nullptr)
{
    auto instance = dynamic_cast<MinecraftInstance*>(findInstance(parameters.value("instance").toString()));
    if (!instance && error)
        *error = QObject::tr("Minecraft instance not found: %1").arg(parameters.value("instance").toString());
    return instance;
}

QString taskId(Task* task)
{
    return task ? task->getUid().toString(QUuid::WithoutBraces) : QString();
}

bool waitForTask(LauncherApi& api, Task* task, UserInteraction& interaction, QString* error)
{
    if (!task)
        return true;

    QEventLoop loop;
    api.trackTask(task);
    QObject::connect(task, &Task::status, &loop, [&interaction](const QString& status) { interaction.status(status); });
    QObject::connect(task, &Task::finished, &loop, &QEventLoop::quit);
    if (!task->isFinished()) {
        if (!task->isRunning())
            task->start();
        if (!task->isFinished())
            loop.exec();
    }
    api.clearTrackedTask(task);
    if (!task->wasSuccessful()) {
        if (error)
            *error = task->failReason();
        return false;
    }
    return true;
}

bool ensureLoaded(MinecraftInstance* instance, LauncherApi& api, UserInteraction& interaction, QString* error)
{
    auto profile = instance->getPackProfile();
    try {
        // An online update may already own this profile. Wait for it instead
        // of replacing it with an offline reload.
        auto task = profile->getCurrentTask();
        if (!waitForTask(api, task.get(), interaction, error))
            return false;

        if (profile->isLoaded()) {
            // instance.list/info can load mmc-pack.json without resolving the
            // standard components' metadata. The list alone is not enough.
            bool resolved = true;
            for (int i = 0; i < profile->rowCount(); ++i) {
                if (!profile->getComponent(static_cast<size_t>(i))->getVersionFile()) {
                    resolved = false;
                    break;
                }
            }
            if (resolved)
                return true;
            profile->resolve(Net::Mode::Offline);
        } else {
            const auto result = profile->reload(Net::Mode::Offline);
            if (!result) {
                if (error)
                    *error = result.error;
                return false;
            }
        }
        task = profile->getCurrentTask();
        if (!waitForTask(api, task.get(), interaction, error))
            return false;
    } catch (const std::exception& exception) {
        if (error)
            *error = QString::fromUtf8(exception.what());
        return false;
    }
    return true;
}

QJsonObject componentJson(const ComponentPtr& component)
{
    if (!component)
        return {};
    QJsonArray problems;
    for (const auto& problem : component->getProblems())
        problems.append(QJsonObject{ { "severity", static_cast<int>(problem.m_severity) }, { "description", problem.m_description } });
    QJsonObject contents;
    if (const auto file = component->getVersionFile()) {
        QJsonArray libraries, embeddedLoaders;
        for (const auto& library : file->libraries) {
            const auto& name = library->rawName();
            libraries.append(name.serialize());
            if (component->getID() != "net.xiaoyu233.fishmodloader" &&
                name.groupId() == "net.xiaoyu233.fishmodloader" && name.artifactId() == "fishmodloader")
                embeddedLoaders.append(QJsonObject{{"name", "FishModLoader"}, {"version", name.version()}});
        }
        QJsonArray javaVersions;
        for (const auto major : file->compatibleJavaMajors) javaVersions.append(major);
        contents = {{"mainClass", file->mainClass}, {"mainJar", file->mainJar ? file->mainJar->rawName().serialize() : QString()},
                    {"libraries", libraries}, {"embeddedLoaders", embeddedLoaders}, {"javaVersions", javaVersions},
                    {"jarModCount", file->jarMods.size()}, {"agentCount", file->agents.size()}};
    }
    return { { "id", component->getID() },
             { "name", component->getName() },
             { "version", component->getVersion() },
             { "enabled", component->isEnabled() },
             { "custom", component->isCustom() },
             { "dependencyOnly", component->m_dependencyOnly },
             { "important", component->m_important },
             { "filename", component->getFilename() },
             { "order", component->getOrder() },
             { "releaseDate", component->getReleaseDateTime().toString(Qt::ISODate) },
             { "canDisable", component->canBeDisabled() },
             { "canRemove", component->isRemovable() },
             { "canMove", component->isMoveable() },
             { "canChangeVersion", component->isVersionChangeable(false) },
             { "canCustomize", component->isCustomizable() },
             { "canRevert", component->isRevertible() },
             { "canEdit", component->isCustom() && QFileInfo::exists(component->getFilename()) },
             { "problemSeverity", static_cast<int>(component->getProblemSeverity()) },
             { "contents", contents },
             { "problems", problems } };
}

void makeForeignComponentReadOnly(QJsonObject& object)
{
    // A vanilla/PCL/HMCL profile is backed by the external version JSON. It
    // can be inspected in the editor, while Prism-only component mutations
    // require an explicit conversion first.
    for (const auto key : { "canDisable", "canRemove", "canMove", "canChangeVersion", "canCustomize", "canRevert", "canEdit" })
        object.insert(key, false);
}

bool rejectForeignComponentEdit(const MinecraftInstance* instance)
{
    return instance && instance->traits().contains(QStringLiteral("vanilla-compatible"));
}

QJsonObject listComponents(LauncherApi& api, const QJsonObject& parameters, UserInteraction& interaction)
{
    QString error;
    auto instance = findMinecraftInstance(parameters, &error);
    if (!instance)
        return OperationService::failure(error, 2);
    if (!ensureLoaded(instance, api, interaction, &error))
        return OperationService::failure(error.isEmpty() ? QObject::tr("Could not load the instance profile.") : error);

    QJsonArray components;
    const auto profile = instance->getPackProfile();
    const bool foreign = rejectForeignComponentEdit(instance);
    for (int i = 0; i < profile->rowCount(); ++i) {
        auto item = componentJson(profile->getComponent(static_cast<size_t>(i)));
        if (foreign)
            makeForeignComponentReadOnly(item);
        item.insert("index", i);
        components.append(item);
    }
    return OperationService::success(QJsonObject{ { "instance", instance->id() }, { "components", components },
        { "readOnly", foreign }, { "running", instance->isRunning() }, { "canInstallCustom", !foreign && !instance->isRunning() } });
}

bool validComponentUid(const QString& uid)
{
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9_.-]{0,199}$"));
    return pattern.match(uid).hasMatch() && !uid.contains("..");
}

QJsonObject reloadComponents(LauncherApi& api, const QJsonObject& parameters, UserInteraction& interaction)
{
    QString error;
    auto instance = findMinecraftInstance(parameters, &error);
    if (!instance) return OperationService::failure(error, 2);
    if (instance->isRunning()) return listComponents(api, parameters, interaction);
    instance->getPackProfile()->saveNow();
    const auto loaded = instance->getPackProfile()->reload(Net::Mode::Offline);
    if (!loaded) return OperationService::failure(loaded.error);
    auto task = instance->getPackProfile()->getCurrentTask();
    if (!waitForTask(api, task.get(), interaction, &error)) return OperationService::failure(error);
    return listComponents(api, parameters, interaction);
}

QJsonObject addEmptyComponent(LauncherApi& api, const QJsonObject& parameters, UserInteraction& interaction)
{
    QString error;
    auto instance = findMinecraftInstance(parameters, &error);
    if (!instance) return OperationService::failure(error, 2);
    if (rejectForeignComponentEdit(instance) || instance->isRunning())
        return OperationService::failure(QObject::tr("Stop the instance and use a Prism-format copy before adding custom components."), 2);
    if (!ensureLoaded(instance, api, interaction, &error)) return OperationService::failure(error);
    const auto uid = parameters.value("component").toString().trimmed();
    const auto name = parameters.value("name").toString().trimmed();
    if (!validComponentUid(uid) || name.isEmpty())
        return OperationService::failure(QObject::tr("A component name and a valid UID containing letters, digits, dots, hyphens or underscores are required."), 2);
    auto profile = instance->getPackProfile();
    if (profile->getComponent(uid) || QFileInfo::exists(profile->patchFilePathForUid(uid)))
        return OperationService::failure(QObject::tr("A component with this UID already exists."), 2);
    if (!profile->installEmpty(uid, name)) return OperationService::failure(QObject::tr("Could not create the component."));
    profile->saveNow();
    return OperationService::success(QJsonObject{{"componentInfo", componentJson(profile->getComponent(uid))}});
}

QJsonObject componentPatch(LauncherApi& api, const QJsonObject& parameters, UserInteraction& interaction, bool write)
{
    QString error;
    auto instance = findMinecraftInstance(parameters, &error);
    if (!instance) return OperationService::failure(error, 2);
    if (rejectForeignComponentEdit(instance) || (write && instance->isRunning()))
        return OperationService::failure(QObject::tr("This component patch cannot be edited in the current instance."), 2);
    if (!ensureLoaded(instance, api, interaction, &error)) return OperationService::failure(error);
    const auto uid = parameters.value("component").toString();
    auto component = instance->getPackProfile()->getComponent(uid);
    if (!validComponentUid(uid) || !component || !component->isCustom())
        return OperationService::failure(QObject::tr("Select a local custom component before editing its JSON."), 2);
    const QFileInfo path(component->getFilename());
    const QFileInfo patches(QDir(instance->componentRoot()).filePath("patches"));
    if (path.isSymLink() || patches.isSymLink() || !path.isFile() ||
        path.canonicalFilePath() != QDir(patches.canonicalFilePath()).filePath(uid + ".json"))
        return OperationService::failure(QObject::tr("The component patch must be a regular file in the instance patches directory."), 2);
    QFile file(path.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly) || file.size() > 4 * 1024 * 1024)
        return OperationService::failure(QObject::tr("Could not read the component patch, or it exceeds 4 MiB."));
    const auto current = file.readAll();
    file.close();
    const auto hash = [](const QByteArray& bytes) {
        return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
    };
    if (!write) return OperationService::success(QJsonObject{{"content", QString::fromUtf8(current)},
        {"revision", hash(current)}, {"filename", path.absoluteFilePath()}});
    if (parameters.value("revision").toString() != hash(current))
        return OperationService::failure(QObject::tr("The patch changed on disk. Reload it before saving."), 2);
    const auto content = parameters.value("content").toString().toUtf8();
    if (content.size() > 4 * 1024 * 1024) return OperationService::failure(QObject::tr("Component JSON exceeds 4 MiB."), 2);
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(content, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return OperationService::failure(QObject::tr("Invalid component JSON: %1").arg(parseError.errorString()), 2);
    try {
        const auto parsed = OneSixVersionFormat::versionFileFromJson(document, path.absoluteFilePath(), false);
        if (parsed->uid != uid) return OperationService::failure(QObject::tr("The component UID cannot be changed while editing."), 2);
    } catch (const Exception& exception) {
        return OperationService::failure(exception.cause(), 2);
    }
    QSaveFile output(path.absoluteFilePath());
    if (!output.open(QIODevice::WriteOnly) || output.write(content) != content.size() || !output.commit())
        return OperationService::failure(output.errorString());
    auto result = reloadComponents(api, parameters, interaction);
    if (result.value("ok").toBool()) result.insert("revision", hash(content));
    return result;
}

QJsonObject setComponentVersion(LauncherApi& api, const QJsonObject& parameters, UserInteraction& interaction)
{
    QString error;
    auto instance = findMinecraftInstance(parameters, &error);
    if (!instance)
        return OperationService::failure(error, 2);
    if (rejectForeignComponentEdit(instance))
        return OperationService::failure(QObject::tr("This external vanilla instance must be converted to a Prism copy before changing components."), 2);
    if (instance->isRunning())
        return OperationService::failure(QObject::tr("Components cannot be modified while the instance is running."), 2);
    if (!ensureLoaded(instance, api, interaction, &error))
        return OperationService::failure(error, 2);

    const auto uid = parameters.value("component").toString().trimmed();
    const auto version = parameters.value("version").toString().trimmed();
    if (uid.isEmpty() || version.isEmpty())
        return OperationService::failure(QObject::tr("A component id and non-empty version are required."), 2);

    const bool important = parameters.value("important").toBool(false);
    auto profile = instance->getPackProfile();
    if (!profile->setComponentVersion(uid, version, important))
        return OperationService::failure(QObject::tr("The component version could not be changed."));

    const bool resolve = parameters.value("resolve").toBool(true);
    QJsonObject result{ { "component", uid }, { "version", version }, { "changed", true } };
    if (resolve) {
        profile->resolve(Net::Mode::Online);
        const auto task = profile->getCurrentTask();
        const auto id = taskId(task.get());
        if (parameters.value("wait").toBool(true)) {
            if (!waitForTask(api, task.get(), interaction, &error))
                return OperationService::failure(error.isEmpty() ? QObject::tr("Component resolution failed.") : error);
            result.insert("resolved", true);
        } else if (!id.isEmpty()) {
            api.trackTask(task.get());
            result.insert("taskId", id);
            result.insert("resolved", false);
        }
    }
    profile->saveNow();
    if (auto component = profile->getComponent(uid))
        result.insert("componentInfo", componentJson(component));
    return OperationService::success(result);
}

QJsonObject setComponentEnabled(LauncherApi& api, const QJsonObject& parameters, UserInteraction& interaction)
{
    QString error;
    auto instance = findMinecraftInstance(parameters, &error);
    if (!instance)
        return OperationService::failure(error, 2);
    if (rejectForeignComponentEdit(instance))
        return OperationService::failure(QObject::tr("This external vanilla instance must be converted to a Prism copy before changing components."), 2);
    if (instance->isRunning())
        return OperationService::failure(QObject::tr("Components cannot be modified while the instance is running."), 2);
    if (!ensureLoaded(instance, api, interaction, &error))
        return OperationService::failure(error, 2);

    const auto uid = parameters.value("component").toString().trimmed();
    auto component = instance->getPackProfile()->getComponent(uid);
    if (!component)
        return OperationService::failure(QObject::tr("Component not found: %1").arg(uid), 2);
    const bool enabled = parameters.value("enabled").toBool();
    if (!component->setEnabled(enabled))
        return OperationService::failure(QObject::tr("The component cannot be enabled or disabled."));
    instance->getPackProfile()->invalidateLaunchProfile();
    instance->getPackProfile()->saveNow();
    return OperationService::success(QJsonObject{ { "componentInfo", componentJson(component) }, { "changed", true } });
}

QJsonObject removeComponent(LauncherApi& api, const QJsonObject& parameters, UserInteraction& interaction)
{
    if (!parameters.value("confirm").toBool())
        return OperationService::failure(QObject::tr("Component removal requires confirm=true."), 2);
    QString error;
    auto instance = findMinecraftInstance(parameters, &error);
    if (!instance)
        return OperationService::failure(error, 2);
    if (rejectForeignComponentEdit(instance))
        return OperationService::failure(QObject::tr("This external vanilla instance must be converted to a Prism copy before changing components."), 2);
    if (instance->isRunning())
        return OperationService::failure(QObject::tr("Components cannot be modified while the instance is running."), 2);
    if (!ensureLoaded(instance, api, interaction, &error))
        return OperationService::failure(error, 2);

    const auto uid = parameters.value("component").toString().trimmed();
    auto profile = instance->getPackProfile();
    auto component = profile->getComponent(uid);
    if (!component)
        return OperationService::failure(QObject::tr("Component not found: %1").arg(uid), 2);
    if (!component->isRemovable())
        return OperationService::failure(QObject::tr("The component cannot be removed."), 2);
    if (!profile->remove(uid))
        return OperationService::failure(QObject::tr("The component could not be removed."));
    profile->saveNow();
    return OperationService::success(QJsonObject{ { "component", uid }, { "removed", true } });
}

QJsonObject moveComponent(LauncherApi& api, const QJsonObject& parameters, UserInteraction& interaction)
{
    QString error;
    auto instance = findMinecraftInstance(parameters, &error);
    if (!instance)
        return OperationService::failure(error, 2);
    if (rejectForeignComponentEdit(instance))
        return OperationService::failure(QObject::tr("This external vanilla instance must be converted to a Prism copy before changing components."), 2);
    if (instance->isRunning())
        return OperationService::failure(QObject::tr("Components cannot be modified while the instance is running."), 2);
    if (!ensureLoaded(instance, api, interaction, &error))
        return OperationService::failure(error, 2);

    const auto uid = parameters.value("component").toString().trimmed();
    const auto direction = parameters.value("direction").toString().trimmed().toLower();
    auto profile = instance->getPackProfile();
    int index = -1;
    for (int i = 0; i < profile->rowCount(); ++i) {
        if (auto component = profile->getComponent(static_cast<size_t>(i)); component && component->getID() == uid) {
            index = i;
            break;
        }
    }
    if (index < 0)
        return OperationService::failure(QObject::tr("Component not found: %1").arg(uid), 2);
    if (direction != "up" && direction != "down")
        return OperationService::failure(QObject::tr("Direction must be 'up' or 'down'."), 2);
    auto component = profile->getComponent(static_cast<size_t>(index));
    if (!component->isMoveable())
        return OperationService::failure(QObject::tr("The component cannot be moved."), 2);
    const int target = direction == "up" ? qMax(0, index - 1) : qMin(profile->rowCount() - 1, index + 1);
    if (target == index)
        return OperationService::success(QJsonObject{ { "component", uid }, { "index", index }, { "changed", false } });
    profile->move(index, direction == "up" ? PackProfile::MoveUp : PackProfile::MoveDown);
    profile->saveNow();
    return OperationService::success(QJsonObject{ { "component", uid }, { "index", target }, { "changed", true } });
}

QJsonObject customizeComponent(LauncherApi& api, const QJsonObject& parameters, UserInteraction& interaction, bool revert)
{
    QString error;
    auto instance = findMinecraftInstance(parameters, &error);
    if (!instance)
        return OperationService::failure(error, 2);
    if (rejectForeignComponentEdit(instance))
        return OperationService::failure(QObject::tr("This external vanilla instance must be converted to a Prism copy before changing components."), 2);
    if (instance->isRunning())
        return OperationService::failure(QObject::tr("Components cannot be modified while the instance is running."), 2);
    if (!ensureLoaded(instance, api, interaction, &error))
        return OperationService::failure(error, 2);
    const auto uid = parameters.value("component").toString().trimmed();
    auto profile = instance->getPackProfile();
    int index = -1;
    for (int i = 0; i < profile->rowCount(); ++i) {
        if (auto component = profile->getComponent(static_cast<size_t>(i)); component && component->getID() == uid) {
            index = i;
            break;
        }
    }
    if (index < 0)
        return OperationService::failure(QObject::tr("Component not found: %1").arg(uid), 2);
    const bool changed = revert ? profile->revertToBase(index) : profile->customize(index);
    if (!changed)
        return OperationService::failure(revert ? QObject::tr("The component could not be reverted.")
                                               : QObject::tr("The component could not be customized."));
    profile->saveNow();
    return OperationService::success(QJsonObject{ { "componentInfo", componentJson(profile->getComponent(static_cast<size_t>(index))) },
                                                  { "changed", true } });
}

QJsonObject installCustomComponent(LauncherApi& api, const QJsonObject& parameters, UserInteraction& interaction)
{
    QString error;
    auto instance = findMinecraftInstance(parameters, &error);
    if (!instance) return OperationService::failure(error, 2);
    if (rejectForeignComponentEdit(instance)) return OperationService::failure(QObject::tr("This external vanilla instance must be converted to a Prism copy before changing components."), 2);
    if (instance->isRunning()) return OperationService::failure(QObject::tr("Components cannot be modified while the instance is running."), 2);
    if (!ensureLoaded(instance, api, interaction, &error)) return OperationService::failure(error, 2);
    const auto source = QFileInfo(parameters.value("source").toString());
    if (!source.isFile() || !source.isReadable()) return OperationService::failure(QObject::tr("Component source file was not found."), 2);
    const auto type = parameters.value("type").toString("component").trimmed().toLower();
    auto profile = instance->getPackProfile();
    bool changed = false;
    if (type == "component") {
        QFile file(source.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly) || file.size() > 4 * 1024 * 1024)
            return OperationService::failure(QObject::tr("Could not read the component JSON, or it exceeds 4 MiB."), 2);
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject())
            return OperationService::failure(QObject::tr("Invalid component JSON: %1").arg(parseError.errorString()), 2);
        try {
            const auto parsed = OneSixVersionFormat::versionFileFromJson(document, source.absoluteFilePath(), false);
            if (!validComponentUid(parsed->uid) || profile->getComponent(parsed->uid) || QFileInfo::exists(profile->patchFilePathForUid(parsed->uid)))
                return OperationService::failure(QObject::tr("The imported component UID is invalid or already installed."), 2);
        } catch (const Exception& exception) {
            return OperationService::failure(exception.cause(), 2);
        }
        changed = profile->installComponents({ source.absoluteFilePath() });
    } else if (type == "jarmod") {
        profile->installJarMods({ source.absoluteFilePath() });
        changed = true;
    } else if (type == "jar") {
        profile->installCustomJar(source.absoluteFilePath());
        changed = true;
    } else if (type == "agent") {
        profile->installAgents({ source.absoluteFilePath() });
        changed = true;
    } else {
        return OperationService::failure(QObject::tr("type must be component, jarmod, jar, or agent."), 2);
    }
    if (!changed) return OperationService::failure(QObject::tr("The custom component could not be installed."));
    profile->saveNow();
    return OperationService::success(QJsonObject{ { "instance", instance->id() }, { "type", type },
        { "source", source.absoluteFilePath() }, { "changed", true } });
}

}  // namespace

void registerLauncherApiComponentOperations(LauncherApi& api)
{
    const auto instance = stringProperty("Minecraft instance ID, managed name, or display name.");
    const auto component = stringProperty("Component UID, such as net.minecraft or a loader UID.");
    api.registerOperation({ "instance.components.list", "List the ordered Minecraft components and their editable capabilities.",
                            objectSchema({ { "instance", instance } }, { "instance" }), "instance.components" },
                           [&api](const QJsonObject& parameters, UserInteraction& interaction) {
                               return listComponents(api, parameters, interaction);
                           });
    api.registerOperation({ "instance.components.reload", "Reload local component files and return the ordered profile.",
                            objectSchema({{"instance", instance}}, {"instance"}), "instance.components" },
                          [&api](const QJsonObject& p, UserInteraction& ui) { return reloadComponents(api, p, ui); });
    api.registerOperation({ "instance.component.add-empty", "Create a named local component without replacing an existing UID.",
                            objectSchema({{"instance", instance}, {"component", component}, {"name", stringProperty("Component display name.")}},
                                         {"instance", "component", "name"}), "instance.components", true },
                          [&api](const QJsonObject& p, UserInteraction& ui) { return addEmptyComponent(api, p, ui); });
    for (const bool write : {false, true}) {
        auto properties = QJsonObject{{"instance", instance}, {"component", component}};
        QJsonArray required{"instance", "component"};
        if (write) {
            properties.insert("content", stringProperty("Component JSON, at most 4 MiB."));
            properties.insert("revision", stringProperty("Revision returned by read-json."));
            required.append("content"); required.append("revision");
        }
        api.registerOperation({write ? "instance.component.write-json" : "instance.component.read-json",
                               "Read or atomically update a local component JSON with revision checking.",
                               objectSchema(properties, required), "instance.components", write},
                              [&api, write](const QJsonObject& p, UserInteraction& ui) { return componentPatch(api, p, ui, write); });
    }
    api.registerOperation({ "instance.component.set-version", "Change a Minecraft component version and optionally resolve dependencies.",
                            objectSchema({ { "instance", instance }, { "component", component }, { "version", stringProperty("Version descriptor.") },
                                           { "important", boolProperty("Mark the component as important.") }, { "resolve", boolProperty("Resolve dependencies online.", true) },
                                           { "wait", boolProperty("Wait for resolution to finish.", true) } },
                                          { "instance", "component", "version" }),
                            "instance.components" },
                           [&api](const QJsonObject& parameters, UserInteraction& interaction) {
                               return setComponentVersion(api, parameters, interaction);
                           });
    api.registerOperation({ "instance.component.set-enabled", "Enable or disable an optional Minecraft component.",
                            objectSchema({ { "instance", instance }, { "component", component }, { "enabled", boolProperty("Whether the component is enabled.") } },
                                          { "instance", "component", "enabled" }),
                            "instance.components" },
                           [&api](const QJsonObject& parameters, UserInteraction& interaction) {
                               return setComponentEnabled(api, parameters, interaction);
                           });
    api.registerOperation({ "instance.component.remove", "Remove a removable Minecraft component and its custom patch file.",
                            objectSchema({ { "instance", instance }, { "component", component }, { "confirm", boolProperty("Confirm removal.") } },
                                          { "instance", "component", "confirm" }),
                            "instance.components", true },
                           [&api](const QJsonObject& parameters, UserInteraction& interaction) {
                               return removeComponent(api, parameters, interaction);
                           });
    api.registerOperation({ "instance.component.move", "Move a Minecraft component up or down in the ordered profile.",
                            objectSchema({ { "instance", instance }, { "component", component }, { "direction", stringProperty("up or down.") } },
                                          { "instance", "component", "direction" }),
                            "instance.components" },
                           [&api](const QJsonObject& parameters, UserInteraction& interaction) {
                               return moveComponent(api, parameters, interaction);
                           });
    api.registerOperation({ "instance.component.customize", "Create a local patch file for a Minecraft component.",
                            objectSchema({ { "instance", instance }, { "component", component } }, { "instance", "component" }),
                            "instance.components" },
                           [&api](const QJsonObject& parameters, UserInteraction& interaction) {
                               return customizeComponent(api, parameters, interaction, false);
                           });
    api.registerOperation({ "instance.component.revert", "Remove a custom patch and return a component to its base metadata.",
                            objectSchema({ { "instance", instance }, { "component", component }, { "confirm", boolProperty("Confirm revert.") } },
                                          { "instance", "component", "confirm" }),
                            "instance.components", true },
                           [&api](const QJsonObject& parameters, UserInteraction& interaction) {
                               if (!parameters.value("confirm").toBool())
                                   return OperationService::failure(QObject::tr("Component revert requires confirm=true."), 2);
                               return customizeComponent(api, parameters, interaction, true);
                           });
    api.registerOperation({ "instance.component.install-custom", "Install a local custom component, jar mod, replacement jar, or Java agent using the existing profile workflow.",
                            objectSchema({ { "instance", instance }, { "source", stringProperty("Local component or jar file.") },
                                           { "type", stringProperty("component, jarmod, jar, or agent.") } }, { "instance", "source" }),
                            "instance.components", true },
                           [&api](const QJsonObject& parameters, UserInteraction& interaction) {
                               return installCustomComponent(api, parameters, interaction);
                           });
}
