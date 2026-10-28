// SPDX-License-Identifier: MPL-2.0

#include "LauncherApiSpecial.h"

#include "Application.h"
#include "InstanceList.h"
#include "api/LauncherApi.h"
#include "api/LauncherApiSupport.h"
#include "cli/OperationService.h"
#include "cli/UserInteraction.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "net/ApiDownload.h"
#include "net/NetJob.h"
#include "tasks/SequentialTask.h"

#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QStringList>
#include <QUrl>

namespace {

using namespace ApiSupport;

class NullInteraction final : public UserInteraction {
   public:
    void status(const QString&) override {}
    void deviceCode(const QString&, const QString&, int) override {}
    std::optional<QString> input(const QString&, bool) override { return std::nullopt; }
    std::optional<int> select(const QString&, const QJsonArray&) override { return std::nullopt; }
};

MinecraftInstance* findInstance(const QString& reference)
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
    return dynamic_cast<MinecraftInstance*>(instance);
}

bool waitForTask(LauncherApi& api, Task* task, UserInteraction& interaction, QString& error)
{
    if (!task) {
        error = QObject::tr("The operation did not create a task.");
        return false;
    }
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
        error = task->failReason().isEmpty() ? QObject::tr("The task was cancelled.") : task->failReason();
        return false;
    }
    return true;
}

struct Fetcher {
    LauncherApi& api;
    UserInteraction& interaction;
    QString error;

    QByteArray bytes(const QUrl& url)
    {
        if (!url.isValid() || url.host().isEmpty() || (url.scheme() != "http" && url.scheme() != "https")) {
            error = QObject::tr("Provider returned an invalid URL.");
            return {};
        }
        auto [request, response] = Net::ApiDownload::makeByteArray(url);
        auto job = makeShared<NetJob>(QObject::tr("Download provider metadata"), APPLICATION->network());
        job->addNetAction(request);
        if (!waitForTask(api, job.get(), interaction, error))
            return {};
        return *response;
    }

    QJsonDocument json(const QUrl& url)
    {
        const auto data = bytes(url);
        if (!error.isEmpty())
            return {};
        QJsonParseError parse;
        auto result = QJsonDocument::fromJson(data, &parse);
        if (parse.error != QJsonParseError::NoError)
            error = QObject::tr("Invalid provider response: %1").arg(parse.errorString());
        return result;
    }
};

QString safeLoader(const QString& value)
{
    const auto loader = value.trimmed().toLower();
    if (loader == "legacy-fabric" || loader == "legacy_fabric" || loader == "legacyfabric")
        return "legacyfabric";
    if (loader == "labymod-snapshot" || loader == "labymod_snapshot")
        return "labymod-snapshot";
    if (loader == "labymod-production" || loader == "labymod")
        return "labymod-production";
    return {};
}

QJsonObject versionItem(const QString& version, const QString& type, bool recommended = false)
{
    return { { "version", version }, { "type", type }, { "recommended", recommended } };
}

