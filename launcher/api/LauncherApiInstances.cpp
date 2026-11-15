// SPDX-License-Identifier: MPL-2.0

#include "LauncherApiInstances.h"

#include <QDir>
#include <QBuffer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QUrl>

#include <optional>

#include "Application.h"
#include "BaseInstance.h"
#include "FileSystem.h"
#include "cli/OperationService.h"
#include "cli/HeadlessBlockedMods.h"
#include "DesktopServices.h"
#include "InstanceImportTask.h"
#include "InstanceList.h"
#include "InstanceStore.h"
#include "InstanceTask.h"
#include "VanillaConversion.h"
#include "VanillaConversionTask.h"
#include "LaunchController.h"
#include "LauncherApi.h"
#include "api/LauncherApiSupport.h"
#include "api/LauncherApiObjects.h"
#include "icons/IconList.h"
#include "icons/IconUtils.h"
#include "minecraft/ShortcutUtils.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/MinecraftLoadAndCheck.h"
#include "minecraft/PackProfile.h"
#include "minecraft/Component.h"
#include "minecraft/OneSixVersionFormat.h"
#include "settings/INISettingsObject.h"
#include "minecraft/update/AssetUpdateTask.h"
#include "minecraft/VanillaInstanceCreationTask.h"
#include "meta/CleanroomMeta.h"
#include "meta/Index.h"
#include "meta/Version.h"
#include "server/ServerInstance.h"
#include "server/ServerInstanceCreationTask.h"
#include "tasks/Task.h"
#include "cli/UserInteraction.h"
#include "cli/ScopedUserInteraction.h"

