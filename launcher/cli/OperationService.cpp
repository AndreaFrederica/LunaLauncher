// SPDX-License-Identifier: MPL-2.0

#include "OperationService.h"
#include "api/ResourceModelSupport.h"
#include "cli/ScopedUserInteraction.h"

#include <QCryptographicHash>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QTimer>
#include <QUrlQuery>
#include <QVariant>
#include <QUuid>

#include "Application.h"
#include "LauncherResources.h"
#include "DataMigrationTask.h"
#include "api/LauncherApiObjects.h"
#include "BaseInstance.h"
#include "FileSystem.h"
#include "InstanceCopyPrefs.h"
#include "InstanceCopyTask.h"
#include "InstanceImportTask.h"
#include "InstanceList.h"
#include "InstanceStore.h"
#include "VanillaCompatibleProvider.h"
#include "VanillaInstance.h"
#include "VanillaConversion.h"
#include "SyncProfile.h"
#include "LaunchController.h"
#include "java/JavaInstall.h"
#include "java/JavaInstallList.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/OneSixVersionFormat.h"
#include "server/ServerInstance.h"
#include "minecraft/mod/PluginFolderModel.h"
#include "minecraft/mod/ModFolderModel.h"
#include "minecraft/PackProfile.h"
#include "meta/Index.h"
#include "meta/Version.h"
#include "minecraft/auth/AccountData.h"
#include "minecraft/auth/AccountList.h"
#include "minecraft/auth/AuthFlow.h"
#include "minecraft/auth/LocalYggdrasil.h"
#include "minecraft/auth/MinecraftAccount.h"
#include "minecraft/launch/MinecraftTarget.h"
#include "minecraft/mod/Resource.h"
#include "minecraft/mod/ResourceFolderModel.h"
#include "modplatform/ModApiMirror.h"
#include "ui/themes/ThemeManager.h"
#include "modplatform/flame/CurseForgeDownloadPageService.h"
#include "net/ApiDownload.h"
#include "net/Download.h"
#include "net/NetJob.h"
#include "net/Aria2Manager.h"
#include "net/PclDownloadLibrary.h"
#include "net/RawHeaderProxy.h"
#include "net/Upload.h"
#include "settings/Setting.h"
#include "settings/SettingsObject.h"
#include "tasks/SequentialTask.h"
#include "tasks/Task.h"

namespace {

QString accountTypeName(AccountType type)
{
    switch (type) {
        case AccountType::MSA:
            return "microsoft";
        case AccountType::Offline:
            return "offline";
        case AccountType::Yggdrasil:
            return "yggdrasil";
        case AccountType::UnifiedPass:
            return "unified-pass";
    }
    return "unknown";
}

QString accountStateName(AccountState state)
{
    switch (state) {
        case AccountState::Unchecked:
            return "unchecked";
        case AccountState::Offline:
            return "offline";
        case AccountState::Working:
            return "working";
        case AccountState::Online:
            return "online";
        case AccountState::Disabled:
            return "disabled";
        case AccountState::Errored:
            return "errored";
        case AccountState::Expired:
            return "expired";
        case AccountState::Gone:
            return "gone";
    }
    return "unknown";
}

QString hashName(int algorithm)
{
    return algorithm == 2 ? QStringLiteral("md5") : QStringLiteral("sha1");
}

QCryptographicHash::Algorithm hashAlgorithm(const QString& name)
{
    return name == "md5" ? QCryptographicHash::Md5 : QCryptographicHash::Sha1;
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

MinecraftAccountPtr findAccount(const QString& reference, int* row = nullptr)
{
    const auto accounts = APPLICATION->accounts();
    for (int i = 0; i < accounts->count(); ++i) {
        const auto account = accounts->at(i);
        if (account->profileId() == reference || account->internalId() == reference ||
            account->profileName().compare(reference, Qt::CaseInsensitive) == 0) {
            if (row)
                *row = i;
            return account;
        }
    }
    return nullptr;
}

std::shared_ptr<ResourceFolderModel> findResourceModel(BaseInstance* instance, QString kind, const QString& world = {})
{
    kind = kind.toLower().remove('-').remove('_');
    if (!world.isEmpty()) return ApiSupport::worldDataPacks(instance, kind, world);
    if (auto server = dynamic_cast<ServerInstance*>(instance)) {
        if (kind == "mods") return server->loaderModList();
        if (kind == "plugins") return server->pluginList();
        return nullptr;
    }
    static const QMap<QString, int> indexes{ { "mods", 0 },           { "coremods", 1 },           { "nilmods", 2 },
                                             { "resourcepacks", 3 },  { "texturepacks", 4 },       { "shaderpacks", 5 },
                                             { "yesstevemodels", 6 }, { "customplayermodels", 7 }, { "schematics", 8 },
                                             { "datapacks", 9 } };
    if (!indexes.contains(kind))
        return nullptr;
    const auto client = dynamic_cast<MinecraftInstance*>(instance);
    if (!client) return nullptr;
    const auto lists = client->resourceLists();
    const auto index = indexes.value(kind);
    return index < lists.size() ? lists.at(index) : nullptr;
}

[[maybe_unused]] QString resourceTypeName(ResourceType type)
{
    switch (type) {
        case ResourceType::ZIPFILE:
            return "archive";
        case ResourceType::SINGLEFILE:
            return "file";
        case ResourceType::FOLDER:
            return "folder";
        case ResourceType::LITEMOD:
            return "litemod";
        case ResourceType::UNKNOWN:
            return "unknown";
    }
    return "unknown";
}

std::unique_ptr<Resource> findResource(const std::shared_ptr<ResourceFolderModel>& model, const QString& reference)
{
    const auto entries = model->dir().entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const auto& entry : entries) {
        if (entry.fileName() == ".index")
            continue;
        auto resource = std::make_unique<Resource>(entry);
        if (resource->internal_id() == reference || resource->fileinfo().fileName().compare(reference, Qt::CaseInsensitive) == 0 ||
            resource->getOriginalFileName().compare(reference, Qt::CaseInsensitive) == 0 ||
            resource->name().compare(reference, Qt::CaseInsensitive) == 0)
            return resource;
    }
    return {};
}

bool isSensitiveSetting(const QString& id)
{
    const auto lower = id.toLower();
    return lower.endsWith("password") || lower.endsWith("proxypass") || lower.contains("token") || lower.contains("secret") ||
           lower.endsWith("keyoverride");
}

void applyRuntimeDownloadSettings(const QString& key)
{
    if (!APPLICATION_DYN)
        return;

    const auto settings = APPLICATION->settings();
    if (key == "PclDownloadThreadLimit") {
        auto& library = PclDownloadLibrary::instance();
        if (library.isLoaded())
            library.setThreadLimit(qMax(1, settings->get(key).toInt()));
    } else if (key == "PclDownloadSpeedLimitKBps") {
        auto& library = PclDownloadLibrary::instance();
        if (library.isLoaded())
            library.setSpeedLimit(static_cast<qint64>(qMax(0, settings->get(key).toInt())) * 1024);
    }

    if (key == "NumberOfConcurrentDownloads")
        NetJob::applyRuntimeSettings();

    if (key == "Aria2MaxConcurrentDownloads" || key == "NumberOfConcurrentDownloads" ||
        key == "Aria2FollowLauncherDownloadLimits" || key == "Aria2PollInterval") {
        Net::Aria2Manager::instance()->applyRuntimeSettings();
    }

    if (key == "ProxyType" || key == "ProxyAddr" || key == "ProxyPort" || key == "ProxyUser" || key == "ProxyPass") {
        APPLICATION->updateProxySettings(settings->get("ProxyType").toString(), settings->get("ProxyAddr").toString(),
                                         settings->get("ProxyPort").toInt(), settings->get("ProxyUser").toString(),
                                         settings->get("ProxyPass").toString());
    }
}

Resource* findLoadedResource(const std::shared_ptr<ResourceFolderModel>& model, const QString& reference)
{
    for (auto* resource : model->allResources()) {
        if (resource->internal_id() == reference || resource->fileinfo().fileName().compare(reference, Qt::CaseInsensitive) == 0 ||
            resource->getOriginalFileName().compare(reference, Qt::CaseInsensitive) == 0 ||
            resource->name().compare(reference, Qt::CaseInsensitive) == 0)
            return resource;
    }
    return nullptr;
}

QString settingCategory(const QString& id)
{
    // Keep this list limited to values that are consumed exclusively by the
    // legacy Qt widgets. Settings which affect launching, downloads, or the
    // launcher service stay in a shared category so alternative UIs can use
    // them through the API.
    static const QSet<QString> qtOnly{
        "IconTheme", "ApplicationTheme", "BackgroundCat", "ThemeDownloadSources", "ThemeDownloadSource",
        "LastUsedGroupForNewInstance", "MenuBarInsteadOfToolBar", "UseNewUI", "InstanceToolbarFixedWidth", "ShowNewsBar", "StatusBarVisible",
        "ShowServerPreview",
        "ServerPreviewDynamicWidth", "ShowTerracottaToolBar", "ShowYukariConnectToolBar", "ShowTerracottaStatusBar",
        "ShowYukariConnectStatusBar", "ConsoleFont", "ConsoleFontSize", "ConsoleColorScheme", "JsonEditor",
        "Language", "UseSystemLocale", "TheCat", "CatOpacity", "CatFit", "ToolbarsLocked", "InstSortMode",
        "InstRenamingMode", "SelectedInstance", "MainWindowState", "MainWindowGeometry", "ConsoleWindowState",
        "ConsoleWindowGeometry", "SettingsGeometry", "PagedGeometry", "NewInstanceGeometry", "UpdateDialogGeometry",
        "NewsGeometry", "ModDownloadGeometry", "RPDownloadGeometry", "TPDownloadGeometry", "ShaderDownloadGeometry",
        "DataPackDownloadGeometry", "WorldManagementGeometry"
    };
    static const QSet<QString> paths{
        "InstanceDir", "CentralModsDir", "IconsDir", "DownloadsDir", "SkinsDir", "JavaDir", "GlobalDataPacksPath",
        "JavaPath", "CustomGLFWPath", "CustomOpenALPath", "Aria2ExecutablePath", "CurseForgeExternalToolPath",
        "FTBAppInstancesPath", "InstanceDirBookmark", "CentralModsDirBookmark", "IconsDirBookmark",
        "DownloadsDirBookmark", "SkinsDirBookmark", "JavaDirBookmark"
    };
    static const QSet<QString> network{
        "ProxyType", "ProxyAddr", "ProxyPort", "ProxyUser", "ProxyPass", "RequestTimeout", "UserAgentOverride",
        "Aria2ProxyType", "Aria2ProxyAddr", "Aria2ProxyPort", "Aria2ProxyUser", "Aria2ProxyPass"
    };
    static const QSet<QString> downloads{
        "NumberOfConcurrentTasks", "NumberOfConcurrentDownloads", "NumberOfManualRetries", "AssetCacheExpiryDays",
        "DownloadsDirWatchRecursive", "MoveModsFromDownloadsDir", "ModMetadataDisabled", "ModDependenciesDisabled", "McModSearchEnabled",
        "SkipModpackUpdatePrompt", "GlobalDataPacksEnabled", "ModDownloadLoaders", "OverrideModDownloadLoaders",
        "AssetVerificationMode", "DownloadBackend", "PclDownloadFallbackToQt", "PclDownloadThreadLimit",
        "PclDownloadSpeedLimitKBps", "Aria2Enabled", "Aria2AutoInstall", "Aria2FallbackToQt",
        "Aria2FollowLauncherDownloadLimits", "Aria2MaxConcurrentDownloads", "Aria2MaxConnectionPerServer",
        "Aria2Split", "Aria2MinSplitSize", "Aria2FileAllocation", "Aria2Continue", "Aria2RpcPort",
        "Aria2PollInterval", "Aria2ExtraArgs", "Aria2UseLauncherProxy", "CurseForgeDownloadBrowser",
        "ShowModIncompat", "FallbackMRBlockedMods", "MetaURLOverride", "LocalServiceCache", "ResourceURLOverride", "DownloadMirrorType",
        "ModrinthMirror", "CurseForgeMirror", "ModrinthMirrorURL", "CurseForgeMirrorURL", "LibrariesURL", "LegacyFMLLibsURLOverride", "MojangDownloadsMirrorURL",
        "ModrinthDownloadMirrorURL",
        "CFKeyOverride", "FlameKeyOverride", "ModrinthToken"
    };
    static const QSet<QString> java{
        "OverrideJavaLocation", "IgnoreJavaCompatibility", "JvmArgs", "IgnoreJavaWizard", "AutomaticJavaSwitch",
        "AutomaticJavaDownload", "PermGen", "LowMemWarning", "MinMemAlloc", "MaxMemAlloc", "JavaSignature",
        "JavaArchitecture", "JavaRealArchitecture", "JavaVersion", "JavaVendor", "LastHostname",
        "UserAskedAboutAutomaticJavaDownload"
    };
    static const QSet<QString> launch{
        "LaunchMaximized", "MinecraftWinWidth", "MinecraftWinHeight", "CloseAfterLaunch", "QuitAfterGameStop",
        "JoinServerOnLaunch", "JoinServerOnLaunchAddress", "JoinWorldOnLaunch", "InstanceAccountId",
        "PreLaunchCommand", "WrapperCommand", "PostExitCommand", "Env", "LastOfflinePlayerName", "MSAClientIDOverride",
        "JProfilerPath", "JVisualVMPath", "MCEditPath", "Profiler"
    };
    static const QSet<QString> console{
        "ShowConsole", "AutoCloseConsole", "ShowConsoleOnError", "LogPrePostOutput", "ConsoleMaxLines", "ConsoleOverflowStop"
    };
    static const QSet<QString> compatibility{
        "OnlineFixes", "UseNativeGLFW", "UseNativeOpenAL", "EnableFeralGamemode", "EnableMangoHud", "UseDiscreteGpu",
        "UseZink", "OverrideLegacySettings"
    };
    static const QSet<QString> time{ "ShowGameTime", "RecordGameTime", "ShowGlobalGameTime", "ShowGameTimeWithoutDays" };
    static const QSet<QString> instance{
        "name", "iconKey", "notes", "WindowTitle", "InstanceType", "linkedInstances", "shortcuts", "ManagedPack",
        "ManagedPackType", "ManagedPackID", "ManagedPackName", "ManagedPackVersionID", "ManagedPackVersionName",
        "ManagedPackURL", "lastLaunchTime", "totalTimePlayed", "lastTimePlayed", "AutomaticJava"
    };
    static const QSet<QString> instanceOverrides{
        "OverrideJava", "OverrideJavaLocation", "OverrideWindowTitle", "OverrideGameTime", "OverrideCommands",
        "OverrideLaunchCmd", "OverrideConsole"
    };
    if (qtOnly.contains(id)) return "qt-ui";
    if (instance.contains(id)) return "instance";
    if (instanceOverrides.contains(id) || id.startsWith("Override")) return "instance-overrides";
    if (paths.contains(id)) return "paths";
    if (network.contains(id)) return "network";
    if (downloads.contains(id)) return "downloads";
    if (java.contains(id)) return "java";
    if (console.contains(id)) return "console";
    if (launch.contains(id)) return "launch";
    if (compatibility.contains(id)) return "compatibility";
    if (time.contains(id)) return "playtime";
    return "advanced";
}

QString settingSurface(const QString& id)
{
    return settingCategory(id) == "qt-ui" ? QStringLiteral("qt") : QStringLiteral("shared");
}

QJsonValue variantToJson(const QVariant& value)
{
    if (!value.isValid() || value.isNull())
        return QJsonValue::Null;
    if (value.metaType().flags().testFlag(QMetaType::IsEnumeration))
        return value.toInt();
    if (value.metaType().id() == QMetaType::QByteArray)
        return QString::fromLatin1(value.toByteArray().toBase64());
    const auto converted = QJsonValue::fromVariant(value);
    return converted.isUndefined() ? QJsonValue(value.toString()) : converted;
}

QString variantTypeName(const QVariant& value, const QVariant& fallback)
{
    const auto type = value.isValid() ? value.metaType() : fallback.metaType();
    return type.isValid() && type.name() ? QString::fromLatin1(type.name()) : QStringLiteral("QVariant");
}

QJsonObject describeSetting(SettingsObject* settings, const std::shared_ptr<Setting>& setting, bool reveal)
{
    const auto value = settings->get(setting->id());
    const auto defaultValue = setting->defValue();
    const bool sensitive = isSensitiveSetting(setting->id());
    const bool redacted = sensitive && !reveal;
    return { { "key", setting->id() },
             { "type", variantTypeName(value, defaultValue) },
             { "value", redacted ? QJsonValue(QStringLiteral("***")) : variantToJson(value) },
             { "default", redacted ? QJsonValue(QStringLiteral("***")) : variantToJson(defaultValue) },
             { "isDefault", value == defaultValue },
             { "sensitive", sensitive },
             { "redacted", redacted },
             { "category", settingCategory(setting->id()) },
             { "surface", settingSurface(setting->id()) } };
}

bool convertSettingValue(const QJsonValue& input, const QVariant& reference, QVariant* output, QString* error)
{
    if (!reference.isValid()) {
        *output = input.toVariant();
        return true;
    }

    const auto type = reference.metaType();
    const auto fail = [error, type] {
        if (error)
            *error = QObject::tr("The value cannot be converted to %1.").arg(QString::fromLatin1(type.name()));
        return false;
    };

    if (type.id() == QMetaType::QString) {
        if (input.isString()) {
            *output = input.toString();
        } else if (input.isBool()) {
            *output = input.toBool() ? QStringLiteral("true") : QStringLiteral("false");
        } else if (input.isDouble()) {
            *output = QString::number(input.toDouble(), 'g', 16);
        } else {
            return fail();
        }
        return true;
    }
    if (type.id() == QMetaType::Bool) {
        if (input.isBool()) {
            *output = input.toBool();
            return true;
        }
        if (input.isDouble() && (input.toDouble() == 0 || input.toDouble() == 1)) {
            *output = input.toDouble() == 1;
            return true;
        }
        const auto text = input.toString().trimmed().toLower();
        if (text == "true" || text == "yes" || text == "on" || text == "1") {
            *output = true;
            return true;
        }
        if (text == "false" || text == "no" || text == "off" || text == "0") {
            *output = false;
            return true;
        }
        return fail();
    }
    if (type.id() == QMetaType::QStringList) {
        if (!input.isArray())
            return fail();
        QStringList values;
        for (const auto& item : input.toArray()) {
            if (!item.isString())
                return fail();
            values.append(item.toString());
        }
        *output = values;
        return true;
    }
    if (type.id() == QMetaType::QByteArray) {
        if (!input.isString())
            return fail();
        *output = QByteArray::fromBase64(input.toString().toLatin1());
        return true;
    }

    QVariant converted = input.toVariant();
    if (type.flags().testFlag(QMetaType::IsEnumeration)) {
        bool ok = input.isDouble();
        const auto value = input.isDouble() ? static_cast<int>(input.toDouble()) : input.toString().toInt(&ok);
        if (!ok)
            return fail();
        converted = value;
    }
    if (!converted.convert(type))
        return fail();
    *output = converted;
    return true;
}

}  // namespace