QJsonObject specialVersions(LauncherApi& api, const QJsonObject& parameters, UserInteraction& interaction)
{
    const auto loader = safeLoader(parameters.value("loader").toString());
    if (loader.isEmpty())
        return OperationService::failure(QObject::tr("loader must be legacyfabric, labymod-production, or labymod-snapshot."), 2);

    Fetcher fetch{ api, interaction };
    QJsonArray versions;
    if (loader == "legacyfabric") {
        const auto minecraft = parameters.value("minecraftVersion").toString().trimmed();
        const auto encodedMinecraft = QString::fromUtf8(QUrl::toPercentEncoding(minecraft));
        const auto url = minecraft.isEmpty() ? QUrl("https://meta.legacyfabric.net/v2/versions/game")
                                             : QUrl("https://meta.legacyfabric.net/v2/versions/loader/" + encodedMinecraft);
        const auto document = fetch.json(url);
        if (!fetch.error.isEmpty())
            return OperationService::failure(fetch.error);
        for (const auto& value : document.array()) {
            const auto entry = value.toObject();
            if (minecraft.isEmpty()) {
                const auto version = entry.value("version").toString();
                if (!version.isEmpty()) {
                    auto item = versionItem(version, entry.value("stable").toBool() ? "stable" : "preview", entry.value("stable").toBool());
                    item.insert("minecraftVersion", version);
                    versions.append(item);
                }
                continue;
            }
            const auto loaderEntry = entry.value("loader").toObject();
            const auto version = loaderEntry.value("version").toString();
            if (version.isEmpty())
                continue;
            auto item = versionItem(version, loaderEntry.value("stable").toBool() ? "stable" : "preview", loaderEntry.value("stable").toBool());
            item.insert("minecraftVersion", minecraft);
            item.insert("loaderVersion", version);
            versions.append(item);
        }
    } else {
        const QString channel = loader.endsWith("snapshot") ? QStringLiteral("snapshot") : QStringLiteral("production");
        const auto document = fetch.json(QUrl("https://releases.r2.labymod.net/api/v1/manifest/" + QString(channel) + "/latest.json"));
        if (!fetch.error.isEmpty())
            return OperationService::failure(fetch.error);
        const auto minecraftFilter = parameters.value("minecraftVersion").toString().trimmed();
        const auto object = document.object();
        const auto commitRef = object.value("commitReference").toString();
        const auto labyVersion = object.value("labyModVersion").toString();
        for (const auto& value : object.value("minecraftVersions").toArray()) {
            const auto entry = value.toObject();
            const auto minecraft = entry.value("version").toString();
            if (minecraft.isEmpty() || (!minecraftFilter.isEmpty() && minecraft != minecraftFilter))
                continue;
            auto item = versionItem(labyVersion, channel == "production" ? "stable" : "snapshot", channel == "production");
            item.insert("minecraftVersion", minecraft);
            item.insert("commitRef", commitRef);
            item.insert("channel", channel);
            item.insert("runtime", entry.value("runtime"));
            versions.append(item);
        }
    }
    return OperationService::success(QJsonObject{ { "loader", loader }, { "versions", versions } });
}

QJsonObject adaptLabyManifest(QJsonObject manifest,
                             const QJsonObject& libraries,
                             const QJsonObject& core,
                             const QString& minecraft,
                             const QString& channel);

QJsonObject buildSpecialLoaderProfile(LauncherApi& api,
                                      const QString& loader,
                                      const QString& minecraft,
                                      const QString& loaderVersion,
                                      const QString& commitRef,
                                      UserInteraction& interaction,
                                      QString& error)
{
    const auto normalizedLoader = safeLoader(loader);
    if (normalizedLoader.isEmpty()) {
        error = QObject::tr("Unknown special loader.");
        return {};
    }

    Fetcher fetch{ api, interaction };
    QJsonObject patch;
    if (normalizedLoader == "legacyfabric") {
        if (loaderVersion.trimmed().isEmpty()) {
            error = QObject::tr("Legacy Fabric loader version is required.");
            return {};
        }
        const auto encodedMinecraft = QString::fromUtf8(QUrl::toPercentEncoding(minecraft));
        const auto encodedLoader = QString::fromUtf8(QUrl::toPercentEncoding(loaderVersion.trimmed()));
        const auto url = QUrl("https://meta.legacyfabric.net/v2/versions/loader/" + encodedMinecraft + "/" + encodedLoader + "/profile/json");
        patch = fetch.json(url).object();
        patch.insert("uid", "net.legacyfabric");
        patch.insert("name", "Legacy Fabric");
        patch.insert("version", loaderVersion.trimmed());
    } else {
        const QString channel = normalizedLoader.endsWith("snapshot") ? QStringLiteral("snapshot") : QStringLiteral("production");
        if (commitRef.trimmed().isEmpty()) {
            error = QObject::tr("LabyMod commit reference is required.");
            return {};
        }
        const auto libraries = fetch.json(QUrl(QString("https://releases.r2.labymod.net/api/v1/libraries/%1.json").arg(channel)));
        const auto core = fetch.error.isEmpty() ? fetch.json(QUrl(QString("https://releases.r2.labymod.net/api/v1/manifest/%1/latest.json").arg(channel))) : QJsonDocument();
        const auto manifest = fetch.error.isEmpty() ? fetch.json(QUrl(QString("https://releases.r2.labymod.net/api/v1/download/manifest/labymod4/%1/%2/%3.json")
                                                                           .arg(channel, minecraft, commitRef.trimmed()))) : QJsonDocument();
        if (fetch.error.isEmpty())
            patch = adaptLabyManifest(manifest.object(), libraries.object(), core.object(), minecraft, channel);
        patch.insert("version", commitRef.trimmed());
    }
    if (fetch.error.isEmpty() && patch.isEmpty())
        error = QObject::tr("The special loader returned an empty profile.");
    else if (!fetch.error.isEmpty())
        error = fetch.error;
    if (error.isEmpty()) {
        const auto effectiveVersion = normalizedLoader == QLatin1String("legacyfabric")
                                          ? loaderVersion.trimmed()
                                          : patch.value("labymod_data").toObject().value("version").toString();
        auto descriptor = minecraft + QLatin1Char('|') + effectiveVersion;
        if (normalizedLoader != QLatin1String("legacyfabric"))
            descriptor += QLatin1Char('|') + commitRef.trimmed() + QLatin1Char('|') + normalizedLoader;
        patch.insert("version", descriptor);
        patch.insert("requires", QJsonArray{ QJsonObject{ { "uid", "net.minecraft" }, { "equals", minecraft } } });
    }
    return patch;
}