namespace {

QJsonObject stringProperty(const QString& description)
{
    return { { "type", "string" }, { "description", description } };
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

QString loaderUid(QString loader)
{
    loader = loader.trimmed().toLower();
    static const QHash<QString, QString> aliases{ { "fabric", "net.fabricmc.fabric-loader" },
                                                  { "quilt", "org.quiltmc.quilt-loader" },
                                                  { "forge", "net.minecraftforge" },
                                                  { "neoforge", "net.neoforged" },
                                                  { "cleanroom", "com.cleanroommc.cleanroom" },
                                                  { "liteloader", "com.mumfrey.liteloader" } };
    return aliases.value(loader, loader);
}

bool waitForTask(Task* task, UserInteraction& interaction, QString* error, LauncherApi* api = nullptr)
{
    if (!task) {
        if (error)
            *error = QObject::tr("The operation did not create a task.");
        return false;
    }
    QEventLoop loop;
    if (api)
        api->trackTask(task);
    QObject::connect(task, &Task::status, &loop, [&interaction](const QString& status) { interaction.status(status); });
    QObject::connect(task, &Task::finished, &loop, &QEventLoop::quit);
    if (!task->isFinished()) {
        if (!task->isRunning())
            task->start();
        if (!task->isFinished())
            loop.exec();
    }
    if (api)
        api->clearTrackedTask(task);
    if (!task->wasSuccessful() && error)
        *error = task->failReason().isEmpty() ? QObject::tr("The operation was aborted.") : task->failReason();
    return task->wasSuccessful();
}

bool resolveVanillaExport(LauncherApi& api, const QString& root, VanillaExportOptions& options,
                          UserInteraction& interaction, QString& error)
{
    // Temporary directories are intentionally outside the registered stores.
    // Load directly instead of relying on InstanceList's configured-root scan.
    auto settings = std::make_unique<INISettingsObject>(QDir(root).filePath("instance.cfg"));
    MinecraftInstance instance(APPLICATION->settings(), std::move(settings), root);
    instance.loadSpecificSettings();
    MinecraftLoadAndCheck load(&instance, Net::Mode::Online);
    if (!waitForTask(&load, interaction, &error, &api)) return false;
    const auto components = instance.getPackProfile();
    for (int i = 0; i < components->rowCount(); ++i) {
        const auto component = components->getComponent(static_cast<size_t>(i));
        if (!component->isEnabled()) continue;
        const auto file = component->getVersionFile();
        if (!file) {
            error = QObject::tr("Missing resolved metadata for %1.").arg(component->getID());
            return false;
        }
        // Preserve structured arguments, download rules and logging fields
        // which the internal VersionFile model does not round-trip.
        const auto meta = component->getMeta();
        const auto path = component->isCustom() || !meta ? components->patchFilePathForUid(component->getID())
                                                       : QDir("meta").filePath(meta->localFilename());
        QFile raw(path);
        auto profile = raw.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(raw.readAll()).object() : QJsonObject{};
        if (profile.isEmpty()) profile = OneSixVersionFormat::versionFileToJson(file).object();
        profile.insert("uid", component->getID());
        profile.insert("version", component->getVersion());
        options.componentProfiles.append(profile);
    }
    options.librariesRoot = QDir("libraries").absolutePath();
    options.assetsRoot = instance.assetsRoot();
    return true;
}

struct InstanceStoreTarget {
    InstanceStore store;

    bool isVanilla() const { return store.isVanillaCompatible(); }
};

std::optional<InstanceStoreTarget> chooseInstanceStoreTarget(const QJsonObject& parameters,
                                                              UserInteraction& interaction,
                                                              QString* error)
{
    const auto requestedId = parameters.value(QStringLiteral("targetStoreId")).toString().trimmed();
    auto format = parameters.value("targetFormat").toString().trimmed();
    if (!format.isEmpty() && format != "prism" && format != "vanilla-compatible") {
        if (error) *error = QObject::tr("targetFormat must be prism or vanilla-compatible.");
        return std::nullopt;
    }
    if (format.isEmpty() && requestedId.isEmpty() && !parameters.value("source").toString().isEmpty()) {
        const auto selected = interaction.select(QObject::tr("请选择导入后的实例格式"), QJsonArray{
            QJsonObject{{"id","prism"}, {"name",QObject::tr("Prism / MMC 格式")}},
            QJsonObject{{"id","vanilla-compatible"}, {"name",QObject::tr("原版格式（HMCL / PCL，需要已配置原版存储位置）")}}});
        if (!selected || *selected < 0 || *selected > 1) {
            if (error) *error = QObject::tr("Instance import was cancelled."); return std::nullopt;
        }
        format = *selected == 0 ? "prism" : "vanilla-compatible";
    }
    const auto matches = [&format](const InstanceStore& store) {
        return format.isEmpty() || (format == "vanilla-compatible" ? store.isVanillaCompatible() : store.isPrism());
    };
    auto* registry = APPLICATION->instanceStores();
    if (!registry) {
        if (error) *error = QObject::tr("Instance stores are not available.");
        return std::nullopt;
    }

    if (!requestedId.isEmpty()) {
        const auto* store = registry->find(requestedId);
        if (!store || !store->enabled || !store->writable || !matches(*store)) {
            if (error) *error = QObject::tr("The selected instance store is not enabled, writable, or compatible with the selected format.");
            return std::nullopt;
        }
        return InstanceStoreTarget{ *store };
    }

    QList<InstanceStore> candidates;
    for (const auto& store : registry->stores()) {
        if (store.enabled && store.writable && matches(store))
            candidates.append(store);
    }
    if (candidates.isEmpty()) {
        if (error) *error = QObject::tr("No enabled writable instance store matches the selected format. Configure a matching storage location first.");
        return std::nullopt;
    }
    if (candidates.size() == 1) {
        interaction.status(QObject::tr("安装到：%1 · %2").arg(candidates.constFirst().path, candidates.constFirst().isVanillaCompatible() ? QObject::tr("原版格式") : QObject::tr("Prism / MMC 格式")));
        return InstanceStoreTarget{ candidates.constFirst() };
    }

    QJsonArray choices;
    for (const auto& store : candidates) {
        const auto format = store.isVanillaCompatible() ? QObject::tr("原版格式") : QObject::tr("Prism / MMC 格式");
        choices.append(QJsonObject{ { "id", store.id },
                                    { "name", QStringLiteral("%1 — %2 · %3").arg(store.name.isEmpty() ? store.id : store.name, store.path, format) } });
    }
    const auto selected = interaction.select(QObject::tr("请选择实例安装目录"), choices);
    if (!selected || *selected < 0 || *selected >= candidates.size()) {
        if (error) *error = QObject::tr("Instance installation was cancelled because no target store was selected.");
        return std::nullopt;
    }
    return InstanceStoreTarget{ candidates.at(*selected) };
}

QString versionIdFromPrismRoot(const QString& prismRoot, const QString& requested)
{
    if (!requested.trimmed().isEmpty())
        return requested.trimmed();
    QFile patch(QDir(prismRoot).filePath(QStringLiteral("patches/net.minecraft.json")));
    if (patch.open(QIODevice::ReadOnly)) {
        QJsonParseError parseError;
        const auto object = QJsonDocument::fromJson(patch.readAll(), &parseError).object();
        const auto version = object.value(QStringLiteral("version")).toString().trimmed();
        if (!version.isEmpty()) return version;
        const auto id = object.value(QStringLiteral("id")).toString().trimmed();
        if (!id.isEmpty()) return id;
    }
    QFile pack(QDir(prismRoot).filePath(QStringLiteral("mmc-pack.json")));
    if (pack.open(QIODevice::ReadOnly)) {
        QJsonParseError parseError;
        const auto components = QJsonDocument::fromJson(pack.readAll(), &parseError).object().value(QStringLiteral("components")).toArray();
        for (const auto& value : components) {
            const auto component = value.toObject();
            if (component.value(QStringLiteral("uid")).toString() == QStringLiteral("net.minecraft"))
                return component.value(QStringLiteral("version")).toString().trimmed();
        }
    }
    return {};
}

QJsonObject createInstance(const QJsonObject& parameters, UserInteraction& interaction, LauncherApi& api)
{
    const auto source = parameters.value("source").toString().trimmed();
    const auto type = parameters.value("type").toString().trimmed().toLower();
    const auto name = parameters.value("name").toString().trimmed();
    const auto group = parameters.value("group").toString();
    const auto icon = parameters.value("icon").toString("default");

    InstanceTask* rawTask = nullptr;
    if (!source.isEmpty()) {
        const QFileInfo localFile(source);
        QUrl url = localFile.exists() && localFile.isFile() ? QUrl::fromLocalFile(localFile.absoluteFilePath())
                                                             : QUrl(source, QUrl::TolerantMode);
        if (!url.isValid() || (url.scheme().isEmpty() && !url.isLocalFile()))
            return OperationService::failure(QObject::tr("The instance source is not a valid path or URL."), 2);
        rawTask = new InstanceImportTask(url);
    } else if (type == "server") {
        const auto executable = parameters.value("executable").toString();
        QStringList arguments;
        if (parameters.value("arguments").isArray()) {
            for (const auto& argument : parameters.value("arguments").toArray())
                arguments.append(argument.toString());
        } else {
            arguments = parameters.value("arguments").toString().split(' ', Qt::SkipEmptyParts);
        }
        rawTask = new ServerInstanceCreationTask(executable, arguments);
    } else {
        const auto versionName = parameters.value("version").toString().trimmed();
        if (versionName.isEmpty())
            return OperationService::failure(QObject::tr("A Minecraft version is required."), 2);
        if (!ApiSupport::identifier(versionName))
            return OperationService::failure(QObject::tr("Invalid Minecraft version."), 2);

        interaction.status(QObject::tr("Loading Minecraft version %1...").arg(versionName));
        QString metadataError;
        const auto index = APPLICATION->metadataIndex();
        if (!ApiSupport::wait(api, index->loadVersion("net.minecraft", versionName), interaction, metadataError))
            return OperationService::failure(metadataError);
        const auto version = index->get("net.minecraft", versionName);
        if (!version || !version->isLoaded())
            return OperationService::failure(QObject::tr("Minecraft version not found: %1").arg(versionName), 2);

        const auto requestedLoader = parameters.value("loader").toString();
        if (requestedLoader.isEmpty()) {
            rawTask = new VanillaCreationTask(version);
        } else {
            const auto loader = loaderUid(requestedLoader);
            auto loaderVersionName = parameters.value("loaderVersion").toString().trimmed();
            if (loaderVersionName.isEmpty())
                return OperationService::failure(QObject::tr("loaderVersion is required when loader is specified."), 2);
            if (!ApiSupport::identifier(loader) || !ApiSupport::identifier(loaderVersionName))
                return OperationService::failure(QObject::tr("Invalid loader or loader version."), 2);
            interaction.status(QObject::tr("Loading %1 version %2...").arg(loader, loaderVersionName));
            if (!ApiSupport::wait(api, index->loadVersion(loader, loaderVersionName), interaction, metadataError))
                return OperationService::failure(metadataError);
            const auto loaderVersion = index->get(loader, loaderVersionName);
            if (!loaderVersion || !loaderVersion->isLoaded())
                return OperationService::failure(QObject::tr("Loader version not found: %1:%2").arg(loader, loaderVersionName), 2);
            if (Meta::Cleanroom::isUid(loader)) {
                // Cleanroom is distributed as a complete instance package;
                // creating a vanilla profile with only the loader component
                // omits the package and patch files required at launch.
                rawTask = new InstanceImportTask(QUrl(Meta::Cleanroom::packageUrl(loaderVersion->descriptor())));
            } else {
                rawTask = new VanillaCreationTask(version, loader, loaderVersion);
            }
        }
    }

    if (!rawTask)
        return OperationService::failure(QObject::tr("Could not create an instance task."));
    if (!name.isEmpty())
        rawTask->setName(name);
    rawTask->setGroup(group);
    rawTask->setIcon(icon);

    /* target-store installation is handled by the shared writer below. */
    return installInstanceTaskToStore(api, rawTask, parameters, interaction);
}

QJsonObject stopInstance(const QJsonObject& parameters, bool force)
{
    const auto reference = parameters.value("instance").toString();
    auto instance = findInstance(reference);
    if (!instance)
        return OperationService::failure(QObject::tr("Instance not found: %1").arg(reference), 2);
    if (!instance->isRunning())
        return OperationService::success(QJsonObject{ { "id", instance->id() }, { "running", false }, { "stopped", false } });

    bool stopped = false;
    if (auto server = dynamic_cast<ServerInstance*>(instance)) {
        // Server instances have a real graceful path (the configured console
        // command) and a separate process-tree kill path for instance.kill.
        stopped = server->stopServer(force);
    } else {
        // Use the controller when present, or the instance's launch task for
        // detached API launches.
        stopped = APPLICATION->kill(instance);
    }
    if (!stopped)
        return OperationService::failure(force ? QObject::tr("The instance could not be killed.")
                                               : QObject::tr("The instance could not be stopped."));
    return OperationService::success(QJsonObject{ { "id", instance->id() }, { "running", instance->isRunning() }, { "stopped", true } });
}

QString instanceFolderPath(BaseInstance* instance, QString kind, QString& error)
{
    kind = kind.trimmed().toLower();
    QString path = instance->instanceRoot();
    if (kind == "game")
        path = instance->gameRoot();
    else if (kind == "mods")
        path = instance->modsRoot();
    else if (kind == "screenshots")
        path = QDir(instance->gameRoot()).filePath("screenshots");
    else if (kind == "saves")
        path = QDir(instance->gameRoot()).filePath("saves");
    else if (kind == "resourcepacks" || kind == "texturepacks" || kind == "shaderpacks" || kind == "datapacks")
        path = QDir(instance->gameRoot()).filePath(kind);
    else if (kind == "coremods" || kind == "nilmods" || kind == "schematics" || kind == "customplayermodels") {
        const auto folder = kind == "coremods" ? "coremods" : kind == "nilmods" ? "nilmods" : kind == "schematics" ? "schematics" : "custom-models";
        path = QDir(instance->gameRoot()).filePath(folder);
    } else if (kind == "yesstevemodels") {
        if (auto* minecraft = dynamic_cast<MinecraftInstance*>(instance))
            path = QDir(minecraft->instanceConfigFolder()).filePath("yes_steve_model/custom");
    } else if (kind == "logs") {
        const auto paths = instance->getLogFileSearchPaths();
        if (!paths.isEmpty())
            path = paths.first();
    } else if (kind != "root") {
        error = QObject::tr("Unknown instance folder kind: %1").arg(kind);
        return {};
    }
    QDir().mkpath(path);
    return path;
}

QJsonObject openInstanceFolder(const QJsonObject& parameters)
{
    auto instance = findInstance(parameters.value("instance").toString());
    if (!instance)
        return OperationService::failure(QObject::tr("Instance not found: %1").arg(parameters.value("instance").toString()), 2);
    QString error;
    const auto path = instanceFolderPath(instance, parameters.value("kind").toString("root"), error);
    if (!error.isEmpty())
        return OperationService::failure(error, 2);
    const bool opened = DesktopServices::openPath(path, true);
    return opened ? OperationService::success(QJsonObject{ { "path", path } })
                  : OperationService::failure(QObject::tr("Could not open instance folder: %1").arg(path));
}

QJsonObject setInstanceIcon(const QJsonObject& parameters)
{
    auto instance = findInstance(parameters.value("instance").toString());
    if (!instance)
        return OperationService::failure(QObject::tr("Instance not found: %1").arg(parameters.value("instance").toString()), 2);
    auto key = parameters.value("icon").toString().trimmed();
    const auto source = parameters.value("path").toString().trimmed();
    bool imported = false;
    if (!source.isEmpty()) {
        if (!QFileInfo(source).isFile())
            return OperationService::failure(QObject::tr("Icon file not found: %1").arg(source), 2);
        auto name = parameters.value("name").toString().trimmed();
        if (name.isEmpty())
            name = QFileInfo(source).fileName();
        if (!IconUtils::isIconSuffix(QFileInfo(name).suffix().toLower()))
            return OperationService::failure(QObject::tr("Unsupported icon file type: %1").arg(name), 2);
        APPLICATION->icons()->installIcon(source, name);
        key = QFileInfo(name).completeBaseName();
        imported = QFileInfo(QDir(APPLICATION->icons()->getDirectory()).filePath(name)).exists();
    }
    if (key.isEmpty() || (!imported && APPLICATION->icons()->getIcon(key).isNull()))
        return OperationService::failure(QObject::tr("Unknown or invalid icon: %1").arg(key), 2);
    instance->setIconKey(key);
    instance->saveNow();
    return OperationService::success(QJsonObject{ { "id", instance->id() }, { "icon", key } });
}

QJsonObject setInstanceNotes(const QJsonObject& parameters)
{
    auto instance = findInstance(parameters.value("instance").toString());
    if (!instance)
        return OperationService::failure(QObject::tr("Instance not found: %1").arg(parameters.value("instance").toString()), 2);
    instance->setNotes(parameters.value("notes").toString());
    instance->saveNow();
    return OperationService::success(QJsonObject{ { "id", instance->id() }, { "notes", instance->notes() } });
}

QJsonObject verifyInstance(LauncherApi& api, const QJsonObject& parameters, UserInteraction& interaction)
{
    auto instance = dynamic_cast<MinecraftInstance*>(findInstance(parameters.value("instance").toString()));
    if (!instance)
        return OperationService::failure(QObject::tr("Minecraft instance not found: %1").arg(parameters.value("instance").toString()), 2);

    auto* settings = instance->settings();
    const bool hadOverride = settings->get("OverrideAssetVerification").toBool();
    const int savedMode = settings->get("AssetVerificationMode").toInt();
    settings->set("OverrideAssetVerification", true);
    settings->set("AssetVerificationMode", static_cast<int>(AlwaysVerify));

    auto task = new AssetUpdateTask(instance, true);
    QString error;
    const bool successful = waitForTask(task, interaction, &error, &api);

    settings->set("OverrideAssetVerification", hadOverride);
    if (hadOverride)
        settings->set("AssetVerificationMode", savedMode);
    else
        settings->reset("AssetVerificationMode");

    if (!successful)
        return OperationService::failure(error);
    if (parameters.value("trust").toBool()) {
        settings->set("AssetIntegrityTrusted", true);
        instance->saveNow();
    }
    return OperationService::success(QJsonObject{ { "id", instance->id() }, { "verified", true },
        { "trusted", parameters.value("trust").toBool() } });
}

QJsonObject createInstanceShortcut(const QJsonObject& parameters)
{
    auto instance = findInstance(parameters.value("instance").toString());
    if (!instance)
        return OperationService::failure(QObject::tr("Instance not found: %1").arg(parameters.value("instance").toString()), 2);
    ShortcutUtils::Shortcut shortcut;
    shortcut.instance = instance;
    shortcut.name = parameters.value("name").toString(instance->name()).trimmed();
    shortcut.iconKey = parameters.value("icon").toString();
    shortcut.targetString = "instance";
    shortcut.target = ShortcutTarget::Other;
    for (const auto& argument : parameters.value("extraArgs").toArray())
        shortcut.extraArgs.append(argument.toString());

    const auto target = parameters.value("target").toString("path").trimmed().toLower();
    bool created = false;
    if (target == "desktop") {
        shortcut.target = ShortcutTarget::Desktop;
        created = ShortcutUtils::createInstanceShortcutOnDesktop(shortcut);
    } else if (target == "applications") {
        shortcut.target = ShortcutTarget::Applications;
        created = ShortcutUtils::createInstanceShortcutInApplications(shortcut);
    } else {
        const auto path = parameters.value("path").toString().trimmed();
        if (path.isEmpty())
            return OperationService::failure(QObject::tr("A shortcut path or target is required."), 2);
        created = ShortcutUtils::createInstanceShortcut(shortcut, path);
    }
    return created ? OperationService::success(QJsonObject{ { "name", shortcut.name }, { "target", target } })
                   : OperationService::failure(QObject::tr("Could not create instance shortcut."));
}

QJsonObject iconPayload(BaseInstance* inst)
{
    auto icon = APPLICATION->icons()->getIcon(inst->iconKey());
    if (icon.isNull()) {
        // The CLI does not initialize a desktop icon theme.
        const auto key = inst->iconKey() == "default" ? QStringLiteral("grass") : inst->iconKey();
        if (!key.contains('/') && !key.contains('\\'))
            icon = QIcon(":/icons/multimc/scalable/instances/" + key + ".svg");
        if (icon.isNull()) icon = QIcon(":/icons/multimc/scalable/instances/grass.svg");
    }
    const auto pixmap = icon.pixmap(96, 96);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    if (pixmap.isNull() || !pixmap.save(&buffer, "PNG"))
        return OperationService::failure("Instance icon could not be rendered.");
    return OperationService::success(QJsonObject{ { "id", inst->id() }, { "icon", inst->iconKey() },
        { "dataUrl", "data:image/png;base64," + QString::fromLatin1(bytes.toBase64()) } });
}

QJsonObject iconLibraryPayload()
{
    // Keep the browser and Qt clients on the same source of truth.  IconList
    // contains both the bundled launcher icons and user-installed icons, with
    // user files already taking precedence through its normal directory scan.
    APPLICATION->icons()->directoryChanged(APPLICATION->icons()->getDirectory());
    QJsonArray icons;
    const auto list = APPLICATION->icons();
    for (int row = 0; row < list->rowCount(); ++row) {
        const auto modelIndex = list->index(row, 0);
        const auto key = list->data(modelIndex, Qt::UserRole).toString();
        if (key.isEmpty()) continue;
        const auto icon = list->getIcon(key);
        const auto pixmap = icon.pixmap(64, 64);
        QByteArray bytes;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        if (pixmap.isNull() || !pixmap.save(&buffer, "PNG")) continue;
        icons.append(QJsonObject{
            { "key", key },
            { "name", list->data(modelIndex, Qt::DisplayRole).toString() },
            { "dataUrl", "data:image/png;base64," + QString::fromLatin1(bytes.toBase64()) },
        });
    }
    return OperationService::success(QJsonObject{ { "icons", icons } });
}

}  // namespace

QJsonObject installInstanceTaskToStore(LauncherApi& api,
                                       InstanceTask* task,
                                       const QJsonObject& parameters,
                                       UserInteraction& interaction,
                                       QJsonArray warnings)
{
    ScopedUserInteraction interactionScope(interaction);
    std::unique_ptr<InstanceTask> pending(task);
    if (!pending)
        return OperationService::failure(QObject::tr("Could not create an instance task."));

    QString targetError;
    const auto target = chooseInstanceStoreTarget(parameters, interaction, &targetError);
    if (!target)
        return OperationService::failure(targetError, 2);
    if (parameters.value(QStringLiteral("type")).toString().trimmed().compare(QStringLiteral("server"), Qt::CaseInsensitive) == 0 && target->isVanilla())
        return OperationService::failure(QObject::tr("Server instances can only be installed into a Prism / MMC store."), 2);

    QSet<QString> previousIds;
    for (int i = 0; i < APPLICATION->instances()->count(); ++i)
        previousIds.insert(APPLICATION->instances()->at(i)->id());
    const auto requestedName = pending->name();
    const auto requestedGroup = pending->group();

    if (target->isVanilla()) {
        const auto install = [&]() -> QJsonObject {
            QTemporaryDir staging;
            if (!staging.isValid())
                return OperationService::failure(QObject::tr("Could not create a temporary instance directory."));
            // Run the ordinary installer privately. Only the converted vanilla
            // profile is committed to the user's selected store.
            std::unique_ptr<InstanceTask> stagedTask(pending.release());
            stagedTask->setStagingPath(staging.path());
            stagedTask->setParentSettings(APPLICATION->settings());
            stagedTask->setConfirmUpdate(false);
            auto* directoryImport = dynamic_cast<InstanceImportTask*>(stagedTask.get());
            if (directoryImport) directoryImport->preserveVanillaSource(true);
            QString error;
            if (!waitForTask(stagedTask.get(), interaction, &error, &api))
                return OperationService::failure(error);
            for (const auto& warning : stagedTask->warnings()) warnings.append(warning);
            if (directoryImport && !directoryImport->vanillaSource().isEmpty()) {
                const auto source = directoryImport->vanillaSource();
                const auto name = requestedName.isEmpty() ? QFileInfo(source).fileName() : requestedName;
                const auto versions = QDir(VanillaConverter::gameRootForStore(target->store.path)).filePath("versions");
                if (!QDir().mkpath(versions)) return OperationService::failure(QObject::tr("Could not create the selected versions directory."));
                const auto destination = QDir(versions).filePath(FS::DirNameFromString(name, versions));
                VanillaImportOptions options; options.name = name; options.group = parameters.value("group").toString(requestedGroup);
                VanillaConversionTask conversion([source, destination, options, temporary = staging.path()](const VanillaConversionFeedback& feedback) mutable {
                    options.feedback = feedback;
                    auto report = VanillaConverter::importToVanilla(source, destination, options);
                    feedback.update(QObject::tr("正在清理解压临时文件"));
                    FS::deletePath(temporary);
                    return report;
                });
                if (!waitForTask(&conversion, interaction, &error, &api)) return OperationService::failure(error);
                const auto& report = conversion.report();
                for (const auto& warning : report.warnings) warnings.append(warning);
                return OperationService::success(QJsonObject{{"name",name},{"root",destination},{"format","vanilla-compatible"},{"warnings",warnings}});
            }
            const auto prismRoot = staging.path();
            if (!QFileInfo::exists(QDir(prismRoot).filePath("instance.cfg")))
                return OperationService::failure(QObject::tr("The temporary Minecraft instance was not created."));
            const auto versionId = versionIdFromPrismRoot(prismRoot, parameters.value("version").toString());
            if (versionId.isEmpty())
                return OperationService::failure(QObject::tr("The created instance does not contain a Minecraft version profile."));
            const auto instanceName = requestedName.isEmpty() ? stagedTask->name() : requestedName;
            const auto name = instanceName.isEmpty() ? versionId : instanceName;
            const auto versionsRoot = QDir(VanillaConverter::gameRootForStore(target->store.path)).filePath("versions");
            if (!QDir().mkpath(versionsRoot))
                return OperationService::failure(QObject::tr("Could not create the selected instance store."));
            const auto instanceId = FS::DirNameFromString(name, versionsRoot);
            const auto destination = QDir(versionsRoot).filePath(instanceId);
            VanillaExportOptions options;
            options.versionId = instanceId;
            options.isolated = true;
            options.name = name;
            options.group = parameters.value("group").toString(requestedGroup);
            if (!resolveVanillaExport(api, prismRoot, options, interaction, error))
                return OperationService::failure(error);
            VanillaConversionTask conversion([prismRoot, destination, options](const VanillaConversionFeedback& feedback) mutable {
                options.feedback = feedback;
                auto report = VanillaConverter::exportToVanilla(prismRoot, destination, options);
                feedback.update(QObject::tr("正在清理解压临时文件"));
                FS::deletePath(prismRoot);
                return report;
            });
            if (!waitForTask(&conversion, interaction, &error, &api)) return OperationService::failure(error);
            const auto& report = conversion.report();
            for (const auto& warning : report.warnings) warnings.append(warning);
            return OperationService::success(QJsonObject{ { "name", name }, { "root", destination },
                                                           { "format", "vanilla-compatible" }, { "warnings", warnings } });
        };
        auto result = install();

        // Discover the committed external profile after discarding temporary
        // MMC data, then return its stable ID.
        APPLICATION->instances()->loadList();
        if (!result.value("ok").toBool())
            return result;
        const auto data = result.value("data").toObject();
        const auto destination = data.value("root").toString();
        for (int i = 0; i < APPLICATION->instances()->count(); ++i) {
            auto* instance = APPLICATION->instances()->at(i);
            if (!previousIds.contains(instance->id()) && QDir::cleanPath(instance->instanceRoot()) == QDir::cleanPath(destination)) {
                auto updated = data;
                updated.insert("id", instance->id());
                updated.insert("name", instance->name());
                return OperationService::success(updated);
            }
        }
        return result;
    }

    Task::Ptr wrapped(APPLICATION->instances()->wrapInstanceTask(pending.release(), target->store.path));
    QString error;
    if (!waitForTask(wrapped.get(), interaction, &error, &api))
        return OperationService::failure(error);
    for (const auto& warning : wrapped->warnings()) warnings.append(warning);
    APPLICATION->instances()->saveNow();
    for (int i = 0; i < APPLICATION->instances()->count(); ++i) {
        auto* instance = APPLICATION->instances()->at(i);
        if (!previousIds.contains(instance->id()))
            return OperationService::success(QJsonObject{ { "id", instance->id() },
                                                           { "name", instance->name() },
                                                           { "root", instance->instanceRoot() },
                                                           { "format", "prism" },
                                                           { "warnings", warnings } });
    }
    return OperationService::success(QJsonObject{ { "name", parameters.value(QStringLiteral("name")).toString() },
                                                   { "format", "prism" },
                                                   { "warnings", warnings } });
}

void registerInstanceApiOperations(LauncherApi& api)
{
    api.registerOperation({ "instance.get", "Read one complete instance object for frontend adapters.",
                            ApiSupport::schema({ { "instance", ApiSupport::string("Installed instance ID, managed name, or display name.") } }, { "instance" }), "instances" },
                           [](const QJsonObject& p, UserInteraction&) {
                               const auto instance = LauncherApiModel::InstanceObject::resolve(p.value("instance").toString());
                               if (!instance) return OperationService::failure("Instance not found.", 2);
                               return OperationService::success(instance.snapshot());
                           });

    api.registerOperation({ "instance.managed-pack.info", "Read managed pack identity and update source.",
        ApiSupport::schema({ { "instance", ApiSupport::string("Installed instance ID.") } }, { "instance" }), "instances" },
        [](const QJsonObject& p, UserInteraction&) {
            auto inst = findInstance(p.value("instance").toString());
            if (!inst) return OperationService::failure("Instance not found.", 2);
            return OperationService::success(QJsonObject{ { "managed", inst->isManagedPack() }, { "type", inst->getManagedPackType() },
                { "projectId", inst->getManagedPackID() }, { "versionId", inst->getManagedPackVersionID() },
                { "name", inst->getManagedPackName() }, { "versionName", inst->getManagedPackVersionName() },
                { "url", inst->settings()->get("ManagedPackURL").toString() } });
        });
    api.registerOperation({ "instance.managed-pack.versions", "List provider versions for a managed pack.",
        ApiSupport::schema({ { "instance", ApiSupport::string("Installed instance ID.") }, { "offline", ApiSupport::boolean() } }, { "instance" }), "instances" },
        [&api](const QJsonObject& p, UserInteraction& i) {
            auto inst = findInstance(p.value("instance").toString());
            if (!inst || !inst->isManagedPack()) return OperationService::failure("Managed instance not found.", 2);
            const auto type = inst->getManagedPackType();
            if (type == "ftb" || type == "atlauncher")
                return api.execute("modpack.versions", { { "provider", type }, { "projectId", inst->getManagedPackID() },
                    { "offline", p.value("offline").toBool() } }, i);
            if (type != "flame" && type != "modrinth") return OperationService::failure("This managed pack uses a local file or update URL.", 2);
            if (p.value("offline").toBool()) return OperationService::failure("This provider does not expose a cached offline catalog.", 2);
            return api.execute("resource.versions", { { "provider", type == "flame" ? "curseforge" : "modrinth" }, { "kind", "modpacks" },
                { "projectId", inst->getManagedPackID() }, { "includeChangelog", true } }, i);
        });
    api.registerOperation({ "instance.managed-pack.update", "Update an existing managed pack from a selected pack URL or local archive through the existing staged import workflow.",
        ApiSupport::schema({ { "instance", ApiSupport::string("Installed instance ID.") }, { "source", ApiSupport::string("Selected version download URL or local pack file.") },
            { "versionId", ApiSupport::string("Selected provider version ID; resolves the archive when source is omitted.") }, { "confirm", ApiSupport::boolean() } }, { "instance", "confirm" }), "instances", true },
        [&api](const QJsonObject& p, UserInteraction& interaction) {
            auto inst = findInstance(p.value("instance").toString());
            if (!inst || !inst->isManagedPack()) return OperationService::failure("Managed instance not found.", 2);
            if (inst->isRunning() || !p.value("confirm").toBool()) return OperationService::failure("Stop the instance and confirm the pack update.", 2);
            auto source = p.value("source").toString();
            if (source.isEmpty()) {
                const auto versionId = p.value("versionId").toString();
                if (versionId.isEmpty()) return OperationService::failure("Provide source or versionId for the pack update.", 2);
                const auto type = inst->getManagedPackType();
                if (type != "flame" && type != "modrinth") return OperationService::failure("This pack update requires an archive source.", 2);
                const auto versions = api.execute("instance.managed-pack.versions", { { "instance", inst->id() } }, interaction);
                if (!versions.value("ok").toBool()) return versions;
                QJsonObject selected;
                for (const auto& value : versions.value("data").toArray())
                    if (value.toObject().value("versionId").toString() == versionId) { selected = value.toObject(); break; }
                if (selected.isEmpty()) return OperationService::failure("Pack version not found.", 2);
                source = selected.value("downloadUrl").toString();
                if (source.isEmpty() && type == "flame") {
                    const auto project = api.execute("resource.project", { { "provider", "curseforge" }, { "kind", "modpacks" },
                        { "projectId", inst->getManagedPackID() } }, interaction);
                    if (!project.value("ok").toBool()) return project;
                    QList<BlockedMod> files{ { selected.value("fileName").toString(),
                        project.value("data").toObject().value("websiteUrl").toString() + "/download/" + versionId,
                        selected.value("hash").toString(), false, {} } };
                    QString error;
                    if (!resolveHeadlessBlockedMods(files, selected.value("hashType").toString(), error)) return OperationService::failure(error);
                    source = files.first().localPath;
                }
                if (source.isEmpty()) return OperationService::failure("Provider did not supply a downloadable pack.", 2);
            }
            const auto url = QFileInfo(source).isFile() ? QUrl::fromLocalFile(QFileInfo(source).absoluteFilePath()) : QUrl(source);
            if (!url.isValid() || (url.scheme() != "http" && url.scheme() != "https" && !url.isLocalFile())) return OperationService::failure("Invalid pack source.", 2);
            const auto id = inst->id();
            auto raw = new InstanceImportTask(url, nullptr, { { "pack_id", inst->getManagedPackID() },
                { "pack_version_id", p.value("versionId").toString() }, { "original_instance_id", id } });
            raw->setName(inst->name()); raw->setGroup(APPLICATION->instances()->getInstanceGroup(id)); raw->setIcon(inst->iconKey());
            raw->setConfirmUpdate(false);
            auto task = APPLICATION->instances()->wrapInstanceTask(raw);
            QString error;
            if (!waitForTask(task, interaction, &error, &api)) return OperationService::failure(error);
            APPLICATION->instances()->saveNow();
            return OperationService::success(QJsonObject{ { "instance", id }, { "updated", true } });
        });
    const auto instance = stringProperty("Instance ID, managed name, or display name.");
    api.registerOperation({ "instance.create", "Create a vanilla, server, or imported instance.",
                            objectSchema({ { "type", stringProperty("vanilla or server; defaults to vanilla.") },
                                           { "version", stringProperty("Minecraft version for vanilla.") },
                                           { "loader", stringProperty("Optional loader id or alias.") },
                                           { "loaderVersion", stringProperty("Optional loader version.") },
                                           { "source", stringProperty("Local path or URL to an instance pack.") },
                                           { "targetStoreId", stringProperty("Optional enabled writable Prism or vanilla-compatible Store ID for the new instance.") },
                                           { "targetFormat", stringProperty("prism or vanilla-compatible.") },
                                           { "name", stringProperty("Instance name.") }, { "group", stringProperty("Instance group.") },
                                           { "icon", stringProperty("Icon key.") }, { "executable", stringProperty("Server executable.") },
                                           { "arguments", QJsonObject{ { "description", "Server arguments." } } } }) },
                         [&api](const QJsonObject& parameters, UserInteraction& interaction) { return createInstance(parameters, interaction, api); });
    api.registerOperation({ "instance.import", "Import an instance pack into a selected Store.",
                            objectSchema({ { "source", stringProperty("Local path or URL to an instance pack.") },
                                           { "targetStoreId", stringProperty("Optional enabled writable Prism or vanilla-compatible Store ID.") },
                                           { "targetFormat", stringProperty("prism or vanilla-compatible; interactive choice when omitted without targetStoreId.") },
                                           { "name", stringProperty("Optional instance name.") },
                                           { "group", stringProperty("Optional instance group.") },
                                           { "icon", stringProperty("Optional instance icon key.") } },
                                          { "source" }) },
                         [&api](const QJsonObject& parameters, UserInteraction& interaction) { return createInstance(parameters, interaction, api); });
    api.registerOperation({ "instance.stop", "Stop a running instance.", objectSchema({ { "instance", instance } }, { "instance" }) },
                           [](const QJsonObject& parameters, UserInteraction&) { return stopInstance(parameters, false); });
    api.registerOperation({ "instance.kill", "Force-stop a running instance.", objectSchema({ { "instance", instance } }, { "instance" }),
                            {}, true },
                           [](const QJsonObject& parameters, UserInteraction&) { return stopInstance(parameters, true); });
    api.registerOperation({ "instance.open-folder", "Open an instance directory.",
                            objectSchema({ { "instance", instance }, { "kind", stringProperty("root, game, mods, coremods, nilmods, resourcepacks, texturepacks, shaderpacks, datapacks, saves, screenshots, schematics, customplayermodels, yesstevemodels, or logs.") } },
                                          { "instance" }) },
                           [](const QJsonObject& parameters, UserInteraction&) { return openInstanceFolder(parameters); });
    api.registerOperation({ "instance.folder-path", "Resolve an instance subdirectory path and create it when missing.",
                            objectSchema({ { "instance", instance }, { "kind", stringProperty("root, game, mods, coremods, nilmods, resourcepacks, texturepacks, shaderpacks, datapacks, saves, screenshots, schematics, customplayermodels, yesstevemodels, or logs.") } },
                                          { "instance" }) },
                           [](const QJsonObject& parameters, UserInteraction&) {
                               auto instance = findInstance(parameters.value("instance").toString());
                               if (!instance) return OperationService::failure(QObject::tr("Instance not found: %1").arg(parameters.value("instance").toString()), 2);
                               QString error;
                               const auto path = instanceFolderPath(instance, parameters.value("kind").toString("root"), error);
                               if (!error.isEmpty()) return OperationService::failure(error, 2);
                               return OperationService::success(QJsonObject{ { "path", path } });
                           });
    api.registerOperation({ "instance.icon", "Read the instance icon as a bounded PNG data URL for non-Qt frontends.",
                            objectSchema({ { "instance", instance } }, { "instance" }) },
                           [](const QJsonObject& p, UserInteraction&) {
                               auto inst = findInstance(p.value("instance").toString());
                               if (!inst) return OperationService::failure("Instance not found.", 2);
                               // Qt refreshes IconList through its file watcher after an
                               // imported pack copies a custom icon. API clients can ask
                               // immediately after the task completes, before that watcher
                               // event is delivered, so perform the same directory scan
                               // synchronously before reading the icon.
                               APPLICATION->icons()->directoryChanged(APPLICATION->icons()->getDirectory());
                               return iconPayload(inst);
                           });
    api.registerOperation({ "instance.icons", "Read every instance icon in one bounded batch for fast dashboard startup.",
                            objectSchema({}) },
                           [](const QJsonObject&, UserInteraction&) {
                               APPLICATION->icons()->directoryChanged(APPLICATION->icons()->getDirectory());
                               QJsonArray icons;
                               for (int i = 0; i < APPLICATION->instances()->count(); ++i) {
                                   auto payload = iconPayload(APPLICATION->instances()->at(i));
                                   if (payload.value("ok").toBool()) icons.append(payload.value("data"));
                               }
                               return OperationService::success(QJsonObject{ { "icons", icons } });
                           });
     api.registerOperation({ "instance.icon-library", "Read the launcher icon library, including bundled and installed icons.",
                             objectSchema({}) },
                            [](const QJsonObject&, UserInteraction&) { return iconLibraryPayload(); });
    api.registerOperation({ "instance.set-icon", "Assign a built-in or imported icon to an instance.",
                            objectSchema({ { "instance", instance }, { "icon", stringProperty("Icon key.") },
                                           { "path", stringProperty("Icon file to import.") },
                                           { "name", stringProperty("Imported icon filename.") } },
                                          { "instance" }) },
                           [](const QJsonObject& parameters, UserInteraction&) { return setInstanceIcon(parameters); });
    api.registerOperation({ "instance.set-notes", "Set the notes shown for an instance.",
                            objectSchema({ { "instance", instance }, { "notes", stringProperty("Instance notes.") } }, { "instance", "notes" }) },
                           [](const QJsonObject& parameters, UserInteraction&) { return setInstanceNotes(parameters); });
    api.registerOperation({ "instance.verify", "Verify and repair Minecraft asset integrity; optionally trust the instance after a successful full check.",
                            objectSchema({ { "instance", instance }, { "trust", ApiSupport::boolean() } }, { "instance" }) },
                           [&api](const QJsonObject& parameters, UserInteraction& interaction) {
                               return verifyInstance(api, parameters, interaction);
                           });
    api.registerOperation({ "instance.create-shortcut", "Create a launcher shortcut for an instance.",
                            objectSchema({ { "instance", instance }, { "target", stringProperty("desktop, applications, or path.") },
                                           { "path", stringProperty("Shortcut path when target=path.") },
                                           { "name", stringProperty("Shortcut name.") },
                                           { "extraArgs", QJsonObject{ { "type", "array" }, { "items", stringProperty("Launcher argument.") } } } },
                                          { "instance" }) },
                           [](const QJsonObject& parameters, UserInteraction&) { return createInstanceShortcut(parameters); });
}