OperationService::OperationService(QObject* parent) : QObject(parent) {}

void OperationService::cancelCurrent()
{
    if (m_currentTask)
        m_currentTask->abort();
}

QJsonObject OperationService::success(const QJsonValue& data)
{
    return { { "ok", true }, { "exitCode", 0 }, { "data", data } };
}

QJsonObject OperationService::failure(const QString& message, int exitCode)
{
    return { { "ok", false }, { "exitCode", exitCode }, { "error", message } };
}

QJsonObject OperationService::execute(const QString& operation, const QJsonObject& parameters, UserInteraction& interaction)
{
    ScopedUserInteraction interactionScope(interaction);
    if (operation == "instance.list")
        return listInstances();
    if (operation == "account.list")
        return listAccounts();
    if (operation == "account.profile.create" || operation == "account.profile.check-name") {
        const auto account = findAccount(parameters.value("account").toString());
        const auto name = parameters.value("name").toString();
        if (!account || account->accountType() != AccountType::MSA || account->accessToken().isEmpty()) return failure("An authenticated Microsoft account is required.", 2);
        if (!QRegularExpression("^[A-Za-z0-9_]{3,16}$").match(name).hasMatch()) return failure("Invalid Minecraft profile name.", 2);
        if (operation == "account.profile.create") {
            if (account->hasProfile()) return failure("This account already has a Java profile.", 2);
            const auto result = setupMicrosoftProfile(account, name, interaction);
            APPLICATION->accounts()->saveList(); return result;
        }
        auto [request, response] = Net::Download::makeByteArray(QUrl("https://api.minecraftservices.com/minecraft/profile/name/" + name + "/available"));
        request->addHeaderProxy(std::make_unique<Net::RawHeaderProxy>(QList<Net::HeaderPair>{ { "Authorization", "Bearer " + account->accessToken().toUtf8() } }));
        request->setNetwork(APPLICATION->network());
        QString error;
        if (!waitForTask(request.get(), interaction, &error)) return failure(error);
        const auto status = QJsonDocument::fromJson(*response).object().value("status").toString();
        if (status.isEmpty()) return failure("Invalid profile availability response.");
        return success(QJsonObject{ { "name", name }, { "status", status }, { "available", status == "AVAILABLE" } });
    }
    if (operation == "account.set-default")
        return setDefaultAccount(parameters);
    if (operation == "account.configure")
        return configureAccount(parameters);
    if (operation == "account.remove")
        return removeAccount(parameters);
    if (operation == "account.refresh")
        return refreshAccount(parameters, interaction);
    if (operation == "instance.info")
        return instanceInfo(parameters);
    if (operation == "instance.rename")
        return renameInstance(parameters);
    if (operation == "instance.group")
        return groupInstance(parameters);
    if (operation == "instance.copy")
        return copyInstance(parameters, interaction);
    if (operation == "instance.update")
        return updateInstance(parameters, interaction);
    if (operation == "instance.delete")
        return deleteInstance(parameters);
    if (operation == "instance.undo-delete")
        return undoDeleteInstance();
    if (operation == "resource.list")
        return listResources(parameters);
    if (operation == "resource.install")
        return installResource(parameters, interaction);
    if (operation == "resource.enable")
        return setResourceEnabled(parameters, true);
    if (operation == "resource.disable")
        return setResourceEnabled(parameters, false);
    if (operation == "resource.remove")
        return removeResource(parameters);
    if (operation == "java.list")
        return listJava(interaction);
    if (operation == "settings.list")
        return listSettings(parameters);
    if (operation == "settings.get")
        return getSetting(parameters);
    if (operation == "settings.set")
        return setSetting(parameters, interaction);
    if (operation == "settings.reset")
        return resetSetting(parameters);
    if (operation == "settings.export")
        return exportSettings(parameters);
    if (operation == "settings.import")
        return importSettings(parameters);
    if (operation == "startup.status")
        return success(APPLICATION->startupStatus());
    if (operation == "startup.data-location")
        return success(APPLICATION->dataLocationStatus());
    if (operation == "startup.set-data-location")
        return setDataLocation(parameters, interaction);
    if (operation == "instance-store.list")
        return success(QJsonObject{ { "stores", APPLICATION->instanceStoresStatus() } });
    if (operation == "instance-store.add" || operation == "instance-store.update") {
        QString error;
        if (!APPLICATION->addOrUpdateInstanceStore(parameters, &error)) return failure(error, 2);
        return success(QJsonObject{ { "stores", APPLICATION->instanceStoresStatus() } });
    }
    if (operation == "instance-store.remove") {
        QString error;
        if (!APPLICATION->removeInstanceStore(parameters.value("id").toString(), &error)) return failure(error, 2);
        return success(QJsonObject{ { "stores", APPLICATION->instanceStoresStatus() } });
    }
    if (operation == "instance.store-transfer") {
        const auto instanceId = parameters.value("instance").toString();
        const auto targetStoreId = parameters.value("targetStoreId").toString();
        const auto mode = parameters.value("mode").toString("copy");
        auto* instance = APPLICATION->instances()->getInstanceById(instanceId);
        auto* targetStore = APPLICATION->instanceStores()->find(targetStoreId);
        if (!instance || !targetStore) return failure(tr("The instance or target store does not exist."), 2);
        if (!targetStore->isPrism() || !targetStore->enabled || !targetStore->writable) return failure(tr("The target store is not a writable Prism store."), 2);
        if (instance->isRunning()) return failure(tr("Stop the instance before transferring it."), 2);
        const auto source = QFileInfo(instance->instanceRoot()).canonicalFilePath();
        if (!QFileInfo::exists(QDir(source).filePath(QStringLiteral("instance.cfg"))))
            return failure(tr("Only Prism instances can use direct Store transfer; convert the external instance first."), 2);
        const auto destination = QFileInfo(QDir(targetStore->path).filePath(QFileInfo(source).fileName())).absoluteFilePath();
        if (source.isEmpty() || destination.isEmpty() || QFileInfo(source).canonicalFilePath() == QFileInfo(destination).canonicalFilePath() || QFileInfo::exists(destination))
            return failure(tr("The destination already exists or is invalid."), 2);
        if (!QDir().mkpath(targetStore->path)) return failure(tr("Could not create the target store."), 2);
        const auto staging = destination + QStringLiteral(".transfer-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        FS::copy copier(source, staging);
        copier.overwrite(false);
        if (!copier()) { FS::deletePath(staging); return failure(tr("Could not copy the instance to the target store."), 2); }
        if (!QDir().rename(staging, destination)) { FS::deletePath(staging); return failure(tr("Could not commit the transferred instance."), 2); }
        if (mode.compare("move", Qt::CaseInsensitive) == 0 && !FS::deletePath(source)) return failure(tr("The instance was copied, but the source could not be removed."), 2);
        APPLICATION->instances()->loadList();
        return success(QJsonObject{ { "source", source }, { "destination", destination }, { "mode", mode } });
    }
    if (operation == "vanilla-instance.list") {
        const auto storeId = parameters.value("storeId").toString();
        QJsonArray instances;
        for (const auto& store : APPLICATION->instanceStores()->stores()) {
            if (!store.enabled || !store.isVanillaCompatible() || (!storeId.isEmpty() && store.id != storeId)) continue;
            for (const auto& instance : VanillaCompatibleProvider::scan(store.id, store.path))
                instances.append(QJsonObject{ { "id", instance.stableId }, { "storeId", instance.storeId }, { "name", instance.name }, { "root", instance.root }, { "dialect", instance.dialect }, { "provenance", instance.provenance.toJson() }, { "versionId", instance.versionId }, { "profileVersionId", instance.profileVersionId }, { "minecraftVersion", instance.minecraftVersion }, { "loaderType", instance.loaderType }, { "loaderVersion", instance.loaderVersion }, { "gameRoot", instance.layout.gameRoot }, { "isolated", instance.layout.isolated }, { "writable", instance.writable } });
        }
        return success(QJsonObject{ { "instances", instances } });
    }
    if (operation == "vanilla-instance.provenance") {
        const auto reference = parameters.value("instance").toString();
        const auto* instance = APPLICATION->instances()->getInstanceById(reference);
        if (!instance)
            instance = APPLICATION->instances()->getInstanceByManagedName(reference);
        if (!instance) {
            for (int i = 0; i < APPLICATION->instances()->count(); ++i) {
                if (APPLICATION->instances()->at(i)->name().compare(reference, Qt::CaseInsensitive) == 0) {
                    instance = APPLICATION->instances()->at(i);
                    break;
                }
            }
        }
        const auto* vanilla = dynamic_cast<const VanillaInstance*>(instance);
        if (!vanilla)
            return failure(tr("The selected instance is not a vanilla-compatible external instance."), 2);
        return success(QJsonObject{ { "instance", vanilla->id() }, { "root", vanilla->instanceRoot() },
                                    { "provenance", vanilla->descriptor().provenance.toJson() } });
    }
    if (operation == "external-launchers.list-recognizers") {
        QJsonArray recognizers;
        for (const auto& recognizer : ExternalLauncherRecognizerRegistry::instance().recognizers())
            recognizers.append(QJsonObject{ { "id", recognizer.id }, { "displayName", recognizer.displayName },
                                            { "builtIn", recognizer.builtIn }, { "iconId", recognizer.iconId },
                                            { "priority", recognizer.priority } });
        return success(QJsonObject{ { "recognizers", recognizers } });
    }
    if (operation == "external-launchers.icon") {
        const auto id = parameters.value("id").toString().trimmed();
        static const QRegularExpression validId(QStringLiteral("^[A-Za-z0-9._-]{1,80}$"));
        if (!validId.match(id).hasMatch()) return failure(tr("Invalid recognizer ID."), 2);
        auto iconId = id;
        for (const auto& recognizer : ExternalLauncherRecognizerRegistry::instance().recognizers())
            if (recognizer.id.compare(id, Qt::CaseInsensitive) == 0 && validId.match(recognizer.iconId).hasMatch()) iconId = recognizer.iconId;
        for (const auto& directory : LauncherResources::searchPaths(QStringLiteral("external-launchers/%1").arg(iconId))) {
            QFile file(QDir(directory).filePath(QStringLiteral("icon.png")));
            if (!file.open(QIODevice::ReadOnly) || file.size() > 512 * 1024) continue;
            return success(QJsonObject{ { "id", id }, { "mime", "image/png" },
                                        { "dataUrl", QStringLiteral("data:image/png;base64,") + QString::fromLatin1(file.readAll().toBase64()) } });
        }
        return failure(tr("Recognizer icon not found: %1").arg(id), 2);
    }
    if (operation == "instance.convert.import") {
        VanillaImportOptions options; options.name = parameters.value("name").toString(); options.group = parameters.value("group").toString(); options.copyGameContent = parameters.value("copyGameContent").toBool(true);
        auto target = parameters.value("target").toString();
        const auto storeId = parameters.value("targetStoreId").toString().trimmed();
        if (!storeId.isEmpty()) {
            const auto* store = APPLICATION->instanceStores()->find(storeId);
            if (!store || !store->isPrism() || !store->enabled || !store->writable)
                return failure(tr("The selected target store is not an enabled writable Prism store."), 2);
            const auto instanceName = options.name.isEmpty() ? QFileInfo(parameters.value("source").toString()).fileName() : options.name;
            target = QDir(store->path).filePath(FS::DirNameFromString(instanceName, store->path));
        }
        if (target.isEmpty()) {
            const auto* store = APPLICATION->instanceStores()->defaultStore();
            if (!store)
                return failure(tr("No writable Prism Store is configured for the conversion."), 2);
            const auto instanceName = options.name.isEmpty() ? QFileInfo(parameters.value("source").toString()).fileName() : options.name;
            target = QDir(store->path).filePath(FS::DirNameFromString(instanceName, store->path));
        }
        const auto report = VanillaConverter::importToPrism(parameters.value("source").toString(), target, options);
        return report.succeeded ? success(report.toJson()) : failure(report.error.isEmpty() ? QObject::tr("Vanilla import was blocked.") : report.error, 2);
    }
    if (operation == "instance.convert.export") {
        VanillaExportOptions options; options.versionId = parameters.value("versionId").toString(); options.versionJson = parameters.value("versionJson").toObject(); options.dialect = parameters.value("dialect").toString(); options.name = parameters.value("name").toString(); options.group = parameters.value("group").toString(); options.extensionFields = parameters.value("extensionFields").toObject(); options.copyGameContent = parameters.value("copyGameContent").toBool(true);
        const auto report = VanillaConverter::exportToVanilla(parameters.value("source").toString(), parameters.value("target").toString(), options);
        return report.succeeded ? success(report.toJson()) : failure(report.error.isEmpty() ? QObject::tr("Vanilla export was blocked.") : report.error, 2);
    }
    if (operation == "vanilla.loader.install") {
        VanillaLoaderInstallOptions options;
        options.versionId = parameters.value("versionId").toString();
        options.loaderId = VanillaCompatibleProvider::loaderUidForType(parameters.value("loaderId").toString());
        options.loaderVersion = parameters.value("loaderVersion").toString();
        options.versionJson = parameters.value("versionJson").toObject();
        options.clientJar = parameters.value("clientJar").toString();
        if (options.loaderId.isEmpty())
            return failure(tr("Unsupported standard loader: %1").arg(parameters.value("loaderId").toString()), 2);
        if (options.versionJson.isEmpty()) {
            interaction.status(tr("Loading %1 version %2...").arg(options.loaderId, options.loaderVersion));
            QString metadataError;
            const auto index = APPLICATION->metadataIndex();
            const auto loadTask = index->loadVersion(options.loaderId, options.loaderVersion);
            if (!waitForTask(loadTask.get(), interaction, &metadataError))
                return failure(metadataError);
            const auto loaderVersion = index->get(options.loaderId, options.loaderVersion);
            if (!loaderVersion || !loaderVersion->isLoaded() || !loaderVersion->data())
                return failure(tr("Loader version not found: %1:%2").arg(options.loaderId, options.loaderVersion), 2);
            options.versionJson = OneSixVersionFormat::versionFileToJson(loaderVersion->data()).object();
        }
        const auto report = VanillaConverter::installLoader(parameters.value("target").toString(), options);
        return report.succeeded ? success(report.toJson()) : failure(report.error.isEmpty() ? QObject::tr("Vanilla loader installation was blocked.") : report.error, 2);
    }
    if (operation == "sync.profile.save") {
        QString error; const auto profile = SyncProfile::fromJson(parameters.value("profile").toObject(), &error);
        if (!error.isEmpty() || profile.id.isEmpty() || !SyncEngine::saveProfile(APPLICATION->dataRoot(), profile, &error)) return failure(error, 2);
        return success(QJsonObject{ { "profile", profile.toJson() } });
    }
    if (operation == "sync.profile.list") {
        QJsonArray profiles;
        const auto directory = QDir(APPLICATION->dataRoot()).filePath(QStringLiteral("sync-profiles"));
        for (const auto& file : QDir(directory).entryInfoList({ QStringLiteral("*.json") }, QDir::Files | QDir::Readable, QDir::Name)) {
            QString error;
            const auto profile = SyncEngine::loadProfile(APPLICATION->dataRoot(), file.baseName(), &error);
            if (error.isEmpty() && !profile.id.isEmpty())
                profiles.append(profile.toJson());
        }
        return success(QJsonObject{ { "profiles", profiles } });
    }
    if (operation == "sync.profile.remove") {
        const auto id = parameters.value("id").toString().trimmed();
        if (id.isEmpty() || id.contains('/') || id.contains('\\') || id == "." || id == "..")
            return failure(tr("A valid profile ID is required."), 2);
        const auto path = SyncEngine::profilePath(APPLICATION->dataRoot(), id);
        if (!QFile::remove(path) && QFileInfo::exists(path))
            return failure(tr("Could not remove sync profile."), 2);
        return success(QJsonObject{ { "id", id }, { "removed", true } });
    }
    if (operation == "sync.profile.plan" || operation == "sync.profile.apply") {
        QString error; SyncProfile profile;
        if (parameters.contains("profile")) profile = SyncProfile::fromJson(parameters.value("profile").toObject(), &error);
        else profile = SyncEngine::loadProfile(APPLICATION->dataRoot(), parameters.value("id").toString(), &error);
        if (!error.isEmpty() || profile.id.isEmpty()) return failure(error.isEmpty() ? QObject::tr("A valid sync profile is required.") : error, 2);
        auto clientRoot = parameters.value("clientRoot").toString();
        auto serverRoot = parameters.value("serverRoot").toString();
        auto* clientInstance = APPLICATION->instances()->getInstanceById(profile.clientInstanceId);
        const auto serverIds = profile.serverInstanceIds.isEmpty() ? QStringList{ profile.serverInstanceId } : profile.serverInstanceIds;
        if (clientRoot.isEmpty()) {
            if (!clientInstance) return failure(tr("The sync profile references an instance that is not loaded."), 2);
            clientRoot = clientInstance->gameRoot();
        }
        if (!serverRoot.isEmpty()) {
            const auto plan = SyncEngine::plan(profile, clientRoot, serverRoot);
            if (operation == "sync.profile.plan") return success(plan.toJson());
            if (!SyncEngine::apply(profile, plan, clientRoot, serverRoot, &error)) return failure(error, 2);
        } else {
            QJsonArray plans;
            for (const auto& serverId : serverIds) {
                const auto* serverInstance = APPLICATION->instances()->getInstanceById(serverId);
                if (!serverInstance) return failure(tr("The sync profile references an instance that is not loaded: %1").arg(serverId), 2);
                const auto plan = SyncEngine::plan(profile, clientRoot, serverInstance->gameRoot());
                if (operation == "sync.profile.plan") plans.append(plan.toJson());
                else if (!SyncEngine::apply(profile, plan, clientRoot, serverInstance->gameRoot(), &error)) return failure(error, 2);
            }
            if (operation == "sync.profile.plan") return success(QJsonObject{ { "plans", plans } });
        }
        if (!SyncEngine::saveProfile(APPLICATION->dataRoot(), profile, &error)) return failure(error, 2);
        return success(QJsonObject{ { "profile", profile.toJson() }, { "applied", true } });
    }
    if (operation == "startup.migrate")
        return migrateStartupData(parameters, interaction);
    if (operation == "startup.dismiss-migration")
        return dismissStartupMigration(parameters);
    if (operation == "startup.complete")
        return completeStartupSetup();
    if (operation == "account.login")
        return loginAccount(parameters, interaction);
    if (operation == "instance.import")
        return importInstance(parameters, interaction);
    if (operation == "instance.launch")
        return launchInstance(parameters, interaction);
    return failure(tr("Unknown operation: %1").arg(operation), 2);
}

QJsonObject OperationService::startupStatus() const
{
    return OperationService::success(APPLICATION->startupStatus());
}

QJsonObject OperationService::dataLocationStatus() const
{
    return success(APPLICATION->dataLocationStatus());
}

QJsonObject OperationService::setDataLocation(const QJsonObject& parameters, UserInteraction& interaction)
{
    if (!parameters.contains("portable")) return failure(tr("portable is required."), 2);
    const bool portable = parameters.value("portable").toBool();
    const bool copyData = parameters.value("copyData").toBool(false);
    QString error;
    if (copyData) {
        auto task = APPLICATION->createDataLocationTask(portable, &error);
        if (!task) return failure(error, 2);
        if (!waitForTask(task.get(), interaction, &error)) return failure(error);
    }
    if (!APPLICATION->finishDataLocationSwitch(portable, &error)) return failure(error);
    return success(QJsonObject{ { "status", APPLICATION->dataLocationStatus() }, { "restartRequired", true }, { "copied", copyData } });
}

QJsonObject OperationService::migrateStartupData(const QJsonObject& parameters, UserInteraction& interaction)
{
    const auto name = parameters.value("name").toString().trimmed();
    if (name.isEmpty()) return failure(tr("A migration source name is required."), 2);
    QString error;
    auto task = APPLICATION->createDataMigrationTask(name, &error);
    if (!task) return failure(error, 2);
    if (!waitForTask(task.get(), interaction, &error)) return failure(error);
    if (!APPLICATION->finishDataMigration(name, &error)) return failure(error);
    return success(QJsonObject{ { "migrated", name }, { "status", APPLICATION->startupStatus() } });
}

QJsonObject OperationService::dismissStartupMigration(const QJsonObject& parameters)
{
    const auto name = parameters.value("name").toString().trimmed();
    if (name.isEmpty()) return failure(tr("A migration source name is required."), 2);
    QString error;
    if (!APPLICATION->declineDataMigration(name, &error)) return failure(error, 2);
    return success(QJsonObject{ { "dismissed", name }, { "status", APPLICATION->startupStatus() } });
}

QJsonObject OperationService::completeStartupSetup()
{
    if (!APPLICATION->dataMigrationCandidates().isEmpty())
        return failure(tr("Complete or skip all pending data migrations first."), 2);
    // Do not mark setup complete while the stored Qt appearance points to a
    // removed managed package. The next launch must keep the wizard available
    // so the user can choose a valid Qt theme and icon theme here.
    auto* themeManager = APPLICATION->themeManager();
    if (!themeManager ||
        !themeManager->isValidApplicationTheme(APPLICATION->settings()->get("ApplicationTheme").toString()) ||
        !themeManager->isValidIconTheme(APPLICATION->settings()->get("IconTheme").toString()))
        return failure("Please choose a valid Qt application theme and icon theme before completing setup.", 2);
    APPLICATION->settings()->set("NeoUISetupComplete", true);
    return success(QJsonObject{ { "status", APPLICATION->startupStatus() } });
}

QJsonObject OperationService::listInstances()
{
    QJsonArray instances;
    auto list = APPLICATION->instances();
    for (int i = 0; i < list->count(); ++i) {
        const auto instance = list->at(i);
        QJsonObject object{ { "id", instance->id() },
                                      { "name", instance->name() },
                                      { "type", instance->typeName() },
                                      { "group", list->getInstanceGroup(instance->id()) },
                                      { "icon", instance->iconKey() },
                                      { "notes", instance->notes() },
                                      { "running", instance->isRunning() },
                                      { "managed", instance->isManagedPack() },
                                      { "lastLaunch", instance->lastLaunch() },
                                      { "playTimeSeconds", static_cast<qint64>(instance->totalTimePlayed()) },
                                      { "format", QStringLiteral("prism") } };
        if (const auto* vanilla = dynamic_cast<const VanillaInstance*>(instance)) {
            const auto& descriptor = vanilla->descriptor();
            object.insert("format", QStringLiteral("vanilla-compatible"));
            object.insert("dialect", descriptor.dialect);
            object.insert("provenance", descriptor.provenance.toJson());
            object.insert("loaderType", descriptor.loaderType);
            object.insert("loaderVersion", descriptor.loaderVersion);
            object.insert("minecraftVersion", descriptor.minecraftVersion);
            object.insert("vanillaCompatible", true);
        } else {
            object.insert("vanillaCompatible", false);
        }
        instances.append(object);
    }
    return success(instances);
}

QJsonObject OperationService::listAccounts()
{
    QJsonArray accounts;
    const auto list = APPLICATION->accounts();
    list->refreshCacheStatuses();
    const auto defaultAccount = list->defaultAccount();
    for (int i = 0; i < list->count(); ++i) {
        const auto account = list->at(i);
        const auto object = LauncherApiModel::AccountObject(account.get());
        auto snapshot = object.snapshot(account == defaultAccount);
        snapshot.insert("type", accountTypeName(account->accountType()));
        snapshot.insert("state", accountStateName(account->accountState()));
        accounts.append(snapshot);
    }
    return success(accounts);
}

QJsonObject OperationService::setDefaultAccount(const QJsonObject& parameters)
{
    const auto reference = parameters.value("account").toString();
    auto accounts = APPLICATION->accounts();
    if (reference.isEmpty() || reference == "-") {
        accounts->setDefaultAccount(nullptr);
        accounts->saveList();
        return success(QJsonObject{ { "default", QJsonValue::Null } });
    }
    const auto account = findAccount(reference);
    if (!account)
        return failure(tr("Account not found: %1").arg(reference), 2);
    accounts->setDefaultAccount(account);
    accounts->saveList();
    return success(QJsonObject{ { "id", account->profileId() }, { "profileName", account->profileName() }, { "default", true } });
}

QJsonObject OperationService::removeAccount(const QJsonObject& parameters)
{
    if (!parameters.value("confirm").toBool())
        return failure(tr("Account removal requires confirm=true."), 2);
    int row = -1;
    const auto reference = parameters.value("account").toString();
    const auto account = findAccount(reference, &row);
    if (!account)
        return failure(tr("Account not found: %1").arg(reference), 2);
    const auto profileName = account->profileName();
    auto accounts = APPLICATION->accounts();
    accounts->removeAccount(accounts->index(row, 0));
    accounts->saveList();
    return success(QJsonObject{ { "removed", profileName } });
}

QJsonObject OperationService::refreshAccount(const QJsonObject& parameters, UserInteraction& interaction)
{
    const auto reference = parameters.value("account").toString();
    const auto account = findAccount(reference);
    if (!account)
        return failure(tr("Account not found: %1").arg(reference), 2);
    auto task = account->refresh();
    connect(task.get(), &AuthFlow::authorizeWithBrowserWithExtra, this,
            [&interaction](const QString& url, const QString& code, int expiresIn) { interaction.deviceCode(url, code, expiresIn); });
    connect(task.get(), &AuthFlow::authorizeWithBrowser, this,
            [&interaction](const QUrl& url) { interaction.deviceCode(url.toString(), QString(), 0); });
    QString error;
    if (!waitForTask(task.get(), interaction, &error))
        return failure(error);
    APPLICATION->accounts()->saveList();
    return success(LauncherApiModel::AccountObject(account.get()).snapshot(APPLICATION->accounts()->defaultAccount() == account));
}

QJsonObject OperationService::instanceInfo(const QJsonObject& parameters)
{
    const auto reference = parameters.value("instance").toString();
    const auto instance = findInstance(reference);
    if (!instance)
        return failure(tr("Instance not found: %1").arg(reference), 2);
    QJsonObject data{ { "id", instance->id() },
                      { "name", instance->name() },
                      { "type", instance->typeName() },
                      { "group", APPLICATION->instances()->getInstanceGroup(instance->id()) },
                      { "root", instance->instanceRoot() },
                      { "gameRoot", instance->gameRoot() },
                      { "modsRoot", instance->modsRoot() },
                      { "icon", instance->iconKey() },
                      { "notes", instance->notes() },
                      { "running", instance->isRunning() },
                      { "managed", instance->isManagedPack() },
                      { "managedType", instance->getManagedPackType() },
                      { "managedId", instance->getManagedPackID() },
                      { "managedVersionId", instance->getManagedPackVersionID() },
                      { "lastLaunch", instance->lastLaunch() },
                      { "playTimeSeconds", static_cast<qint64>(instance->totalTimePlayed()) } };
    if (const auto minecraft = dynamic_cast<MinecraftInstance*>(instance))
        data.insert("minecraftVersion", minecraft->getPackProfile()->getComponentVersion("net.minecraft"));
    if (const auto vanilla = dynamic_cast<VanillaInstance*>(instance)) {
        const auto& descriptor = vanilla->descriptor();
        // Keep the legacy instance.info response in lockstep with
        // InstanceObject::snapshot()/instance.list.  Neo UI opens the
        // standalone editor through this operation, so omitting these
        // discriminator fields makes every external instance look like a
        // native Prism instance there.
        data.insert("format", QStringLiteral("vanilla-compatible"));
        data.insert("vanillaCompatible", true);
        data.insert("minecraftVersion", descriptor.minecraftVersion);
        data.insert("profileVersionId", descriptor.profileVersionId);
        data.insert("loaderType", descriptor.loaderType);
        data.insert("loaderVersion", descriptor.loaderVersion);
        data.insert("dialect", descriptor.dialect);
        data.insert("provenance", descriptor.provenance.toJson());
    } else {
        data.insert("format", QStringLiteral("prism"));
        data.insert("vanillaCompatible", false);
    }
    return success(data);
}

QJsonObject OperationService::renameInstance(const QJsonObject& parameters)
{
    const auto reference = parameters.value("instance").toString();
    const auto name = parameters.value("name").toString().trimmed();
    if (name.isEmpty())
        return failure(tr("A non-empty instance name is required."), 2);
    const auto instance = findInstance(reference);
    if (!instance)
        return failure(tr("Instance not found: %1").arg(reference), 2);
    instance->setName(name);
    instance->saveNow();
    return success(QJsonObject{ { "id", instance->id() }, { "name", instance->name() } });
}

QJsonObject OperationService::groupInstance(const QJsonObject& parameters)
{
    const auto reference = parameters.value("instance").toString();
    const auto instance = findInstance(reference);
    if (!instance)
        return failure(tr("Instance not found: %1").arg(reference), 2);
    auto group = parameters.value("group").toString().simplified();
    if (group == "-")
        group.clear();
    APPLICATION->instances()->setInstanceGroup(instance->id(), group);
    return success(QJsonObject{ { "id", instance->id() }, { "group", group } });
}

QJsonObject OperationService::copyInstance(const QJsonObject& parameters, UserInteraction& interaction)
{
    const auto reference = parameters.value("instance").toString();
    const auto instance = findInstance(reference);
    if (!instance)
        return failure(tr("Instance not found: %1").arg(reference), 2);
    const auto name = parameters.value("name").toString().trimmed();
    if (name.isEmpty())
        return failure(tr("A name for the copied instance is required."), 2);

    InstanceCopyPrefs prefs;
    if (parameters.contains("copySaves"))
        prefs.enableCopySaves(parameters.value("copySaves").toBool());
    if (parameters.contains("keepPlaytime"))
        prefs.enableKeepPlaytime(parameters.value("keepPlaytime").toBool());
    if (parameters.contains("copyMods"))
        prefs.enableCopyMods(parameters.value("copyMods").toBool());
    if (parameters.contains("copyResourcePacks"))
        prefs.enableCopyResourcePacks(parameters.value("copyResourcePacks").toBool());
    if (parameters.contains("copyShaderPacks"))
        prefs.enableCopyShaderPacks(parameters.value("copyShaderPacks").toBool());
    if (parameters.contains("copyScreenshots"))
        prefs.enableCopyScreenshots(parameters.value("copyScreenshots").toBool());

    QSet<QString> previousIds;
    for (int i = 0; i < APPLICATION->instances()->count(); ++i)
        previousIds.insert(APPLICATION->instances()->at(i)->id());
    auto copyTask = new InstanceCopyTask(instance, prefs);
    copyTask->setName(name);
    copyTask->setIcon(parameters.value("icon").toString(instance->iconKey()));
    copyTask->setGroup(parameters.value("group").toString(APPLICATION->instances()->getInstanceGroup(instance->id())));
    auto task = APPLICATION->instances()->wrapInstanceTask(copyTask);
    QString error;
    if (!waitForTask(task, interaction, &error))
        return failure(error);
    for (int i = 0; i < APPLICATION->instances()->count(); ++i) {
        const auto copied = APPLICATION->instances()->at(i);
        if (!previousIds.contains(copied->id()))
            return success(QJsonObject{ { "id", copied->id() }, { "name", copied->name() } });
    }
    return success(QJsonObject{ { "name", name } });
}

QJsonObject OperationService::updateInstance(const QJsonObject& parameters, UserInteraction& interaction)
{
    const auto reference = parameters.value("instance").toString();
    const auto instance = findInstance(reference);
    if (!instance)
        return failure(tr("Instance not found: %1").arg(reference), 2);
    if (instance->isRunning())
        return failure(tr("A running instance cannot be updated."), 2);
    const auto updateTasks = instance->createUpdateTask();
    if (updateTasks.isEmpty())
        return success(QJsonObject{ { "id", instance->id() }, { "updated", false } });
    auto task = makeShared<SequentialTask>(tr("Update instance %1").arg(instance->name()));
    for (const auto& updateTask : updateTasks)
        task->addTask(updateTask);
    QString error;
    if (!waitForTask(task.get(), interaction, &error))
        return failure(error);
    int syncedProfiles = 0;
    const auto profileDir = QDir(APPLICATION->dataRoot()).filePath(QStringLiteral("sync-profiles"));
    const auto profileFiles = QDir(profileDir).entryInfoList({ QStringLiteral("*.json") }, QDir::Files | QDir::Readable);
    for (const auto& profileFile : profileFiles) {
        QString profileError;
        auto profile = SyncEngine::loadProfile(APPLICATION->dataRoot(), QFileInfo(profileFile).baseName(), &profileError);
        const auto serverIds = profile.serverInstanceIds.isEmpty() ? QStringList{ profile.serverInstanceId } : profile.serverInstanceIds;
        if (!profileError.isEmpty() || !profile.autoAfterUpdate || (profile.clientInstanceId != instance->id() && !serverIds.contains(instance->id())))
            continue;
        auto* client = APPLICATION->instances()->getInstanceById(profile.clientInstanceId);
        if (!client || client->isRunning())
            continue;
        for (const auto& serverId : serverIds) {
            auto* server = APPLICATION->instances()->getInstanceById(serverId);
            if (!server || server->isRunning()) continue;
            const auto syncPlan = SyncEngine::plan(profile, client->gameRoot(), server->gameRoot());
            if (!syncPlan.valid || !syncPlan.conflicts.isEmpty()) continue;
            if (SyncEngine::apply(profile, syncPlan, client->gameRoot(), server->gameRoot(), &profileError))
                ++syncedProfiles;
        }
        SyncEngine::saveProfile(APPLICATION->dataRoot(), profile, &profileError);
    }
    return success(QJsonObject{ { "id", instance->id() }, { "updated", true }, { "autoSyncedProfiles", syncedProfiles } });
}

QJsonObject OperationService::deleteInstance(const QJsonObject& parameters)
{
    if (!parameters.value("confirm").toBool())
        return failure(tr("Instance deletion requires confirm=true."), 2);
    const auto reference = parameters.value("instance").toString();
    const auto instance = findInstance(reference);
    if (!instance)
        return failure(tr("Instance not found: %1").arg(reference), 2);
    if (instance->isRunning())
        return failure(tr("A running instance cannot be deleted."), 2);
    const auto linked = APPLICATION->instances()->getLinkedInstancesById(instance->id());
    if (!linked.isEmpty() && !parameters.value("force").toBool())
        return failure(tr("The instance is linked by: %1. Use force=true to continue.").arg(linked.join(", ")), 2);
    const auto id = instance->id();
    const auto name = instance->name();
    if (parameters.value("permanent").toBool()) {
        const auto root = instance->instanceRoot();
        APPLICATION->instances()->deleteInstance(id);
        if (QFileInfo::exists(root))
            return failure(tr("The instance could not be completely deleted."));
        return success(QJsonObject{ { "id", id }, { "name", name }, { "permanent", true } });
    }
    if (!APPLICATION->instances()->trashInstance(id))
        return failure(tr("The instance could not be moved to the trash. Use permanent=true to delete it permanently."));
    return success(QJsonObject{ { "id", id }, { "name", name }, { "permanent", false } });
}

QJsonObject OperationService::undoDeleteInstance()
{
    if (!APPLICATION->instances()->trashedSomething())
        return failure(tr("There is no trashed instance to restore."), 2);
    if (!APPLICATION->instances()->undoTrashInstance())
        return failure(tr("The trashed instance or its shortcuts could not be completely restored."));
    return success(QJsonObject{ { "restored", true } });
}

QJsonObject OperationService::listResources(const QJsonObject& parameters)
{
    const auto reference = parameters.value("instance").toString();
    const auto instance = findInstance(reference);
    if (!instance)
        return failure(tr("Minecraft instance not found: %1").arg(reference), 2);
    const auto kind = parameters.value("kind").toString();
    const auto model = findResourceModel(instance, kind, parameters.value("world").toString());
    if (!model)
        return failure(tr("Unknown or unavailable resource kind: %1").arg(kind), 2);

    const bool includeIcons = parameters.value("includeIcons").toBool(true);
    ApiSupport::ensureResourceModelReady(model, includeIcons);
    QJsonArray resources;
    for (const auto* resource : model->allResources())
        resources.append(ApiSupport::resourceSnapshot(*resource, includeIcons));
    return success(resources);
}

QJsonObject OperationService::installResource(const QJsonObject& parameters, UserInteraction& interaction)
{
    const auto reference = parameters.value("instance").toString();
    const auto instance = findInstance(reference);
    if (!instance)
        return failure(tr("Minecraft instance not found: %1").arg(reference), 2);
    if (instance->isRunning())
        return failure(tr("Resources cannot be installed while the instance is running."), 2);
    const auto kind = parameters.value("kind").toString();
    const auto model = findResourceModel(instance, kind, parameters.value("world").toString());
    if (!model)
        return failure(tr("Unknown or unavailable resource kind: %1").arg(kind), 2);
    const auto source = parameters.value("source").toString();
    if (source.isEmpty())
        return failure(tr("A local path or direct URL is required."), 2);

    QString localPath;
    const QUrl url(source, QUrl::TolerantMode);
    if (url.isLocalFile()) {
        localPath = url.toLocalFile();
    } else if (QFileInfo::exists(source)) {
        localPath = QFileInfo(source).absoluteFilePath();
    } else if (url.isValid() && (url.scheme() == "http" || url.scheme() == "https")) {
        auto fileName = QFileInfo(url.path()).fileName();
        if (fileName.isEmpty())
            return failure(tr("The resource URL does not contain a file name."), 2);
        localPath = FS::getUniqueResourceName(QDir(m_downloads.path()).filePath(fileName));
        auto job = makeShared<NetJob>("CLI::DownloadResource", APPLICATION->network());
        job->addNetAction(Net::Download::makeFile(url, localPath));
        QString error;
        if (!waitForTask(job.get(), interaction, &error))
            return failure(error);
    } else {
        return failure(tr("Resource source is neither a local file nor a direct URL: %1").arg(source), 2);
    }
    if (!model->installResource(localPath))
        return failure(tr("The resource could not be installed."));
    return success(QJsonObject{ { "instance", instance->id() }, { "kind", kind }, { "fileName", QFileInfo(localPath).fileName() } });
}

QJsonObject OperationService::setResourceEnabled(const QJsonObject& parameters, bool enabled)
{
    const auto reference = parameters.value("instance").toString();
    const auto instance = findInstance(reference);
    if (!instance)
        return failure(tr("Minecraft instance not found: %1").arg(reference), 2);
    if (instance->isRunning())
        return failure(tr("Resources cannot be changed while the instance is running."), 2);
    const auto kind = parameters.value("kind").toString();
    const auto model = findResourceModel(instance, kind, parameters.value("world").toString());
    if (!model)
        return failure(tr("Unknown or unavailable resource kind: %1").arg(kind), 2);
    ApiSupport::ensureResourceModelReady(model);
    const auto resourceReference = parameters.value("resource").toString();
    auto* resource = findLoadedResource(model, resourceReference);
    std::unique_ptr<Resource> fallback;
    if (!resource) {
        fallback = findResource(model, resourceReference);
        resource = fallback.get();
    }
    if (!resource)
        return failure(tr("Resource not found: %1").arg(parameters.value("resource").toString()), 2);
    if (resource->enabled() == enabled)
        return success(QJsonObject{ { "fileName", resource->fileinfo().fileName() }, { "enabled", enabled }, { "changed", false } });
    if (!resource->enable(enabled ? EnableAction::ENABLE : EnableAction::DISABLE))
        return failure(tr("The resource could not be %1.").arg(enabled ? tr("enabled") : tr("disabled")));
    return success(QJsonObject{ { "fileName", resource->fileinfo().fileName() }, { "enabled", enabled }, { "changed", true } });
}

QJsonObject OperationService::removeResource(const QJsonObject& parameters)
{
    if (!parameters.value("confirm").toBool())
        return failure(tr("Resource removal requires confirm=true."), 2);
    const auto reference = parameters.value("instance").toString();
    const auto instance = findInstance(reference);
    if (!instance)
        return failure(tr("Minecraft instance not found: %1").arg(reference), 2);
    if (instance->isRunning())
        return failure(tr("Resources cannot be removed while the instance is running."), 2);
    const auto kind = parameters.value("kind").toString();
    const auto model = findResourceModel(instance, kind, parameters.value("world").toString());
    if (!model)
        return failure(tr("Unknown or unavailable resource kind: %1").arg(kind), 2);
    ApiSupport::ensureResourceModelReady(model);
    const auto resourceReference = parameters.value("resource").toString();
    auto* resource = findLoadedResource(model, resourceReference);
    std::unique_ptr<Resource> fallback;
    if (!resource) {
        fallback = findResource(model, resourceReference);
        resource = fallback.get();
    }
    if (!resource)
        return failure(tr("Resource not found: %1").arg(parameters.value("resource").toString()), 2);
    const auto fileName = resource->fileinfo().fileName();
    if (!resource->destroy(model->indexDir(), parameters.value("preserveMetadata").toBool(), true))
        return failure(tr("The resource could not be removed."));
    // Resource::destroy removes the file immediately, while the model keeps
    // its in-memory snapshot until the asynchronous directory refresh. Wait
    // for that refresh before returning so a following API list observes the
    // removal deterministically.
    model->update();
    if (model->isUpdating()) {
        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        QObject::connect(model.get(), &ResourceFolderModel::updateFinished, &loop, &QEventLoop::quit);
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        timeout.start(10000);
        loop.exec();
    }
    return success(QJsonObject{ { "fileName", fileName }, { "removed", true } });
}

QJsonObject OperationService::listJava(UserInteraction& interaction)
{
    auto list = APPLICATION->javalist();
    if (!list->isLoaded()) {
        auto task = list->getLoadTask();
        QString error;
        if (task && !waitForTask(task.get(), interaction, &error))
            return failure(error);
    }
    QJsonArray installations;
    for (int i = 0; i < list->count(); ++i) {
        const auto java = std::dynamic_pointer_cast<JavaInstall>(list->at(i));
        if (!java)
            continue;
        installations.append(LauncherApiModel::JavaObject(java).snapshot());
    }
    return success(installations);
}

SettingsObject* OperationService::resolveSettings(const QJsonObject& parameters, QString* error, BaseInstance** instance) const
{
    if (instance)
        *instance = nullptr;
    const auto scope = parameters.value("scope").toString("launcher").toLower();
    if (scope == "launcher" || scope == "global")
        return APPLICATION->settings();
    if (scope != "instance") {
        *error = tr("Unknown settings scope: %1").arg(scope);
        return nullptr;
    }

    const auto reference = parameters.value("instance").toString();
    if (reference.isEmpty()) {
        *error = tr("Instance settings require an instance ID or name.");
        return nullptr;
    }
    auto resolved = findInstance(reference);
    if (!resolved) {
        *error = tr("Instance not found: %1").arg(reference);
        return nullptr;
    }
    if (instance)
        *instance = resolved;
    return resolved->settings();
}

QJsonObject OperationService::listSettings(const QJsonObject& parameters)
{
    QString error;
    auto settings = resolveSettings(parameters, &error);
    if (!settings)
        return failure(error, 2);

    const auto filter = parameters.value("filter").toString();
    const bool reveal = parameters.value("reveal").toBool(false);
    QJsonArray values;
    for (const auto& key : settings->settingIds()) {
        if (!filter.isEmpty() && !key.contains(filter, Qt::CaseInsensitive))
            continue;
        values.append(describeSetting(settings, settings->getSetting(key), reveal));
    }
    return success(values);
}

QJsonObject OperationService::getSetting(const QJsonObject& parameters)
{
    QString error;
    auto settings = resolveSettings(parameters, &error);
    if (!settings)
        return failure(error, 2);
    const auto key = parameters.value("key").toString();
    const auto setting = settings->getSetting(key);
    if (!setting)
        return failure(tr("Unknown setting: %1").arg(key), 2);
    return success(describeSetting(settings, setting, parameters.value("reveal").toBool(false)));
}

QJsonObject OperationService::setSetting(const QJsonObject& parameters, UserInteraction& interaction)
{
    QString error;
    BaseInstance* instance = nullptr;
    auto settings = resolveSettings(parameters, &error, &instance);
    if (!settings)
        return failure(error, 2);
    const auto key = parameters.value("key").toString();
    const auto setting = settings->getSetting(key);
    if (!setting)
        return failure(tr("Unknown setting: %1").arg(key), 2);
    if (isSensitiveSetting(key) && parameters.value("valueFromCommandLine").toBool())
        return failure(tr("Sensitive settings must be entered interactively or with --password-stdin."), 2);

    QJsonValue input = parameters.value("value");
    if (input.isUndefined()) {
        const auto value = interaction.input(tr("Value for %1").arg(key), isSensitiveSetting(key));
        if (!value)
            return failure(tr("A value is required."), 2);
        input = *value;
    }
    const auto current = settings->get(key);
    const auto reference = current.isValid() ? current : setting->defValue();
    QVariant converted;
    if (!convertSettingValue(input, reference, &converted, &error))
        return failure(error, 2);
    if (key == "AssetIntegrityTrusted" && converted.toBool())
        return failure(tr("An instance can be added to the integrity whitelist only after instance.verify succeeds with trust=true."), 2);
    if (!settings->set(key, converted))
        return failure(tr("Could not set %1.").arg(key));
    applyRuntimeDownloadSettings(key);
    if (instance)
        instance->saveNow();
    return success(describeSetting(settings, setting, parameters.value("reveal").toBool(false)));
}

QJsonObject OperationService::resetSetting(const QJsonObject& parameters)
{
    QString error;
    BaseInstance* instance = nullptr;
    auto settings = resolveSettings(parameters, &error, &instance);
    if (!settings)
        return failure(error, 2);
    const auto key = parameters.value("key").toString();
    const auto setting = settings->getSetting(key);
    if (!setting)
        return failure(tr("Unknown setting: %1").arg(key), 2);
    settings->reset(key);
    applyRuntimeDownloadSettings(key);
    if (instance)
        instance->saveNow();
    return success(describeSetting(settings, setting, parameters.value("reveal").toBool(false)));
}

QJsonObject OperationService::configureAccount(const QJsonObject& parameters)
{
    const auto reference = parameters.value("account").toString();
    const auto account = findAccount(reference);
    if (!account)
        return failure(tr("Account not found: %1").arg(reference), 2);

    auto* data = account->accountData();
    const auto validUrl = [](const QString& value) {
        if (value.isEmpty()) return true;
        const QUrl url(value, QUrl::TolerantMode);
        return url.isValid() && !url.host().isEmpty() && (url.scheme() == "http" || url.scheme() == "https");
    };

    if (data->type == AccountType::Yggdrasil) {
        auto authUrl = data->yggdrasilConfig.authServerUrl;
        auto sessionUrl = data->yggdrasilConfig.sessionServerUrl;
        for (const auto key : { "authUrl", "sessionUrl" }) {
            if (!parameters.contains(key)) continue;
            const auto value = parameters.value(key).toString().trimmed();
            if (!validUrl(value)) return failure(tr("%1 must be an HTTP(S) URL.").arg(QString::fromLatin1(key)), 2);
            if (QString::fromLatin1(key) == "authUrl") authUrl = value;
            else sessionUrl = value;
        }
        if (!validUrl(authUrl) || !validUrl(sessionUrl)) return failure(tr("Yggdrasil authUrl and sessionUrl must be HTTP(S) URLs."), 2);
        data->yggdrasilConfig.authServerUrl = authUrl;
        data->yggdrasilConfig.sessionServerUrl = sessionUrl;
        if (parameters.contains("sourceName")) data->yggdrasilConfig.sourceName = parameters.value("sourceName").toString().trimmed();
        if (parameters.contains("authenticateEndpoint")) data->yggdrasilConfig.authenticateEndpoint = parameters.value("authenticateEndpoint").toString().trimmed();
        if (parameters.contains("refreshEndpoint")) data->yggdrasilConfig.refreshEndpoint = parameters.value("refreshEndpoint").toString().trimmed();
        if (parameters.contains("validateEndpoint")) data->yggdrasilConfig.validateEndpoint = parameters.value("validateEndpoint").toString().trimmed();
        if (parameters.contains("profileEndpoint")) data->yggdrasilConfig.profileEndpoint = parameters.value("profileEndpoint").toString().trimmed();
        if (parameters.contains("oauthTokenEndpoint")) data->yggdrasilConfig.oauthTokenEndpoint = parameters.value("oauthTokenEndpoint").toString().trimmed();
        if (parameters.contains("tokenType")) {
            const auto tokenType = parameters.value("tokenType").toString();
            if (tokenType.compare("Standard", Qt::CaseInsensitive) != 0 && tokenType.compare("OAuth", Qt::CaseInsensitive) != 0)
                return failure(tr("tokenType must be Standard or OAuth."), 2);
            data->yggdrasilConfig.tokenType = tokenType.compare("OAuth", Qt::CaseInsensitive) == 0 ? YggdrasilTokenType::OAuth : YggdrasilTokenType::Standard;
        }
        if (data->yggdrasilConfig.authServerUrl.isEmpty() || data->yggdrasilConfig.sessionServerUrl.isEmpty())
            return failure(tr("Yggdrasil accounts require both authUrl and sessionUrl."), 2);
    } else if (data->type == AccountType::UnifiedPass) {
        if (parameters.contains("serverId")) {
            const auto serverId = parameters.value("serverId").toString().trimmed();
            if (serverId.isEmpty()) return failure(tr("UnifiedPass serverId cannot be empty."), 2);
            data->unifiedPassConfig.serverId = serverId;
        }
        if (parameters.contains("baseUrl")) {
            const auto baseUrl = parameters.value("baseUrl").toString().trimmed();
            if (!validUrl(baseUrl)) return failure(tr("baseUrl must be an HTTP(S) URL."), 2);
            data->unifiedPassConfig.baseUrl = baseUrl;
        }
    } else {
        return failure(tr("This account type has no editable authentication configuration."), 2);
    }

    account->changed();
    APPLICATION->accounts()->saveList();
    return success(LauncherApiModel::AccountObject(account.get()).snapshot(APPLICATION->accounts()->defaultAccount() == account));
}

QJsonObject OperationService::exportSettings(const QJsonObject& parameters)
{
    QString error;
    auto settings = resolveSettings(parameters, &error);
    if (!settings) return failure(error, 2);
    const bool reveal = parameters.value("reveal").toBool(false);
    QJsonObject values;
    int count = 0;
    for (const auto& key : settings->settingIds()) {
        if (isSensitiveSetting(key) && !reveal) continue;
        values.insert(key, variantToJson(settings->get(key)));
        ++count;
    }
    return success(QJsonObject{ { "scope", parameters.value("scope").toString() }, { "values", values },
                                { "count", count }, { "includesSensitive", reveal } });
}

QJsonObject OperationService::importSettings(const QJsonObject& parameters)
{
    QString error;
    BaseInstance* instance = nullptr;
    auto settings = resolveSettings(parameters, &error, &instance);
    if (!settings) return failure(error, 2);
    const auto values = parameters.value("values").toObject();
    if (values.isEmpty()) return failure(tr("values must contain at least one setting."), 2);
    const bool allowSensitive = parameters.value("allowSensitive").toBool(false);
    QList<QPair<QString, QVariant>> converted;
    int skipped = 0;
    for (auto it = values.begin(); it != values.end(); ++it) {
        const auto setting = settings->getSetting(it.key());
        if (!setting) { ++skipped; continue; }
        if (isSensitiveSetting(it.key()) && !allowSensitive) return failure(tr("Sensitive setting %1 requires allowSensitive=true.").arg(it.key()), 2);
        const auto current = settings->get(it.key());
        QVariant value;
        if (!convertSettingValue(it.value(), current.isValid() ? current : setting->defValue(), &value, &error))
            return failure(tr("%1: %2").arg(it.key(), error), 2);
        converted.append({ it.key(), value });
    }
    for (const auto& pair : converted) settings->set(pair.first, pair.second);
    if (instance) instance->saveNow();
    return success(QJsonObject{ { "imported", converted.size() }, { "skippedUnknown", skipped }, { "allowSensitive", allowSensitive } });
}

bool OperationService::waitForTask(Task* task, UserInteraction& interaction, QString* error)
{
    if (!task) {
        if (error)
            *error = tr("The operation did not create a task.");
        return false;
    }
    QEventLoop loop;
    const auto previousTask = m_currentTask;
    m_currentTask = task;
    emit taskStarted(task);
    connect(task, &Task::status, &loop, [&interaction](const QString& status) { interaction.status(status); });
    connect(task, &Task::finished, &loop, &QEventLoop::quit);
    if (!task->isFinished()) {
        if (!task->isRunning())
            task->start();
        if (!task->isFinished())
            loop.exec();
    }
    if (!task->wasSuccessful() && error)
        *error = task->failReason().isEmpty() ? tr("The operation was aborted.") : task->failReason();
    m_currentTask = previousTask;
    emit taskFinished(task);
    return task->wasSuccessful();
}

QJsonObject OperationService::setupMicrosoftProfile(const MinecraftAccountPtr& account, const QString& name, UserInteraction& interaction)
{
    static const QRegularExpression permittedNames("^[a-zA-Z0-9_]{3,16}$");
    if (!permittedNames.match(name).hasMatch())
        return failure(tr("Minecraft profile names must contain 3-16 letters, digits, or underscores."), 2);

    const auto payload = QJsonDocument(QJsonObject{ { "profileName", name } }).toJson(QJsonDocument::Compact);
    auto [request, response] = Net::Upload::makeByteArray(QUrl("https://api.minecraftservices.com/minecraft/profile"), payload);
    request->addHeaderProxy(std::make_unique<Net::RawHeaderProxy>(QList<Net::HeaderPair>{
        { "Content-Type", "application/json" },
        { "Accept", "application/json" },
        { "Authorization", QString("Bearer %1").arg(account->accessToken()).toUtf8() },
    }));
    request->setNetwork(APPLICATION->network());
    QString error;
    if (!waitForTask(request.get(), interaction, &error)) {
        const auto serverMessage = QJsonDocument::fromJson(*response).object().value("errorMessage").toString();
        return failure(serverMessage.isEmpty() ? error : serverMessage);
    }

    auto refresh = account->refresh();
    if (!waitForTask(refresh.get(), interaction, &error))
        return failure(error);
    return success();
}

QJsonObject OperationService::loginAccount(const QJsonObject& parameters, UserInteraction& interaction)
{
    auto type = parameters.value("type").toString().trimmed().toLower();
    if (type == "msa" || type == "microsoft-account") type = "microsoft";
    auto username = parameters.value("username").toString();
    if (username.isEmpty() && type != "microsoft") {
        const auto value = interaction.input(tr("Username"), false);
        if (!value || value->trimmed().isEmpty())
            return failure(tr("A username is required."), 2);
        username = value->trimmed();
    }

    MinecraftAccountPtr account;
    if (type == "offline") {
        if (!APPLICATION->accounts()->canAddOfflineAccount())
            return failure(tr("You must add a Microsoft account that owns Minecraft before adding an offline account."), 2);
        account = MinecraftAccount::createOffline(username);
        const auto uuidMode = parameters.value("offlineUuidMode").toString("default").trimmed().toLower();
        if (uuidMode == "random") {
            account->accountData()->minecraftProfile.id = QUuid::createUuid().toString(QUuid::Id128);
        } else if (uuidMode == "custom") {
            const auto custom = QUuid::fromString(parameters.value("offlineUuid").toString());
            if (custom.isNull()) return failure(tr("A valid custom offline UUID is required."), 2);
            account->accountData()->minecraftProfile.id = custom.toString(QUuid::Id128);
        } else if (uuidMode != "default" && !uuidMode.isEmpty()) {
            return failure(tr("Unsupported offline UUID mode: %1").arg(uuidMode), 2);
        }
    } else if (type == "microsoft") {
        account = MinecraftAccount::createBlankMSA();
    } else if (type == "yggdrasil" || type == "unified-pass") {
        const auto password = parameters.contains("password") ? std::optional<QString>(parameters.value("password").toString()) : interaction.input(tr("Password"), true);
        if (!password)
            return failure(tr("A password is required; use an interactive terminal or --password-stdin."), 2);
        if (type == "yggdrasil") {
            const auto authUrl = parameters.value("authUrl").toString();
            const auto sessionUrl = parameters.value("sessionUrl").toString();
            const auto managedLocal = parameters.value("managedLocal").toBool(false);
            if (!managedLocal && (authUrl.isEmpty() || sessionUrl.isEmpty()))
                return failure(tr("Yggdrasil login requires authUrl and sessionUrl."), 2);
            const auto isHttpUrl = [](const QString& value) {
                const QUrl url(value, QUrl::TolerantMode);
                return url.isValid() && !url.host().isEmpty() && (url.scheme() == "http" || url.scheme() == "https");
            };
            if (!managedLocal && (!isHttpUrl(authUrl) || !isHttpUrl(sessionUrl)))
                return failure(tr("Yggdrasil authUrl and sessionUrl must be HTTP(S) URLs."), 2);
            account =
                MinecraftAccount::createYggdrasil(username, *password, authUrl, sessionUrl, parameters.value("sourceName").toString(),
                    parameters.value("refreshEndpoint").toString(), parameters.value("validateEndpoint").toString(),
                    parameters.value("authenticateEndpoint").toString(), parameters.value("profileEndpoint").toString(),
                    parameters.value("oauthTokenEndpoint").toString(), parameters.value("tokenType").toString().compare("OAuth", Qt::CaseInsensitive) == 0 ? YggdrasilTokenType::OAuth : YggdrasilTokenType::Standard);
            account->accountData()->yggdrasilConfig.managedLocal = managedLocal;
            if (managedLocal) {
                QJsonObject profile;
                QString localError;
                if (!LocalYggdrasil::instance().ensureProfile(username, *password, profile, localError))
                    return failure(localError.isEmpty() ? tr("Could not provision the local verification account.") : localError, 2);
                // Keep the launcher account bound to the profile created in the
                // local service even if the first authentication attempt is
                // interrupted. This also lets skin management repair older
                // accounts by name.
                account->accountData()->minecraftProfile.id = profile.value("id").toString();
                account->accountData()->minecraftProfile.name = profile.value("name").toString(username);
            }
        } else {
            const auto serverId = parameters.value("serverId").toString();
            if (serverId.isEmpty())
                return failure(tr("UnifiedPass login requires serverId."), 2);
            const auto baseUrl = parameters.value("baseUrl").toString().trimmed();
            if (!baseUrl.isEmpty()) {
                const QUrl url(baseUrl, QUrl::TolerantMode);
                if (!url.isValid() || url.host().isEmpty() || (url.scheme() != "http" && url.scheme() != "https"))
                    return failure(tr("UnifiedPass baseUrl must be an HTTP(S) URL."), 2);
            }
            account = MinecraftAccount::createUnifiedPass(username, *password, serverId, baseUrl);
        }
    } else {
        return failure(tr("Unsupported account type: %1").arg(type), 2);
    }

    if (type != "offline") {
        const auto requestedProfile = parameters.value("profileId").toString();
        const auto selector = [&interaction, requestedProfile](const QJsonArray& profiles) -> std::optional<int> {
            if (!requestedProfile.isEmpty()) {
                for (int i = 0; i < profiles.size(); ++i) {
                    if (profiles.at(i).toObject().value("id").toString() == requestedProfile)
                        return i;
                }
                return std::nullopt;
            }
            return interaction.select(QObject::tr("Select a game profile"), profiles);
        };
        auto task = account->login(type == "microsoft", selector);
        connect(task.get(), &AuthFlow::authorizeWithBrowserWithExtra, this,
                [&interaction](const QString& url, const QString& code, int expiresIn) { interaction.deviceCode(url, code, expiresIn); });
        connect(task.get(), &AuthFlow::authorizeWithBrowser, this,
                [&interaction](const QUrl& url) { interaction.deviceCode(url.toString(), QString(), 0); });
        QString error;
        if (!waitForTask(task.get(), interaction, &error))
            return failure(error);
        if (type == "microsoft" && !account->hasProfile()) {
            auto profileName = parameters.value("minecraftProfileName").toString();
            if (profileName.isEmpty()) {
                const auto value = interaction.input(tr("New Minecraft Java profile name"), false);
                if (value)
                    profileName = value->trimmed();
            }
            if (profileName.isEmpty())
                return failure(tr("This Microsoft account has no Java profile. Provide minecraftProfileName to create one."), 2);
            const auto setupResult = setupMicrosoftProfile(account, profileName, interaction);
            if (!setupResult.value("ok").toBool())
                return setupResult;
        }
    }

    auto accounts = APPLICATION->accounts();
    accounts->addAccount(account);
    if (!accounts->defaultAccount())
        accounts->setDefaultAccount(account);
    accounts->saveList();
    return success(LauncherApiModel::AccountObject(account.get()).snapshot(accounts->defaultAccount() == account));
}

QJsonObject OperationService::fetchJson(const QUrl& url, const QString& taskName, QJsonDocument* document, UserInteraction& interaction)
{
    auto job = makeShared<NetJob>(taskName, APPLICATION->network());
    auto [action, response] = Net::ApiDownload::makeByteArray(url);
    job->addNetAction(action);
    QString error;
    if (!waitForTask(job.get(), interaction, &error))
        return failure(error);
    QJsonParseError parseError;
    *document = QJsonDocument::fromJson(*response, &parseError);
    if (parseError.error != QJsonParseError::NoError)
        return failure(tr("The platform returned invalid JSON: %1").arg(parseError.errorString()));
    return success();
}

QJsonObject OperationService::resolvePackSource(const QString& source, UserInteraction& interaction)
{
    const QFileInfo localFile(source);
    if (localFile.exists() && localFile.isFile())
        return success(QUrl::fromLocalFile(localFile.absoluteFilePath()).toString());

    const QUrl sourceUrl(source, QUrl::TolerantMode);
    if (!sourceUrl.isValid() || sourceUrl.scheme().isEmpty())
        return failure(tr("Pack source is neither a local file nor a valid URL: %1").arg(source), 2);

    const auto host = sourceUrl.host().toLower();
    const auto segments = sourceUrl.path().split('/', Qt::SkipEmptyParts);
    if (host.endsWith("modrinth.com") && segments.size() >= 2 && segments.at(0) == "modpack") {
        const auto project = segments.at(1);
        QUrl apiUrl(ModApiMirror::modrinthBaseUrl() + "/project/" + project + "/version");
        if (segments.size() >= 4 && segments.at(2) == "version")
            apiUrl = QUrl(ModApiMirror::modrinthBaseUrl() + "/version/" + segments.at(3));
        QJsonDocument document;
        auto result = fetchJson(apiUrl, "CLI::ResolveModrinthPack", &document, interaction);
        if (!result.value("ok").toBool())
            return result;
        const auto version = document.isArray() ? document.array().at(0).toObject() : document.object();
        const auto files = version.value("files").toArray();
        if (files.isEmpty())
            return failure(tr("The Modrinth version has no downloadable files."));
        QJsonObject selected = files.at(0).toObject();
        for (const auto& file : files) {
            if (file.toObject().value("primary").toBool()) {
                selected = file.toObject();
                break;
            }
        }
        return success(selected.value("url").toString());
    }

    if (host.endsWith("curseforge.com") && segments.size() >= 5 && segments.at(0) == "minecraft" && segments.at(1) == "modpacks" &&
        segments.at(3) == "files") {
        const auto slug = segments.at(2);
        const auto fileId = segments.at(4);
        QUrl searchUrl(ModApiMirror::curseForgeBaseUrl() + "/mods/search");
        QUrlQuery query;
        query.addQueryItem("gameId", "432");
        query.addQueryItem("classId", "4471");
        query.addQueryItem("slug", slug);
        searchUrl.setQuery(query);
        QJsonDocument projectDocument;
        auto result = fetchJson(searchUrl, "CLI::ResolveCurseForgeProject", &projectDocument, interaction);
        if (!result.value("ok").toBool())
            return result;
        const auto projects = projectDocument.object().value("data").toArray();
        if (projects.isEmpty())
            return failure(tr("CurseForge project not found: %1").arg(slug));
        const auto addonId = projects.at(0).toObject().value("id").toVariant().toString();
        QJsonDocument fileDocument;
        result = fetchJson(QUrl(ModApiMirror::curseForgeBaseUrl() + QString("/mods/%1/files/%2").arg(addonId, fileId)),
                           "CLI::ResolveCurseForgeFile", &fileDocument, interaction);
        if (!result.value("ok").toBool())
            return result;
        const auto file = fileDocument.object().value("data").toObject();
        const auto downloadUrl = file.value("downloadUrl").toString();
        if (!downloadUrl.isEmpty())
            return success(downloadUrl);

        const auto hashes = file.value("hashes").toArray();
        if (hashes.isEmpty())
            return failure(tr("The restricted CurseForge file has no hash metadata."));
        const auto hash = hashes.at(0).toObject();
        const auto algorithm = hashName(hash.value("algo").toInt());
        CurseForgeDownloadPageService service(CurseForgeDownloadPageService::Provider::External);
        QString verifiedPath;
        QString serviceError;
        QEventLoop loop;
        connect(&service, &CurseForgeDownloadPageService::failed, &loop, [&](const QString& error) {
            serviceError = error;
            loop.quit();
        });
        connect(&service, &CurseForgeDownloadPageService::fileReady, &loop, [&](const QString& fileName, int, int) {
            const auto path = QDir(service.downloadDirectory()).filePath(fileName);
            QFile downloaded(path);
            if (!downloaded.open(QIODevice::ReadOnly)) {
                serviceError = downloaded.errorString();
                loop.quit();
                return;
            }
            const auto digest = QCryptographicHash::hash(downloaded.readAll(), hashAlgorithm(algorithm)).toHex();
            verifiedPath = service.acceptDownloadedFile(path, QString::fromLatin1(digest));
            if (verifiedPath.isEmpty()) {
                serviceError = tr("The external CurseForge tool returned an invalid file.");
                loop.quit();
            }
        });
        connect(&service, &CurseForgeDownloadPageService::completed, &loop, &QEventLoop::quit);
        const auto pageUrl = QString("https://www.curseforge.com/minecraft/modpacks/%1/download/%2").arg(slug, fileId);
        const CurseForgeDownloadPage page{ pageUrl, file.value("fileName").toString(), algorithm, hash.value("value").toString() };
        interaction.status(tr("Waiting for the configured CurseForge external tool..."));
        if (!service.open({ page }, "cli"))
            return failure(service.errorString());
        loop.exec();
        if (!serviceError.isEmpty())
            return failure(serviceError);
        if (verifiedPath.isEmpty())
            return failure(tr("The external CurseForge tool did not return the requested file."));
        const auto destination = QDir(m_downloads.path()).filePath(file.value("fileName").toString());
        if (!QFile::copy(verifiedPath, destination))
            return failure(tr("Could not preserve the downloaded CurseForge pack."));
        return success(QUrl::fromLocalFile(destination).toString());
    }

    return success(sourceUrl.toString());
}

QJsonObject OperationService::importInstance(const QJsonObject& parameters, UserInteraction& interaction)
{
    const auto source = parameters.value("source").toString();
    if (source.isEmpty())
        return failure(tr("A pack source is required."), 2);
    const auto resolved = resolvePackSource(source, interaction);
    if (!resolved.value("ok").toBool())
        return resolved;
    const auto resolvedUrl = QUrl(resolved.value("data").toString());
    auto importTask = new InstanceImportTask(resolvedUrl, nullptr);
    const auto name = parameters.value("name").toString();
    if (!name.isEmpty())
        importTask->setName(name);
    QString targetDirectory;
    const auto targetStoreId = parameters.value("targetStoreId").toString().trimmed();
    if (!targetStoreId.isEmpty()) {
        const auto* store = APPLICATION->instanceStores()->find(targetStoreId);
        if (!store || !store->isPrism() || !store->enabled || !store->writable)
            return failure(tr("The selected target store is not an enabled writable Prism store."), 2);
        targetDirectory = store->path;
    } else if (const auto* store = APPLICATION->instanceStores()->defaultStore()) {
        targetDirectory = store->path;
    }
    auto task = targetDirectory.isEmpty() ? APPLICATION->instances()->wrapInstanceTask(importTask)
                                          : APPLICATION->instances()->wrapInstanceTask(importTask, targetDirectory);
    QString error;
    if (!waitForTask(task, interaction, &error))
        return failure(error);
    APPLICATION->instances()->saveNow();
    return success(QJsonObject{ { "name", name }, { "source", resolvedUrl.toString() } });
}

QJsonObject OperationService::launchInstance(const QJsonObject& parameters, UserInteraction& interaction)
{
    const auto instanceId = parameters.value("instance").toString();
    auto instance = findInstance(instanceId);
    if (!instance)
        return failure(tr("Instance not found: %1").arg(instanceId), 2);

    MinecraftAccountPtr account;
    const auto modeName = parameters.value("mode").toString("normal");
    if (!QStringList{ "normal", "offline", "demo" }.contains(modeName)) return failure("mode must be normal, offline, or demo.", 2);
    const auto profile = parameters.value("profile").toString();
    const auto accountId = parameters.value("account").toString();
    if (!accountId.isEmpty() || !profile.isEmpty()) {
        account = findAccount(accountId.isEmpty() ? profile : accountId);
        if (!account) return failure("Selected account not found.", 2);
    } else {
        if (instance->settings()->get("UseAccountForInstance").toBool()) account = findAccount(instance->settings()->get("InstanceAccountId").toString());
        if (!account)
        account = APPLICATION->accounts()->defaultAccount();
    }

    const auto offlineName = parameters.value("offlineName").toString();
    auto mode = modeName == "demo" ? LaunchMode::Demo : modeName == "offline" ? LaunchMode::Offline : LaunchMode::Normal;
    if (!offlineName.isEmpty()) {
        if (!QRegularExpression("^[A-Za-z0-9_]{1,16}$").match(offlineName).hasMatch()) return failure("Invalid offline player name.", 2);
        if (mode != LaunchMode::Demo) { mode = LaunchMode::Offline; account = MinecraftAccount::createOffline(offlineName); }
    }
    if (!account && mode != LaunchMode::Demo && offlineName.isEmpty()) {
        QJsonArray choices;
        auto accounts = APPLICATION->accounts();
        for (int i = 0; i < accounts->count(); ++i) choices.append(QJsonObject{ { "id", accounts->at(i)->internalId() }, { "name", accounts->at(i)->profileName() } });
        if (!choices.isEmpty()) {
            const auto selected = interaction.select("Choose the account to launch", choices);
            if (!selected) return failure("Account selection cancelled.", 2);
            if (*selected >= 0 && *selected < accounts->count()) account = accounts->at(*selected);
        }
    }
    if (!account && mode != LaunchMode::Demo)
        return failure(tr("No account is available. Log in or pass an offline player name."), 2);
    if (account && account->accountType() == AccountType::MSA && !account->hasProfile() && mode == LaunchMode::Normal) {
        auto name = parameters.value("minecraftProfileName").toString();
        if (name.isEmpty()) {
            const auto input = interaction.input("Choose your Minecraft Java profile name", false);
            if (!input) return failure("Profile creation cancelled.", 2);
            name = *input;
        }
        const auto result = setupMicrosoftProfile(account, name, interaction);
        if (!result.value("ok").toBool()) return result;
        APPLICATION->accounts()->saveList();
    }

    MinecraftTarget::Ptr target;
    if (!parameters.value("server").toString().isEmpty())
        target.reset(new MinecraftTarget(MinecraftTarget::parse(parameters.value("server").toString(), false)));
    else if (!parameters.value("world").toString().isEmpty())
        target.reset(new MinecraftTarget(MinecraftTarget::parse(parameters.value("world").toString(), true)));

    auto controller = new LaunchController();
    const auto profilerName = parameters.value("profiler").toString();
    if (!profilerName.isEmpty()) {
        const auto profiler = APPLICATION->profilers().value(profilerName);
        if (!profiler) { delete controller; return failure(tr("Unknown profiler."), 2); }
        controller->setProfiler(profiler.get());
    }
    controller->setParent(this);
    controller->setHeadless(true);
    controller->setInstance(instance);
    controller->setLaunchMode(mode);
    controller->setOfflineName(offlineName);
    controller->setAccountToUse(account);
    controller->setTargetToJoin(target);
    controller->setReauthenticateHandler([this, &interaction, parameters](const MinecraftAccountPtr& expired,
                                                                          const QString& reason) -> MinecraftAccountPtr {
        interaction.status(reason);
        const auto requestedProfile = parameters.value("profileId").toString();
        const auto selector = [&interaction, requestedProfile](const QJsonArray& profiles) -> std::optional<int> {
            if (!requestedProfile.isEmpty()) {
                for (int i = 0; i < profiles.size(); ++i) {
                    if (profiles.at(i).toObject().value("id").toString() == requestedProfile)
                        return i;
                }
                return std::nullopt;
            }
            return interaction.select(QObject::tr("Select a game profile"), profiles);
        };

        MinecraftAccountPtr replacement = expired;
        if (expired->accountType() != AccountType::MSA) {
            auto username = parameters.value("username").toString();
            if (username.isEmpty()) {
                const auto input = interaction.input(tr("Username"), false);
                if (!input)
                    return nullptr;
                username = input->trimmed();
            }
            const auto password = interaction.input(tr("Password"), true);
            if (!password)
                return nullptr;
            const auto data = expired->accountData();
            if (expired->accountType() == AccountType::Yggdrasil) {
                const auto& config = data->yggdrasilConfig;
                replacement =
                    MinecraftAccount::createYggdrasil(username, *password, config.authServerUrl, config.sessionServerUrl, config.sourceName,
                                                      config.refreshEndpoint, config.validateEndpoint, config.authenticateEndpoint,
                                                      config.profileEndpoint, config.oauthTokenEndpoint, config.tokenType);
            } else {
                const auto& config = data->unifiedPassConfig;
                replacement = MinecraftAccount::createUnifiedPass(username, *password, config.serverId, config.baseUrl);
            }
        }

        auto auth = replacement->login(replacement->accountType() == AccountType::MSA, selector);
        connect(auth.get(), &AuthFlow::authorizeWithBrowserWithExtra, this,
                [&interaction](const QString& url, const QString& code, int expiresIn) { interaction.deviceCode(url, code, expiresIn); });
        connect(auth.get(), &AuthFlow::authorizeWithBrowser, this,
                [&interaction](const QUrl& url) { interaction.deviceCode(url.toString(), QString(), 0); });
        QString error;
        if (!waitForTask(auth.get(), interaction, &error)) {
            interaction.status(error);
            return nullptr;
        }

        auto accounts = APPLICATION->accounts();
        if (replacement != expired) {
            const bool wasDefault = accounts->defaultAccount() == expired;
            const int index = accounts->findAccountByProfileId(expired->profileId());
            if (index >= 0)
                accounts->removeAccount(accounts->index(index));
            accounts->addAccount(replacement);
            if (wasDefault)
                accounts->setDefaultAccount(replacement);
        }
        accounts->saveList();
        return replacement;
    });

    bool started = false;
    qint64 pid = -1;
    QEventLoop loop;
    connect(controller, &LaunchController::gameStarted, &loop, [&](qint64 processId) {
        started = true;
        pid = processId;
        if (!parameters.value("wait").toBool())
            loop.quit();
    });
    connect(controller, &Task::status, this, [&interaction](const QString& status) { interaction.status(status); });
    connect(controller, &LaunchController::logLine, this, [&interaction](const QString& line) { interaction.status(line); });
    connect(controller, &Task::finished, &loop, &QEventLoop::quit);
    m_currentTask = controller;
    emit taskStarted(controller);
    controller->start();
    if (!controller->isFinished() && (!started || parameters.value("wait").toBool()))
        loop.exec();

    const bool wait = parameters.value("wait").toBool();
    emit taskFinished(controller);
    if (!wait && started) {
        controller->deleteLater();
        m_currentTask.clear();
        return success(QJsonObject{ { "instance", instance->id() }, { "pid", pid }, { "detached", true } });
    }
    const bool ok = controller->wasSuccessful();
    const auto error = controller->failReason();
    controller->deleteLater();
    m_currentTask.clear();
    if (!ok)
        return failure(error.isEmpty() ? tr("Launch failed.") : error);
    return success(QJsonObject{ { "instance", instance->id() }, { "pid", pid }, { "detached", false } });
}