QString argumentString(const QJsonValue& value)
{
    if (value.isString())
        return value.toString();
    if (!value.isObject())
        return {};
    const auto object = value.toObject();
    // Prism's legacy minecraftArguments/+jvmArgs fields cannot represent
    // Mojang's feature/OS rules. Keeping a conditional argument here would
    // unconditionally enable things such as --demo or -XstartOnFirstThread.
    // The unconditional string arguments are still preserved below.
    if (!object.value("rules").toArray().isEmpty())
        return {};
    const auto nested = object.value("value");
    if (nested.isString())
        return nested.toString();
    if (nested.isArray()) {
        QStringList result;
        for (const auto& item : nested.toArray())
            if (item.isString()) result.append(item.toString());
        return result.join(' ');
    }
    return {};
}

QStringList flattenArguments(const QJsonArray& values)
{
    QStringList result;
    for (const auto& value : values) {
        const auto text = argumentString(value);
        if (!text.isEmpty())
            result.append(text);
    }
    return result;
}

QString urlFileName(const QString& url)
{
    return QFileInfo(QUrl(url).path()).fileName();
}

QString labyArtifactPath(const QString& name, const QString& url, const QString& filename)
{
    const auto path = QUrl(url).path();
    const auto marker = QStringLiteral("/libraries/");
    const auto markerIndex = path.indexOf(marker);
    if (markerIndex >= 0)
        return path.mid(markerIndex + marker.size());
    if (!filename.isEmpty()) {
        const auto parts = name.split(':');
        if (parts.size() >= 3) {
            auto group = parts.at(0);
            group.replace('.', '/');
            return group + QStringLiteral("/") + parts.at(1) + QStringLiteral("/") + parts.at(2) + QStringLiteral("/") + filename;
        }
    }
    return {};
}

QJsonObject labyLibraryObject(const QJsonObject& value)
{
    const auto name = value.value("name").toString();
    const auto url = value.value("url").toString();
    if (name.isEmpty() || url.isEmpty())
        return {};
    QJsonObject result{ { "name", name }, { "MMC-absoluteUrl", url } };
    const auto fileName = urlFileName(url);
    if (!fileName.isEmpty())
        result.insert("MMC-filename", fileName);
    QJsonObject artifact{ { "path", labyArtifactPath(name, url, fileName) },
                          { "sha1", value.value("sha1").toString() },
                          { "size", value.value("size").toInteger() },
                          { "url", url } };
    result.insert("downloads", QJsonObject{ { "artifact", artifact } });
    const auto lower = fileName.toLower();
    QJsonArray rules;
    auto addOsRule = [&rules](const QString& os) {
        rules.append(QJsonObject{ { "action", "allow" }, { "os", QJsonObject{ { "name", os } } } });
    };
    if (lower.contains("natives-windows")) addOsRule("windows");
    else if (lower.contains("natives-linux")) addOsRule("linux");
    else if (lower.contains("natives-macos") || lower.contains("natives-osx")) addOsRule("osx");
    if (!rules.isEmpty()) result.insert("rules", rules);
    return result;
}

QJsonObject adaptLabyManifest(QJsonObject manifest, const QJsonObject& libraries, const QJsonObject& core,
                              const QString& minecraft, const QString& channel)
{
    manifest.insert("uid", "net.labymod");
    manifest.insert("name", "LabyMod");
    manifest.insert("version", core.value("commitReference").toString());
    manifest.remove("inheritsFrom");

    const auto arguments = manifest.value("arguments").toObject();
    const auto game = flattenArguments(arguments.value("game").toArray());
    if (!game.isEmpty())
        manifest.insert("minecraftArguments", game.join(' '));
    QStringList jvm;
    for (const auto& value : flattenArguments(arguments.value("jvm").toArray())) {
        if (value == "-cp" || value == "-classpath" || value.contains("${classpath}") || value.contains("${natives_directory}") ||
            value.contains("${launcher_name}") || value.contains("${launcher_version}"))
            continue;
        jvm.append(value);
    }
    if (!jvm.isEmpty()) {
        QJsonArray extra;
        for (const auto& value : jvm) extra.append(value);
        manifest.insert("+jvmArgs", extra);
    }
    manifest.remove("arguments");

    QSet<QString> isolatedLibraries;
    for (const auto& value : libraries.value("isolated_libraries").toArray()) {
        const auto entry = value.toObject();
        const auto versions = entry.value("versions").toArray();
        bool applies = false;
        for (const auto& version : versions) {
            if (version.toString() == minecraft || version.toString() == "all") {
                applies = true;
                break;
            }
        }
        if (applies && !entry.value("name").toString().isEmpty())
            isolatedLibraries.insert(entry.value("name").toString());
    }

    QJsonArray merged;
    for (const auto& value : manifest.value("libraries").toArray())
    {
        const auto entry = value.toObject();
        const auto name = entry.value("name").toString();
        const auto parts = name.split(':');
        const auto artifact = parts.size() > 1 ? parts.at(1) : QString();
        if (!artifact.isEmpty() && isolatedLibraries.contains(artifact))
            continue;
        merged.append(value);
    }
    for (const auto& value : libraries.value("libraries").toArray()) {
        const auto entry = labyLibraryObject(value.toObject());
        if (!entry.isEmpty())
            merged.append(entry);
    }
    const auto coreRef = core.value("commitReference").toString();
    const auto coreUrl = QString("https://releases.r2.labymod.net/api/v1/download/labymod4/%1/%2.jar").arg(channel, coreRef);
    const auto coreSha1 = core.value("sha1").toString();
    const auto coreSize = core.value("size").toInteger();
    merged.append(QJsonObject{ { "name", "net.labymod:LabyMod:4" },
                               { "MMC-absoluteUrl", coreUrl },
                               { "MMC-filename", "LabyMod-4.jar" },
                               { "downloads", QJsonObject{ { "artifact", QJsonObject{ { "path", "net/labymod/LabyMod/4/LabyMod-4.jar" },
                                                                                         { "sha1", coreSha1 },
                                                                                         { "size", coreSize },
                                                                                         { "url", coreUrl } } } } } });
    manifest.insert("libraries", merged);
    manifest.insert("labymod_data", QJsonObject{ { "channelType", channel }, { "commitReference", coreRef },
                                                     { "version", core.value("labyModVersion") }, { "versionType", "release" } });
    return manifest;
}

QJsonObject installSpecialLoader(LauncherApi& api, const QJsonObject& parameters, UserInteraction& interaction)
{
    const auto loader = safeLoader(parameters.value("loader").toString());
    auto instance = findInstance(parameters.value("instance").toString());
    if (loader.isEmpty()) return OperationService::failure(QObject::tr("Unknown special loader."), 2);
    if (!instance) return OperationService::failure(QObject::tr("Minecraft instance not found."), 2);
    if (instance->isRunning()) return OperationService::failure(QObject::tr("The instance must be stopped before installing a loader."), 2);

    auto profile = instance->getPackProfile();
    if (!profile->isLoaded()) {
        const auto loaded = profile->reload(Net::Mode::Offline);
        if (!loaded) return OperationService::failure(loaded.error);
        if (const auto task = profile->getCurrentTask()) {
            QString error;
            if (!waitForTask(api, task.get(), interaction, error))
                return OperationService::failure(error);
        }
    }
    QString minecraft = parameters.value("minecraftVersion").toString().trimmed();
    if (minecraft.isEmpty()) {
        if (auto component = profile->getComponent("net.minecraft")) minecraft = component->getVersion();
    }
    if (minecraft.isEmpty()) return OperationService::failure(QObject::tr("Minecraft version is required."), 2);

    const auto loaderVersion = parameters.value("loaderVersion").toString().trimmed();
    const auto commit = parameters.value("commitRef").toString().trimmed();
    QString profileError;
    auto patch = buildSpecialLoaderProfile(api, loader, minecraft, loaderVersion, commit, interaction, profileError);
    if (!profileError.isEmpty()) return OperationService::failure(profileError);
    const QString uid = loader == "legacyfabric" ? QStringLiteral("net.legacyfabric") : QStringLiteral("net.labymod");

    const auto patchDir = QDir(instance->instanceRoot()).filePath("patches");
    if (!QDir().mkpath(patchDir)) return OperationService::failure(QObject::tr("Could not create the instance patch directory."));
    const auto targetPath = QDir(patchDir).filePath(uid + ".json");
    const auto backupPath = QDir(instance->instanceRoot()).filePath(".neo-backup-" + uid + ".json");
    const bool hadPreviousPatch = QFileInfo::exists(targetPath);
    if (hadPreviousPatch) {
        QFile::remove(backupPath);
        if (!QFile::copy(targetPath, backupPath))
            return OperationService::failure(QObject::tr("Could not back up the existing special loader profile."));
    } else {
        QFile::remove(backupPath);
    }
    // Write through a temporary profile file. installComponents() owns the
    // copy into patches/<uid>.json; writing directly to that destination would
    // make the source and destination identical and QFile::copy would fail.
    const auto sourcePath = QDir(instance->instanceRoot()).filePath(".neo-" + uid + ".json");
    QSaveFile file(sourcePath);
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(patch).toJson()) <= 0 || !file.commit()) {
        QFile::remove(backupPath);
        return OperationService::failure(QObject::tr("Could not save the special loader profile."));
    }

    const auto previousComponent = profile->getComponent(uid);
    const bool hadPreviousComponent = previousComponent != nullptr;
    const bool previousEnabled = hadPreviousComponent && previousComponent->isEnabled();
    if (previousComponent && !profile->remove(uid)) {
        QFile::remove(sourcePath);
        QFile::remove(backupPath);
        return OperationService::failure(QObject::tr("The existing special loader profile could not be replaced."));
    }
    // A stale patch can remain after a previous interrupted install even when
    // it is absent from the in-memory profile. Remove it before copying the
    // new profile, while keeping the backup for rollback.
    auto restorePrevious = [&]() {
        if (profile->getComponent(uid)) profile->remove(uid);
        if (hadPreviousPatch && QFileInfo::exists(backupPath)) {
            profile->installComponents({ backupPath });
            if (auto restored = profile->getComponent(uid); restored && hadPreviousComponent && !previousEnabled)
                restored->setEnabled(false);
        }
        profile->saveNow();
        QFile::remove(backupPath);
    };
    if (QFileInfo::exists(targetPath) && !QFile::remove(targetPath)) {
        QFile::remove(sourcePath);
        restorePrevious();
        return OperationService::failure(QObject::tr("The existing special loader profile could not be replaced."));
    }
    if (!profile->installComponents({ sourcePath })) {
        QFile::remove(sourcePath);
        restorePrevious();
        return OperationService::failure(QObject::tr("Could not install the special loader profile."));
    }
    QFile::remove(sourcePath);
    if (auto installed = profile->getComponent(uid); installed && hadPreviousComponent && !previousEnabled)
        installed->setEnabled(false);
    profile->saveNow();
    profile->resolve(Net::Mode::Online);
    QString error;
    // resolve() can finish synchronously for an already cached profile. Keep
    // the task alive before waiting instead of reading getCurrentTask() again,
    // since a completed resolve may have already cleared that pointer.
    const auto resolveTask = profile->getCurrentTask();
    if (resolveTask && !waitForTask(api, resolveTask.get(), interaction, error)) {
        restorePrevious();
        return OperationService::failure(error);
    }

    const auto updates = instance->createUpdateTask();
    if (!updates.isEmpty()) {
        auto update = makeShared<SequentialTask>(QObject::tr("Download special loader files"));
        for (const auto& task : updates) update->addTask(task);
        if (!waitForTask(api, update.get(), interaction, error)) {
            restorePrevious();
            return OperationService::failure(error);
        }
    }
    QFile::remove(backupPath);
    instance->saveNow();
    return OperationService::success(QJsonObject{ { "instance", instance->id() }, { "loader", loader }, { "minecraftVersion", minecraft },
                                                  { "component", uid }, { "version", patch.value("version") } });
}

bool installSpecialLoaderIntoInstance(LauncherApi& api,
                                      MinecraftInstance* instance,
                                      const QString& loader,
                                      const QString& minecraft,
                                      const QString& loaderVersion,
                                      const QString& commit,
                                      UserInteraction& interaction,
                                      QString& error)
{
    if (!instance) {
        error = QObject::tr("Minecraft instance is not available.");
        return false;
    }
    auto profile = instance->getPackProfile();
    if (!profile->isLoaded()) {
        const auto loaded = profile->reload(Net::Mode::Offline);
        if (!loaded) {
            error = loaded.error;
            return false;
        }
        if (const auto task = profile->getCurrentTask(); task && !waitForTask(api, task.get(), interaction, error))
            return false;
    }

    QString profileError;
    const auto patch = buildSpecialLoaderProfile(api, loader, minecraft, loaderVersion, commit, interaction, profileError);
    if (!profileError.isEmpty()) {
        error = profileError;
        return false;
    }
    const QString uid = safeLoader(loader) == "legacyfabric" ? QStringLiteral("net.legacyfabric") : QStringLiteral("net.labymod");
    const auto patchDir = QDir(instance->instanceRoot()).filePath("patches");
    if (!QDir().mkpath(patchDir)) {
        error = QObject::tr("Could not create the instance patch directory.");
        return false;
    }
    const auto targetPath = QDir(patchDir).filePath(uid + ".json");
    const auto backupPath = QDir(instance->instanceRoot()).filePath(".neo-backup-" + uid + ".json");
    const bool hadPreviousPatch = QFileInfo::exists(targetPath);
    if (hadPreviousPatch) {
        QFile::remove(backupPath);
        if (!QFile::copy(targetPath, backupPath)) {
            error = QObject::tr("Could not back up the existing special loader profile.");
            return false;
        }
    } else {
        QFile::remove(backupPath);
    }

    const auto sourcePath = QDir(instance->instanceRoot()).filePath(".neo-" + uid + ".json");
    QSaveFile file(sourcePath);
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(patch).toJson()) <= 0 || !file.commit()) {
        QFile::remove(backupPath);
        error = QObject::tr("Could not save the special loader profile.");
        return false;
    }

    const auto previousComponent = profile->getComponent(uid);
    const bool hadPreviousComponent = previousComponent != nullptr;
    const bool previousEnabled = hadPreviousComponent && previousComponent->isEnabled();
    if (previousComponent && !profile->remove(uid)) {
        QFile::remove(sourcePath);
        QFile::remove(backupPath);
        error = QObject::tr("The existing special loader profile could not be replaced.");
        return false;
    }
    auto restorePrevious = [&]() {
        if (profile->getComponent(uid)) profile->remove(uid);
        if (hadPreviousPatch && QFileInfo::exists(backupPath)) {
            profile->installComponents({ backupPath });
            if (auto restored = profile->getComponent(uid); restored && hadPreviousComponent && !previousEnabled)
                restored->setEnabled(false);
        }
        profile->saveNow();
        QFile::remove(backupPath);
    };
    if (QFileInfo::exists(targetPath) && !QFile::remove(targetPath)) {
        QFile::remove(sourcePath);
        restorePrevious();
        error = QObject::tr("The existing special loader profile could not be replaced.");
        return false;
    }
    if (!profile->installComponents({ sourcePath })) {
        QFile::remove(sourcePath);
        restorePrevious();
        error = QObject::tr("Could not install the special loader profile.");
        return false;
    }
    QFile::remove(sourcePath);
    if (auto installed = profile->getComponent(uid); installed && hadPreviousComponent && !previousEnabled)
        installed->setEnabled(false);
    profile->saveNow();
    profile->resolve(Net::Mode::Online);
    if (const auto resolveTask = profile->getCurrentTask(); resolveTask && !waitForTask(api, resolveTask.get(), interaction, error)) {
        restorePrevious();
        return false;
    }
    const auto updates = instance->createUpdateTask();
    if (!updates.isEmpty()) {
        auto update = makeShared<SequentialTask>(QObject::tr("Download special loader files"));
        for (const auto& task : updates) update->addTask(task);
        if (!waitForTask(api, update.get(), interaction, error)) {
            restorePrevious();
            return false;
        }
    }
    QFile::remove(backupPath);
    instance->saveNow();
    return true;
}

}  // namespace

namespace SpecialLoader {

QJsonArray versions(const QString& loader, const QString& minecraftVersion, QString* error)
{
    LauncherApi api;
    NullInteraction interaction;
    const auto result = api.execute("special.loader.versions",
                                    QJsonObject{ { "loader", loader }, { "minecraftVersion", minecraftVersion } },
                                    interaction);
    if (!result.value("ok").toBool()) {
        if (error) *error = result.value("error").toString();
        return {};
    }
    return result.value("data").toObject().value("versions").toArray();
}

QJsonObject profile(const QString& loader,
                    const QString& minecraftVersion,
                    const QString& loaderVersion,
                    const QString& commitRef,
                    QString* error)
{
    LauncherApi api;
    NullInteraction interaction;
    QString localError;
    auto result = buildSpecialLoaderProfile(api, loader, minecraftVersion, loaderVersion, commitRef, interaction, localError);
    if (error) *error = localError;
    return result;
}

bool install(MinecraftInstance* instance,
             const QString& loader,
             const QString& minecraftVersion,
             const QString& loaderVersion,
             const QString& commitRef,
             QString* error)
{
    if (!instance) {
        if (error) *error = QObject::tr("Minecraft instance is not available.");
        return false;
    }
    LauncherApi api;
    NullInteraction interaction;
    if (findInstance(instance->id()) == instance) {
        const QJsonObject parameters{ { "instance", instance->id() },
                                      { "loader", loader },
                                      { "minecraftVersion", minecraftVersion },
                                      { "loaderVersion", loaderVersion },
                                      { "commitRef", commitRef } };
        const auto result = api.execute("instance.special-loader.install", parameters, interaction);
        if (!result.value("ok").toBool()) {
            if (error) *error = result.value("error").toString();
            return false;
        }
        return true;
    }
    QString localError;
    const bool ok = installSpecialLoaderIntoInstance(api, instance, loader, minecraftVersion, loaderVersion, commitRef, interaction, localError);
    if (error) *error = localError;
    return ok;
}

}  // namespace SpecialLoader

void registerLauncherApiSpecialOperations(LauncherApi& api)
{
    api.registerOperation({ "special.loader.versions", "List Legacy Fabric or LabyMod versions from their official metadata service.",
                            schema({ { "loader", string("legacyfabric, labymod-production, or labymod-snapshot.") },
                                     { "minecraftVersion", string("Optional Minecraft version filter.") } }, { "loader" }),
                            "components" },
                           [&api](const QJsonObject& p, UserInteraction& i) { return specialVersions(api, p, i); });
    api.registerOperation({ "instance.special-loader.install", "Install an official Legacy Fabric or LabyMod profile into an existing Minecraft instance.",
                            schema({ { "instance", string("Minecraft instance ID, managed name, or display name.") },
                                     { "loader", string("legacyfabric, labymod-production, or labymod-snapshot.") },
                                     { "minecraftVersion", string("Minecraft version.") }, { "loaderVersion", string("Legacy Fabric loader version.") },
                                     { "commitRef", string("LabyMod manifest commit reference.") } },
                                   { "instance", "loader", "minecraftVersion" }),
                            "instances", true },
                           [&api](const QJsonObject& p, UserInteraction& i) { return installSpecialLoader(api, p, i); });
}
