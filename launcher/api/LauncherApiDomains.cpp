// SPDX-License-Identifier: GPL-3.0-only

#include "LauncherApiDomains.h"

#include "Application.h"
#include "DesktopServices.h"
#include "api/LauncherApi.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/WorldList.h"
#include "minecraft/World.h"
#include "minecraft/auth/AccountList.h"
#include "minecraft/auth/MinecraftAccount.h"
#include "minecraft/skins/CapeChange.h"
#include "minecraft/skins/SkinDelete.h"
#include "minecraft/skins/SkinList.h"
#include "minecraft/skins/SkinModel.h"
#include "minecraft/skins/SkinUpload.h"
#include "InstanceList.h"
#include "cli/OperationService.h"
#include "net/NetJob.h"
#include "tasks/Task.h"
#include "MMCZip.h"
#include "archive/ExportToZipTask.h"
#include "FileSystem.h"
#include "net/PasteUpload.h"
#include "screenshots/ImgurUpload.h"
#include "GZip.h"
#include "screenshots/ImgurAlbumCreation.h"
#include "minecraft/auth/YggdrasilPresets.h"
#include "net/HttpMetaCache.h"
#include "tools/MCEditTool.h"
#include "news/NewsChecker.h"
#include "screenshots/Screenshot.h"
#include <BuildConfig.h>

#include <QDir>
#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkProxy>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTimer>
#include <QSettings>
#include <QProcess>
#include <QTemporaryDir>
#include <QRegularExpression>
#include <QClipboard>
#include <QGuiApplication>
#include <QMimeData>
#include <QImageReader>
#include <QBuffer>
#include <QSaveFile>
#include "updater/ExternalUpdater.h"

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

QJsonObject worldJson(const World& world)
{
    return { { "folder", world.folderName() },
             { "name", world.name() },
             { "path", world.container().absoluteFilePath() },
             { "icon", world.iconFile() },
             { "size", static_cast<qint64>(world.bytes()) },
             { "lastPlayed", world.lastPlayed().toString(Qt::ISODate) },
             { "seed", static_cast<qint64>(world.seed()) },
             { "seedText", QString::number(world.seed()) },
             { "gameType", world.gameType().toLogString() },
             { "valid", world.isValid() },
             { "directory", world.isOnFS() } };
}

QString skinDirectory()
{
    return APPLICATION->settings()->get("SkinsDir").toString();
}

SkinModel* findSkin(SkinList& skins, const QString& reference)
{
    const auto normalized = QDir::fromNativeSeparators(reference.trimmed());
    for (int row = 0; row < skins.rowCount(); ++row) {
        const auto key = skins.index(row, 0).data(Qt::UserRole).toString();
        auto skin = skins.skin(key);
        if (!skin)
            continue;
        if (key.compare(reference, Qt::CaseInsensitive) == 0 ||
            QDir::fromNativeSeparators(skin->getPath()).compare(normalized, Qt::CaseInsensitive) == 0 ||
            QFileInfo(skin->getPath()).fileName().compare(reference, Qt::CaseInsensitive) == 0)
            return skin;
    }
    return nullptr;
}

QJsonObject skinJson(const SkinModel& skin, MinecraftAccount& account)
{
    const auto& profile = account.accountData()->minecraftProfile;
    return { { "name", skin.name() },
             { "path", skin.getPath() },
             { "model", skin.getModelString() },
             { "cape", skin.getCapeId() },
             { "url", skin.getURL() },
             { "selected", !profile.skin.url.isEmpty() && profile.skin.url == skin.getURL() } };
}

bool waitForAccountTask(Task* task, UserInteraction& interaction, QString* error, LauncherApi& api)
{
    if (!task) {
        if (error)
            *error = QObject::tr("The account operation did not create a task.");
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
    if (!task->wasSuccessful() && error)
        *error = task->failReason().isEmpty() ? QObject::tr("The account operation failed.") : task->failReason();
    return task->wasSuccessful();
}

QJsonObject listAccountSkins(const QJsonObject& parameters)
{
    const auto account = findAccount(parameters.value("account").toString());
    if (!account)
        return OperationService::failure(QObject::tr("Account not found: %1").arg(parameters.value("account").toString()), 2);
    SkinList skins(nullptr, skinDirectory(), account);
    QJsonArray result;
    for (int row = 0; row < skins.rowCount(); ++row) {
        const auto key = skins.index(row, 0).data(Qt::UserRole).toString();
        if (const auto* skin = skins.skin(key))
            result.append(skinJson(*skin, *account));
    }
    return OperationService::success(result);
}

QJsonObject installAccountSkin(const QJsonObject& parameters)
{
    const auto account = findAccount(parameters.value("account").toString());
    if (!account)
        return OperationService::failure(QObject::tr("Account not found: %1").arg(parameters.value("account").toString()), 2);
    const auto path = parameters.value("path").toString().trimmed();
    if (path.isEmpty())
        return OperationService::failure(QObject::tr("A skin PNG path is required."), 2);
    SkinList skins(nullptr, skinDirectory(), account);
    const auto error = skins.installSkin(path, parameters.value("name").toString().trimmed());
    if (!error.isEmpty())
        return OperationService::failure(error, 2);
    return OperationService::success(QJsonObject{ { "path", path }, { "installed", true } });
}

QJsonObject deleteAccountSkin(const QJsonObject& parameters)
{
    if (!parameters.value("confirm").toBool())
        return OperationService::failure(QObject::tr("Skin deletion requires confirm=true."), 2);
    const auto account = findAccount(parameters.value("account").toString());
    if (!account)
        return OperationService::failure(QObject::tr("Account not found: %1").arg(parameters.value("account").toString()), 2);
    SkinList skins(nullptr, skinDirectory(), account);
    auto* skin = findSkin(skins, parameters.value("skin").toString());
    if (!skin)
        return OperationService::failure(QObject::tr("Skin not found: %1").arg(parameters.value("skin").toString()), 2);
    const auto name = skin->name();
    if (!skins.deleteSkin(name, parameters.value("trash").toBool(true)))
        return OperationService::failure(QObject::tr("The skin could not be deleted."));
    return OperationService::success(QJsonObject{ { "skin", name }, { "deleted", true } });
}

QJsonObject uploadAccountSkin(LauncherApi& api, const QJsonObject& parameters, UserInteraction& interaction)
{
    const auto account = findAccount(parameters.value("account").toString());
    if (!account)
        return OperationService::failure(QObject::tr("Account not found: %1").arg(parameters.value("account").toString()), 2);
    if (account->accountType() != AccountType::MSA)
        return OperationService::failure(QObject::tr("Skin upload requires a Microsoft account."), 2);
    if (account->accessToken().isEmpty())
        return OperationService::failure(QObject::tr("The account has no access token for skin changes."), 3);
    SkinList skins(nullptr, skinDirectory(), account);
    auto* skin = findSkin(skins, parameters.value("skin").toString());
    if (!skin)
        return OperationService::failure(QObject::tr("Skin not found: %1").arg(parameters.value("skin").toString()), 2);
    if (!QFileInfo::exists(skin->getPath()))
        return OperationService::failure(QObject::tr("Skin file not found: %1").arg(skin->getPath()), 2);

    const auto variant = parameters.value("variant").toString(skin->getModelString()).toLower();
    if (variant != "classic" && variant != "slim")
        return OperationService::failure(QObject::tr("Skin variant must be classic or slim."), 2);
    const auto cape = parameters.contains("cape") ? parameters.value("cape").toString() : skin->getCapeId();
    const auto skinPath = skin->getPath();
    const auto skinName = skin->name();

    auto job = makeShared<NetJob>(QObject::tr("Change skin"), APPLICATION->network(), 1);
    job->addNetAction(SkinUpload::make(account->accessToken(), skin->getPath(), variant));
    if (cape != account->accountData()->minecraftProfile.currentCape)
        job->addNetAction(CapeChange::make(account->accessToken(), cape));
    job->addTask(account->refresh().staticCast<Task>());
    QString error;
    if (!waitForAccountTask(job.get(), interaction, &error, api))
        return OperationService::failure(error, 3);
    // SkinList may rebuild its models from filesystem events during the wait.
    const auto& profile = account->accountData()->minecraftProfile;
    if (auto updated = findSkin(skins, skinPath)) {
        updated->setURL(profile.skin.url);
        updated->setCapeId(profile.currentCape);
        skins.save();
    }
    return OperationService::success(QJsonObject{ { "skin", skinName }, { "variant", variant }, { "cape", profile.currentCape },
                                                  { "url", profile.skin.url }, { "uploaded", true } });
}

QJsonObject resetAccountSkin(LauncherApi& api, const QJsonObject& parameters, UserInteraction& interaction)
{
    if (!parameters.value("confirm").toBool())
        return OperationService::failure(QObject::tr("Skin reset requires confirm=true."), 2);
    const auto account = findAccount(parameters.value("account").toString());
    if (!account)
        return OperationService::failure(QObject::tr("Account not found: %1").arg(parameters.value("account").toString()), 2);
    if (account->accountType() != AccountType::MSA)
        return OperationService::failure(QObject::tr("Skin reset requires a Microsoft account."), 2);
    if (account->accessToken().isEmpty())
        return OperationService::failure(QObject::tr("The account has no access token for skin changes."), 3);
    auto job = makeShared<NetJob>(QObject::tr("Reset skin"), APPLICATION->network(), 1);
    job->addNetAction(SkinDelete::make(account->accessToken()));
    job->addTask(account->refresh().staticCast<Task>());
    QString error;
    if (!waitForAccountTask(job.get(), interaction, &error, api))
        return OperationService::failure(error, 3);
    return OperationService::success(QJsonObject{ { "account", account->profileId() }, { "reset", true } });
}

QJsonObject selectAccountCape(LauncherApi& api, const QJsonObject& parameters, UserInteraction& interaction)
{
    const auto account = findAccount(parameters.value("account").toString());
    if (!account)
        return OperationService::failure(QObject::tr("Account not found: %1").arg(parameters.value("account").toString()), 2);
    if (account->accountType() != AccountType::MSA)
        return OperationService::failure(QObject::tr("Cape selection requires a Microsoft account."), 2);
    if (account->accessToken().isEmpty())
        return OperationService::failure(QObject::tr("The account has no access token for cape changes."), 3);
    const auto cape = parameters.value("cape").toString();
    bool knownCape = cape.isEmpty();
    for (const auto& item : account->accountData()->minecraftProfile.capes) {
        if (item.id == cape) {
            knownCape = true;
            break;
        }
    }
    if (!knownCape)
        return OperationService::failure(QObject::tr("Cape not found: %1").arg(cape), 2);
    auto job = makeShared<NetJob>(QObject::tr("Change cape"), APPLICATION->network(), 1);
    job->addNetAction(CapeChange::make(account->accessToken(), cape));
    job->addTask(account->refresh().staticCast<Task>());
    QString error;
    if (!waitForAccountTask(job.get(), interaction, &error, api))
        return OperationService::failure(error, 3);
    return OperationService::success(QJsonObject{ { "cape", account->accountData()->minecraftProfile.currentCape }, { "changed", true } });
}

World* findWorld(WorldList* worlds, const QString& reference)
{
    if (!worlds)
        return nullptr;
    for (size_t i = 0; i < worlds->size(); ++i) {
        auto& world = (*worlds)[i];
        if (world.folderName().compare(reference, Qt::CaseInsensitive) == 0 ||
            world.name().compare(reference, Qt::CaseInsensitive) == 0 ||
            world.container().fileName().compare(reference, Qt::CaseInsensitive) == 0)
            return &world;
    }
    return nullptr;
}

QStringList logRoots(BaseInstance* instance)
{
    QStringList roots = instance->getLogFileSearchPaths();
    roots.removeDuplicates();
    return roots;
}

QFileInfo safeFileInRoots(const QString& reference, const QStringList& roots)
{
    const auto requested = QFileInfo(reference);
    for (const auto& root : roots) {
        const auto rootPath = QFileInfo(root).canonicalFilePath();
        if (rootPath.isEmpty())
            continue;
        auto candidate = requested.isAbsolute() ? requested : QFileInfo(QDir(root).filePath(reference));
        if (!candidate.exists() || !candidate.isFile())
            continue;
        const auto canonical = candidate.canonicalFilePath();
        if (canonical == rootPath || canonical.startsWith(rootPath + '/'))
            return candidate;
    }
    return {};
}

QJsonObject listLogs(const QJsonObject& parameters)
{
    auto instance = findInstance(parameters.value("instance").toString());
    if (!instance)
        return OperationService::failure(QObject::tr("Instance not found."), 2);
    QJsonArray result;
    for (const auto& root : logRoots(instance)) {
        QStringList filters{ "*.log", "*.log.gz" };
        if (QDir(root).absolutePath() != QDir(instance->gameRoot()).absolutePath()) filters.append("*.txt");
        const auto entries = QDir(root).entryInfoList(filters, QDir::Files | QDir::Readable | QDir::NoSymLinks, QDir::Name | QDir::IgnoreCase);
        for (const auto& entry : entries)
            result.append(QJsonObject{ { "name", entry.fileName() }, { "path", entry.absoluteFilePath() },
                                       { "size", static_cast<qint64>(entry.size()) },
                                       { "modified", entry.lastModified().toString(Qt::ISODate) } });
    }
    return OperationService::success(result);
}

QFileInfo logFile(const QJsonObject& parameters)
{
    const auto reference = parameters.value("file").toString();
    for (const auto& value : listLogs(parameters).value("data").toArray()) {
        const QFileInfo file(value.toObject().value("path").toString());
        if (reference == file.fileName() || QDir::cleanPath(QDir::fromNativeSeparators(reference)) == file.absoluteFilePath()) return file;
    }
    return {};
}

QJsonObject installWorldSafely(MinecraftInstance* instance, const QFileInfo& source, QString name, bool replace)
{
    if (instance->isRunning()) return OperationService::failure("The target instance must be stopped.", 2);
    World world(source);
    if (!world.isValid()) return OperationService::failure("The source is not a valid world.", 2);
    if (name.isEmpty()) name = world.name().trimmed();
    static const QRegularExpression unsafe(R"([<>:"/\\|?*\x00-\x1f])");
    static const QRegularExpression reserved(R"(^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\.|$))", QRegularExpression::CaseInsensitiveOption);
    if (name.isEmpty() || name == "." || name == ".." || name.endsWith('.') || name.endsWith(' ') ||
        unsafe.match(name).hasMatch() || reserved.match(name).hasMatch()) return OperationService::failure("Invalid world name.", 2);
    if (!QDir().mkpath(instance->worldDir())) return OperationService::failure("Could not create saves directory.");
    const auto root = QFileInfo(instance->worldDir()).canonicalFilePath();
    const auto destination = QDir(root).filePath(name);
    const QFileInfo target(destination);
    const auto sourcePath = source.canonicalFilePath();
    const auto targetPath = target.canonicalFilePath();
    if (target.isSymLink() || (!targetPath.isEmpty() && !targetPath.startsWith(root + '/')))
        return OperationService::failure("Destination is outside the saves directory or is a symbolic link.", 2);
    if (!targetPath.isEmpty() && (sourcePath == targetPath || sourcePath.startsWith(targetPath + '/')))
        return OperationService::failure("Source and destination overlap.", 2);
    if (source.isDir() && (root == sourcePath || root.startsWith(sourcePath + '/')))
        return OperationService::failure("Destination is inside the source world.", 2);
    if (target.exists() && (!replace || !target.isDir())) return OperationService::failure("Destination exists; replacement requires a world directory and replace=true.", 2);
    QTemporaryDir staging(QDir(root).filePath(".api-world-XXXXXX"));
    if (!staging.isValid()) return OperationService::failure("Could not create world staging directory.");
    const auto incoming = QDir(staging.path()).filePath("incoming");
    if (!QDir().mkpath(incoming) || !world.install(incoming, name)) return OperationService::failure("World staging failed; destination was preserved.");
    const auto entries = QDir(incoming).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (entries.size() != 1 || !World(entries.first()).isValid() || World(entries.first()).name() != name)
        return OperationService::failure("Staged world validation failed; destination was preserved.");
    const auto backup = QDir(staging.path()).filePath("previous");
    if (target.exists() && !QDir().rename(destination, backup)) return OperationService::failure("Could not back up existing world.");
    if (!QDir().rename(entries.first().absoluteFilePath(), destination)) {
        if (target.exists() && !QDir().rename(backup, destination)) {
            staging.setAutoRemove(false);
            return OperationService::failure("Restore failed; previous world is preserved at " + backup);
        }
        return OperationService::failure("World installation failed; previous world restored.");
    }
    instance->worldList()->update();
    return OperationService::success(QJsonObject{ { "instance", instance->id() }, { "name", name }, { "path", destination }, { "changed", true } });
}

QJsonObject proxyInfo()
{
    const auto s = APPLICATION->settings();
    return OperationService::success(QJsonObject{ { "type", s->get("ProxyType").toString() }, { "host", s->get("ProxyAddr").toString() },
                                                   { "port", s->get("ProxyPort").toInt() }, { "username", s->get("ProxyUser").toString() },
                                                   { "configured", s->get("ProxyType").toString() != "None" } });
}

QJsonObject proxySet(const QJsonObject& p)
{
    auto s = APPLICATION->settings();
    const auto type = p.value("type").toString(s->get("ProxyType").toString()).toUpper();
    if (type != "NONE" && type != "HTTP" && type != "SOCKS5" && type != "DEFAULT")
        return OperationService::failure(QObject::tr("Proxy type must be None, HTTP, SOCKS5, or Default."), 2);
    const auto normalized = type == "NONE" ? QString("None") : type == "DEFAULT" ? QString("Default") : type;
    const auto host = p.value("host").toString(s->get("ProxyAddr").toString()).trimmed();
    const auto port = p.value("port").toInt(s->get("ProxyPort").toInt());
    if (port < 1 || port > 65535) return OperationService::failure(QObject::tr("Proxy port is invalid."), 2);
    if ((type == "HTTP" || type == "SOCKS5") && host.isEmpty()) return OperationService::failure("Proxy host is required.", 2);
    const auto user = p.value("username").toString(s->get("ProxyUser").toString());
    const auto password = p.value("password").toString(s->get("ProxyPass").toString());
    s->set("ProxyType", normalized); s->set("ProxyAddr", host); s->set("ProxyPort", port);
    s->set("ProxyUser", user); s->set("ProxyPass", password);
    APPLICATION->updateProxySettings(normalized, host, port, user, password);
    return proxyInfo();
}

QJsonObject proxyTest(const QJsonObject& p)
{
    const auto url = QUrl(p.value("url").toString("https://api.minecraftservices.com/"));
    if (!url.isValid() || url.scheme() != "https" || url.host().isEmpty()) return OperationService::failure("Only HTTPS test URLs are allowed.", 2);
    QNetworkReply* reply = APPLICATION->network()->head(QNetworkRequest(url));
    QEventLoop loop; QTimer timer; timer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit); QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(qBound(1000, p.value("timeoutMs").toInt(10000), 60000)); loop.exec();
    const bool timedOut = !reply->isFinished();
    if (timedOut) reply->abort();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const auto result = OperationService::success(QJsonObject{ { "reachable", !timedOut && status > 0 }, { "timedOut", timedOut },
        { "statusCode", status }, { "errorCode", static_cast<int>(reply->error()) },
        { "error", reply->error() == QNetworkReply::NoError ? QString() : reply->errorString() }, { "url", url.toString() } });
    reply->deleteLater();
    return result;
}

QSettings updateSettings()
{
    return QSettings(QDir(APPLICATION->dataRoot()).filePath("lunalauncher_update.cfg"), QSettings::IniFormat);
}

QJsonObject updateStatus()
{
    auto settings = updateSettings();
    const auto marker = QDir(APPLICATION->dataRoot()).filePath(".lunalauncher_update.success");
    const auto failed = QDir(APPLICATION->dataRoot()).filePath(".prism_launcher_update.fail");
    const auto lock = QDir(APPLICATION->dataRoot()).filePath(".prism_launcher_update.lock");
    const auto log = QDir(APPLICATION->dataRoot()).filePath("logs/prism_launcher_update.log");
    QFile logFile(log);
    QByteArray logTail;
    if (logFile.open(QIODevice::ReadOnly)) {
        logFile.seek(qMax<qint64>(0, logFile.size() - 262144));
        logTail = logFile.read(262144);
    }
    return OperationService::success(QJsonObject{ { "automatic", settings.value("auto_check", true).toBool() },
                                                   { "intervalSeconds", settings.value("update_interval", 86400).toInt() },
                                                   { "beta", settings.value("allow_beta", false).toBool() },
                                                   { "lastCheck", settings.value("last_check").toString() },
                                                   { "updateSuccessMarker", QFileInfo::exists(marker) },
                                                   { "updateFailureMarker", QFileInfo::exists(failed) }, { "updateInProgress", QFileInfo::exists(lock) },
                                                   { "updateLog", QString::fromUtf8(logTail) }, { "updateLogPath", log },
                                                   { "logTruncated", logFile.size() > 262144 },
                                                   { "dataRoot", APPLICATION->dataRoot() } });
}

QJsonObject updateConfigure(const QJsonObject& p)
{
    auto settings = updateSettings();
    if (p.contains("intervalSeconds") && (p.value("intervalSeconds").toInt() < 0 || p.value("intervalSeconds").toInt() > 31536000))
        return OperationService::failure("intervalSeconds is out of range.", 2);
    if (p.contains("automatic")) settings.setValue("auto_check", p.value("automatic").toBool());
    if (p.contains("intervalSeconds")) {
        const auto seconds = p.value("intervalSeconds").toInt();
        if (seconds < 0 || seconds > 31536000) return OperationService::failure("intervalSeconds is out of range.", 2);
        settings.setValue("update_interval", seconds);
    }
    if (p.contains("beta")) settings.setValue("allow_beta", p.value("beta").toBool());
    settings.sync();
    if (settings.status() != QSettings::NoError) return OperationService::failure("Could not save update preferences.");
    if (const auto updater = APPLICATION->updater()) {
        if (p.contains("automatic")) updater->setAutomaticallyChecksForUpdates(p.value("automatic").toBool());
        if (p.contains("beta")) updater->setBetaAllowed(p.value("beta").toBool());
        if (p.contains("intervalSeconds")) updater->setUpdateCheckInterval(p.value("intervalSeconds").toInt());
    }
    return updateStatus();
}

QString updaterPath()
{
#ifdef Q_OS_WIN
    const auto name = QString(BuildConfig.LAUNCHER_APP_BINARY_NAME) + "_updater.exe";
#else
    const auto name = "bin/" + QString(BuildConfig.LAUNCHER_APP_BINARY_NAME) + "_updater";
#endif
    return QDir(APPLICATION->root()).filePath(name);
}

QJsonObject updateCheck(LauncherApi& api, bool releases = false)
{
    const auto updater = updaterPath();
    if (!QFileInfo::exists(updater)) return OperationService::failure("The external updater executable is not installed.", 2);
    QProcess process;
    auto settings = updateSettings();
    QStringList arguments{ releases ? "--json-releases" : "--check-only", "--dir", APPLICATION->dataRoot(), "--debug" };
    if (settings.value("allow_beta", false).toBool()) arguments.append("--pre-release");
    QEventLoop loop;
    QTimer timeout, cancellation; timeout.setSingleShot(true); cancellation.setInterval(100);
    bool timedOut = false;
    QObject::connect(&process, &QProcess::finished, &loop, &QEventLoop::quit);
    QObject::connect(&process, &QProcess::errorOccurred, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&] { timedOut = true; process.kill(); loop.quit(); });
    QObject::connect(&cancellation, &QTimer::timeout, &loop, [&] { if (api.isCancellationRequested()) { process.kill(); loop.quit(); } });
    process.start(updater, arguments); timeout.start(60000); cancellation.start(); loop.exec();
    if (process.state() != QProcess::NotRunning) { process.kill(); process.waitForFinished(5000); }
    if (timedOut || api.isCancellationRequested()) return OperationService::failure(timedOut ? "Updater check timed out." : "Updater check cancelled.");
    if (process.error() == QProcess::FailedToStart) return OperationService::failure(process.errorString());
    const auto output = QString::fromLocal8Bit(process.readAllStandardOutput());
    const auto error = QString::fromLocal8Bit(process.readAllStandardError());
    if (process.exitStatus() != QProcess::NormalExit || (process.exitCode() != 0 && process.exitCode() != 100))
        return OperationService::failure("Updater check failed: " + error);
    if (releases) {
        const auto document = QJsonDocument::fromJson(output.toUtf8());
        return document.isArray() ? OperationService::success(document.array()) : OperationService::failure("Updater returned an invalid release catalog.");
    }
    settings.setValue("last_check", QDateTime::currentDateTime().toString(Qt::ISODate)); settings.sync();
    const auto lines = output.split('\n');
    return OperationService::success(QJsonObject{ { "exitCode", process.exitCode() }, { "available", process.exitCode() == 100 },
        { "versionName", process.exitCode() == 100 ? lines.value(0).section(": ", 1).trimmed() : QString() },
        { "versionTag", process.exitCode() == 100 ? lines.value(1).section(": ", 1).trimmed() : QString() },
        { "releaseTime", process.exitCode() == 100 ? lines.value(2).section(": ", 1).trimmed() : QString() },
        { "releaseNotes", process.exitCode() == 100 ? lines.mid(3).join('\n') : QString() },
        { "output", output }, { "error", error }, { "status", updateStatus().value("data").toObject() } });
}

QJsonObject updateApply(const QJsonObject& p)
{
    const auto tag = p.value("versionTag").toString().trimmed();
    if (tag.isEmpty() || tag.contains('/') || tag.contains('\\')) return OperationService::failure("versionTag is invalid.", 2);
    const auto updater = updaterPath();
    if (!QFileInfo::exists(updater)) return OperationService::failure("The external updater executable is not installed.", 2);
    QProcess process;
    auto settings = updateSettings();
    QStringList arguments{ "--dir", APPLICATION->dataRoot(), "--install-version", tag, "--headless", "--wait-pid", QString::number(QCoreApplication::applicationPid()) };
    if (!p.value("assetName").toString().isEmpty()) arguments.append({ "--asset", p.value("assetName").toString() });
    if (settings.value("allow_beta", false).toBool()) arguments.append("--pre-release");
    if (p.value("allowDowngrade").toBool()) arguments.append("--allow-downgrade");
    const auto started = process.startDetached(updater, arguments);
    if (!started) return OperationService::failure("The updater could not be started.");
    return OperationService::success(QJsonObject{ { "started", true }, { "versionTag", tag }, { "updater", updater },
        { "shutdownRequired", true }, { "externalUpdaterUi", false }, { "systemInstallerMayOpen", true }, { "restartByFrontend", true } });
}

QJsonObject readLog(const QJsonObject& parameters)
{
    auto instance = findInstance(parameters.value("instance").toString());
    if (!instance)
        return OperationService::failure(QObject::tr("Instance not found."), 2);
    const auto file = logFile(parameters);
    if (!file.exists())
        return OperationService::failure(QObject::tr("Log file not found: %1").arg(parameters.value("file").toString()), 2);
    QFile input(file.absoluteFilePath());
    if (!input.open(QIODevice::ReadOnly))
        return OperationService::failure(QObject::tr("Could not read log file: %1").arg(input.errorString()));
    const auto maxBytes = qBound<qint64>(1, parameters.value("maxBytes").toInteger(1024 * 1024), qint64(16 * 1024 * 1024));
    QByteArray bytes;
    bool truncated = false;
    if (file.fileName().endsWith(".gz")) {
        const auto error = GZip::readGzFileByBlocks(&input, [&](const QByteArray& block) {
            const auto available = maxBytes - bytes.size();
            bytes.append(block.first(qMin<qint64>(available, block.size())));
            truncated = block.size() > available;
            return !truncated;
        });
        if (!error.isEmpty()) return OperationService::failure(error);
    } else {
        bytes = input.read(maxBytes); truncated = !input.atEnd();
    }
    return OperationService::success(QJsonObject{ { "path", file.absoluteFilePath() },
        { "content", QString::fromUtf8(bytes) }, { "truncated", truncated }, { "compressed", file.fileName().endsWith(".gz") } });
}

QJsonObject deleteLog(const QJsonObject& parameters, bool clear)
{
    if (!parameters.value("confirm").toBool())
        return OperationService::failure(QObject::tr("Log modification requires confirm=true."), 2);
    auto instance = findInstance(parameters.value("instance").toString());
    if (!instance)
        return OperationService::failure(QObject::tr("Instance not found."), 2);
    QList<QFileInfo> files;
    if (parameters.contains("file")) {
        const auto file = logFile(parameters);
        if (file.exists()) files.append(file);
    } else {
        for (const auto& entry : listLogs(parameters).value("data").toArray()) files.append(QFileInfo(entry.toObject().value("path").toString()));
    }
    if (files.isEmpty())
        return OperationService::failure(QObject::tr("No matching log files found."), 2);
    int changed = 0;
    for (const auto& file : files) {
        if (clear) {
            QFile output(file.absoluteFilePath());
            if (output.open(QIODevice::WriteOnly | QIODevice::Truncate)) { output.close(); ++changed; }
        } else if (QFile::remove(file.absoluteFilePath())) {
            ++changed;
        }
    }
    return OperationService::success(QJsonObject{ { "changed", changed }, { "cleared", clear }, { "complete", changed == files.size() }, { "failed", files.size() - changed } });
}

QJsonObject uploadLog(LauncherApi& api, const QJsonObject& parameters, UserInteraction& interaction)
{
    QString content = parameters.value("content").toString();
    if (content.isEmpty() && parameters.contains("file")) {
        auto request = parameters;
        request.insert("maxBytes", 16 * 1024 * 1024);
        const auto read = readLog(request);
        if (!read.value("ok").toBool()) return read;
        if (read.value("data").toObject().value("truncated").toBool()) return OperationService::failure("Log exceeds the upload limit (16 MiB).", 2);
        content = read.value("data").toObject().value("content").toString();
    }
    if (content.isEmpty())
        return OperationService::failure(QObject::tr("Either content or file is required."), 2);
    int type = parameters.value("pasteType").toInt(APPLICATION->settings()->get("PastebinType").toInt());
    type = qBound<int>(PasteUpload::First, type, PasteUpload::Last);
    QString base = parameters.value("baseUrl").toString(APPLICATION->settings()->get("PastebinCustomAPIBase").toString());
    auto job = makeShared<NetJob>("API log upload", APPLICATION->network());
    auto upload = new PasteUpload(content, base, static_cast<PasteUpload::PasteType>(type));
    job->addNetAction(Net::NetRequest::Ptr(upload));
    QString error;
    if (!waitForAccountTask(job.get(), interaction, &error, api))
        return OperationService::failure(error);
    return OperationService::success(QJsonObject{ { "url", upload->pasteLink() }, { "pasteType", type } });
}

QJsonObject uploadScreenshot(LauncherApi& api, const QJsonObject& parameters, UserInteraction& interaction)
{
    auto instance = findInstance(parameters.value("instance").toString());
    if (!instance)
        return OperationService::failure(QObject::tr("Instance not found."), 2);
    const auto root = QDir(instance->gameRoot()).filePath("screenshots");
    const auto file = safeFileInRoots(parameters.value("file").toString(), { root });
    if (!file.exists())
        return OperationService::failure(QObject::tr("Screenshot not found."), 2);
    auto shot = std::make_shared<ScreenShot>(file);
    auto job = makeShared<NetJob>("API screenshot upload", APPLICATION->network());
    job->addNetAction(ImgurUpload::make(shot));
    QString error;
    if (!waitForAccountTask(job.get(), interaction, &error, api))
        return OperationService::failure(error);
    return OperationService::success(QJsonObject{ { "path", file.absoluteFilePath() }, { "url", shot->m_url }, { "id", shot->m_imgurId } });
}

QJsonObject uploadScreenshotAlbum(LauncherApi& api, const QJsonObject& parameters, UserInteraction& interaction)
{
    auto instance = findInstance(parameters.value("instance").toString());
    if (!instance) return OperationService::failure("Instance not found.", 2);
    const auto files = parameters.value("files").toArray();
    if (files.isEmpty() || files.size() > 100) return OperationService::failure("Select between 1 and 100 screenshots.", 2);
    const auto root = QDir(instance->gameRoot()).filePath("screenshots");
    QList<ScreenShot::Ptr> shots;
    auto uploads = makeShared<NetJob>("API screenshot uploads", APPLICATION->network());
    for (const auto& name : files) {
        const auto file = safeFileInRoots(name.toString(), { root });
        if (!file.isFile()) return OperationService::failure("Screenshot not found: " + name.toString(), 2);
        auto shot = std::make_shared<ScreenShot>(file);
        shots.append(shot); uploads->addNetAction(ImgurUpload::make(shot));
    }
    QString error;
    bool complete = waitForAccountTask(uploads.get(), interaction, &error, api);
    QJsonArray images;
    for (const auto& shot : shots) if (!shot->m_url.isEmpty()) images.append(QJsonObject{ { "url", shot->m_url }, { "id", shot->m_imgurId } });
    QJsonObject result{ { "images", images }, { "complete", false }, { "error", error } };
    if (!complete) return OperationService::success(result);
    auto albumResult = std::make_shared<ImgurAlbumCreation::Result>();
    auto album = makeShared<NetJob>("API screenshot album", APPLICATION->network());
    album->addNetAction(ImgurAlbumCreation::make(albumResult, shots));
    complete = waitForAccountTask(album.get(), interaction, &error, api) && !albumResult->id.isEmpty();
    result.insert("complete", complete); result.insert("error", error);
    if (complete) {
        result.insert("id", albumResult->id); result.insert("url", "https://imgur.com/a/" + albumResult->id);
        result.insert("deleteHash", albumResult->deleteHash);
    }
    return OperationService::success(result);
}

QJsonObject listScreenshots(const QJsonObject& parameters)
{
    auto instance = findInstance(parameters.value("instance").toString());
    if (!instance)
        return OperationService::failure(QObject::tr("Instance not found."), 2);
    const auto root = QDir(instance->gameRoot()).filePath("screenshots");
    QJsonArray result;
    const auto entries = QDir(root).entryInfoList({ "*.png", "*.jpg", "*.jpeg" }, QDir::Files | QDir::Readable, QDir::Name);
    for (const auto& entry : entries)
        result.append(QJsonObject{ { "name", entry.fileName() }, { "path", entry.absoluteFilePath() },
                                   { "size", static_cast<qint64>(entry.size()) },
                                   { "modified", entry.lastModified().toString(Qt::ISODate) } });
    return OperationService::success(result);
}

QJsonObject deleteScreenshot(const QJsonObject& parameters)
{
    if (!parameters.value("confirm").toBool())
        return OperationService::failure(QObject::tr("Screenshot deletion requires confirm=true."), 2);
    auto instance = findInstance(parameters.value("instance").toString());
    if (!instance)
        return OperationService::failure(QObject::tr("Instance not found."), 2);
    const auto root = QDir(instance->gameRoot()).filePath("screenshots");
    const auto file = safeFileInRoots(parameters.value("file").toString(), { root });
    if (!file.exists() || !QFile::remove(file.absoluteFilePath()))
        return OperationService::failure(QObject::tr("Screenshot could not be deleted."));
    return OperationService::success(QJsonObject{ { "path", file.absoluteFilePath() }, { "deleted", true } });
}

}  // namespace

QJsonObject launcherAccountSnapshot()
{
    const auto list = APPLICATION->accounts();
    const auto selected = list->defaultAccount();
    QJsonArray accounts;
    const QStringList states{ "unchecked", "offline", "working", "online", "disabled", "errored", "expired", "gone" };
    for (int i = 0; i < list->count(); ++i) {
        const auto account = list->at(i);
        const auto data = account->accountData();
        const auto& profile = data->minecraftProfile;
        QJsonArray capes;
        for (auto it = profile.capes.constBegin(); it != profile.capes.constEnd(); ++it)
            capes.append(QJsonObject{ { "id", it.value().id }, { "url", it.value().url }, { "alias", it.value().alias },
                                     { "current", it.key() == profile.currentCape } });
        accounts.append(QJsonObject{ { "index", i }, { "id", account->profileId() }, { "internalId", account->internalId() },
            { "name", account->profileName() }, { "type", account->typeString() }, { "state", static_cast<int>(data->accountState) },
            { "stateName", states.value(static_cast<int>(data->accountState), "unknown") }, { "active", account->isActive() },
            { "lastError", account->lastError() }, { "default", account == selected },
            { "ownsMinecraft", data->minecraftEntitlement.ownsMinecraft }, { "canPlayMinecraft", data->minecraftEntitlement.canPlayMinecraft },
            { "skinUrl", profile.skin.url }, { "cape", profile.currentCape }, { "capes", capes },
            { "skin", QJsonObject{ { "id", profile.skin.id }, { "url", profile.skin.url }, { "variant", profile.skin.variant } } } });
    }
    return { { "accounts", accounts }, { "count", accounts.size() }, { "active", list->isActive() },
             { "defaultAccount", selected ? QJsonValue(selected->internalId()) : QJsonValue(QJsonValue::Null) } };
}

QJsonObject launcherUpdateSnapshot() { return updateStatus().value("data").toObject(); }

void registerLauncherApiDomains(LauncherApi& api)
{
    api.registerOperation({ "launcher.news", "Load launcher news using the existing feed parser and cache. Content contains HTML to sanitize in the frontend.", objectSchema({}), "launcher" },
        [&api](const QJsonObject&, UserInteraction& interaction) {
            class ApiNews : public NewsChecker {
            public:
                ApiNews() : NewsChecker(APPLICATION->network(), BuildConfig.NEWS_RSS_URL) {}
                Task::Ptr task() const { return m_newsNetJob; }
            } news;
            news.reloadNews();
            const auto task = news.task();
            QString error;
            if (task && !waitForAccountTask(task.get(), interaction, &error, api)) return OperationService::failure(error);
            if (!news.getLastLoadErrorMsg().isEmpty()) return OperationService::failure(news.getLastLoadErrorMsg());
            QJsonArray entries;
            for (const auto& entry : news.getNewsEntries()) entries.append(QJsonObject{ { "title", entry->title }, { "content", entry->content }, { "url", entry->link } });
            return OperationService::success(entries);
        });
    api.registerOperation({ "launcher.cache.clear", "Evict metadata cache using the same operation as the launcher menu.",
        objectSchema({ { "confirm", boolProperty("Confirm clearing cached metadata.") } }, { "confirm" }), "launcher", true },
        [](const QJsonObject& p, UserInteraction&) {
            if (!p.value("confirm").toBool()) return OperationService::failure("Cache clearing requires confirm=true.", 2);
            const auto cleared = APPLICATION->metacache()->evictAll(); APPLICATION->metacache()->SaveNow();
            return cleared ? OperationService::success() : OperationService::failure("Some metadata cache files could not be cleared.");
        });
    api.registerOperation({ "instance.world.edit", "Open a world in the configured MCEdit executable.",
        objectSchema({ { "instance", stringProperty("Instance ID.") }, { "world", stringProperty("World folder name.") } }, { "instance", "world" }), "worlds", true },
        [](const QJsonObject& p, UserInteraction&) {
            auto instance = dynamic_cast<MinecraftInstance*>(findInstance(p.value("instance").toString()));
            if (!instance || instance->isRunning()) return OperationService::failure("A stopped Minecraft instance is required.", 2);
            auto worlds = instance->worldList(); worlds->update();
            for (const auto& world : worlds->allWorlds()) {
                if (world.folderName() != p.value("world").toString()) continue;
                const auto editor = APPLICATION->mcedit();
                QString error;
                if (!editor->check(editor->path(), error)) return OperationService::failure(error);
                const auto started = QProcess::startDetached(editor->getProgramPath(), { world.container().absoluteFilePath() }, editor->path());
                return started ? OperationService::success(QJsonObject{ { "started", true } }) : OperationService::failure("MCEdit could not start.");
            }
            return OperationService::failure("World not found.", 2);
        });
    api.registerOperation({ "settings.import-prism", "Import compatible Prism Launcher settings while retaining Luna-only settings.",
        objectSchema({ { "confirm", boolProperty("Confirm overwriting matching settings.") } }, { "confirm" }), "settings", true },
        [](const QJsonObject& p, UserInteraction&) {
            if (!p.value("confirm").toBool()) return OperationService::failure("Import requires confirm=true.", 2);
            QString error;
            const auto count = APPLICATION->importPrismSettings(&error);
            return count < 0 ? OperationService::failure(error) : OperationService::success(QJsonObject{ { "imported", count } });
        });
    for (const QString action : { "list", "add", "edit", "remove" }) {
        QJsonObject properties{ { "name", stringProperty("Preset name.") }, { "authUrl", stringProperty("Authentication URL.") },
            { "originalName", stringProperty("Existing custom preset name when renaming; defaults to name.") },
            { "sessionUrl", stringProperty("Session URL.") }, { "confirm", boolProperty("Confirm removal.") } };
        for (const auto key : { "authenticateEndpoint", "refreshEndpoint", "validateEndpoint", "profileEndpoint", "oauthTokenEndpoint", "tokenType" })
            properties.insert(key, stringProperty(QString::fromLatin1(key)));
        api.registerOperation({ "account.preset." + action, "Manage Yggdrasil authentication presets: " + action + ".", objectSchema(properties), "accounts", action != "list" },
            [action](const QJsonObject& p, UserInteraction&) {
                if (action == "list") {
                    QJsonArray result;
                    const auto defaults = YggdrasilPresets::getDefaults();
                    const auto all = YggdrasilPresets::getAllPresets();
                    for (int row = 0; row < all.size(); ++row) {
                        const auto& v = all[row];
                        result.append(QJsonObject{ { "name", v.name }, { "authUrl", v.authUrl }, { "sessionUrl", v.sessionUrl },
                            { "authenticateEndpoint", v.authenticateEndpoint }, { "refreshEndpoint", v.refreshEndpoint },
                            { "validateEndpoint", v.validateEndpoint }, { "profileEndpoint", v.profileEndpoint }, { "oauthTokenEndpoint", v.oauthTokenEndpoint },
                            { "tokenType", v.tokenType == YggdrasilTokenType::OAuth ? "OAuth" : "Standard" }, { "builtin", row < defaults.size() } });
                    }
                    return OperationService::success(result);
                }
                const auto name = p.value("name").toString().trimmed();
                if (name.isEmpty()) return OperationService::failure("Preset name is required.", 2);
                if (action == "remove") {
                    if (!p.value("confirm").toBool()) return OperationService::failure("Removal requires confirm=true.", 2);
                    return YggdrasilPresets::removeCustomPreset(name) ? OperationService::success() : OperationService::failure("Custom preset could not be removed.");
                }
                const auto auth = QUrl(p.value("authUrl").toString()), session = QUrl(p.value("sessionUrl").toString());
                for (const auto& url : { auth, session }) if (!url.isValid() || url.host().isEmpty() || (url.scheme() != "http" && url.scheme() != "https"))
                    return OperationService::failure("Preset authentication and session URLs must be HTTP(S).", 2);
                const auto tokenType = p.value("tokenType").toString("Standard");
                if (tokenType != "Standard" && tokenType != "OAuth") return OperationService::failure("tokenType must be Standard or OAuth.", 2);
                YggdrasilPreset preset;
                preset.name = name; preset.authUrl = auth.toString(); preset.sessionUrl = session.toString();
                preset.authenticateEndpoint = p.value("authenticateEndpoint").toString(); preset.refreshEndpoint = p.value("refreshEndpoint").toString();
                preset.validateEndpoint = p.value("validateEndpoint").toString(); preset.profileEndpoint = p.value("profileEndpoint").toString();
                preset.oauthTokenEndpoint = p.value("oauthTokenEndpoint").toString(); preset.tokenType = tokenType == "OAuth" ? YggdrasilTokenType::OAuth : YggdrasilTokenType::Standard;
                return YggdrasilPresets::addCustomPreset(preset, action == "edit" ? p.value("originalName").toString(name) : QString()) ? OperationService::success() : OperationService::failure("Preset conflicts, does not exist, or could not be saved.");
            });
    }
    api.registerOperation({ "instance.screenshot.upload-album", "Upload selected screenshots and create an Imgur album; partial uploads are returned on failure.",
        objectSchema({ { "instance", stringProperty("Instance ID.") }, { "files", QJsonObject{ { "type", "array" },
            { "items", QJsonObject{ { "type", "string" } } }, { "minItems", 1 }, { "maxItems", 100 } } } }, { "instance", "files" }), "screenshots", true },
        [&api](const QJsonObject& p, UserInteraction& interaction) { return uploadScreenshotAlbum(api, p, interaction); });
    api.registerOperation({ "runtime.info", "Read launcher paths, platform, and optional integration capabilities.", objectSchema({}) },
                           [](const QJsonObject&, UserInteraction&) {
                               const auto caps = APPLICATION->capabilities();
                               QJsonArray capabilities;
                               if (caps & Application::SupportsMSA)
                                   capabilities.append("microsoft-auth");
                               if (caps & Application::SupportsFlame)
                                   capabilities.append("curseforge");
                               if (caps & Application::SupportsGameMode)
                                   capabilities.append("game-mode");
                               if (caps & Application::SupportsMangoHud)
                                   capabilities.append("mangohud");
                               return OperationService::success(QJsonObject{ { "applicationVersion", QCoreApplication::applicationVersion() },
                                                                               { "root", APPLICATION->root() },
                                                                               { "dataRoot", APPLICATION->dataRoot() },
                                                                               { "javaRoot", APPLICATION->javaPath() },
                                                                               { "portable", APPLICATION->isPortable() },
                                                                               { "headless", APPLICATION->isHeadless() },
                                                                               { "capabilities", capabilities } });
                           });
    api.registerOperation({ "network.proxy.get", "Read the effective launcher proxy configuration.", objectSchema({}) },
                           [](const QJsonObject&, UserInteraction&) { return proxyInfo(); });
    api.registerOperation({ "network.proxy.set", "Set the launcher HTTP/SOCKS5/system proxy configuration.",
                            objectSchema({ { "type", stringProperty("None, HTTP, SOCKS5, or Default.") }, { "host", stringProperty("Proxy host.") },
                                           { "port", QJsonObject{ { "type", "integer" } } }, { "username", stringProperty("Optional username.") },
                                           { "password", stringProperty("Optional password.") } }) },
                           [](const QJsonObject& p, UserInteraction&) { return proxySet(p); });
    api.registerOperation({ "network.proxy.test", "Test HTTPS connectivity through the currently configured launcher proxy.",
                            objectSchema({ { "url", stringProperty("HTTPS URL to test.") }, { "timeoutMs", QJsonObject{ { "type", "integer" } } } }) },
                           [](const QJsonObject& p, UserInteraction&) { return proxyTest(p); });
    api.registerOperation({ "launcher.update.status", "Read launcher updater state and persisted preferences.", objectSchema({}) },
                           [](const QJsonObject&, UserInteraction&) { return updateStatus(); });
    api.registerOperation({ "launcher.update.configure", "Configure automatic, beta, and interval updater preferences.",
                            objectSchema({ { "automatic", boolProperty("Enable automatic update checks.") },
                                           { "intervalSeconds", QJsonObject{ { "type", "integer" } } },
                                           { "beta", boolProperty("Allow pre-release updates.") } }) },
                           [](const QJsonObject& p, UserInteraction&) { return updateConfigure(p); });
    api.registerOperation({ "launcher.update.check", "Run the installed external updater in check-only mode.", objectSchema({}) },
                           [&api](const QJsonObject&, UserInteraction&) { return updateCheck(api); });
    api.registerOperation({ "launcher.update.releases", "List releases and compatible assets from the installed updater.", objectSchema({}), "updates" },
                           [&api](const QJsonObject&, UserInteraction&) { return updateCheck(api, true); });
    api.registerOperation({ "launcher.update.apply", "Start the installed updater to download and apply a selected release.",
                            objectSchema({ { "versionTag", stringProperty("Release tag returned by launcher.update.check.") },
                                { "allowDowngrade", boolProperty("Allow selecting an older release.") },
                                { "assetName", stringProperty("Exact release asset name when multiple assets match.") } }, { "versionTag" }) },
                           [](const QJsonObject& p, UserInteraction&) { return updateApply(p); });
    api.registerOperation({ "desktop.open-path", "Open a local path using the operating system desktop handler.",
                            objectSchema({ { "path", stringProperty("Local file or directory path.") }, { "select", boolProperty("Select the item in the file manager.") } }, { "path" }) },
                           [](const QJsonObject& p, UserInteraction&) {
                               const QFileInfo path(p.value("path").toString());
                               if (!path.exists()) return OperationService::failure(QObject::tr("Path does not exist."), 2);
                               if (p.value("select").toBool()) {
                                   bool started = false;
#ifdef Q_OS_WIN
                                   started = QProcess::startDetached("explorer.exe", { "/select,", QDir::toNativeSeparators(path.absoluteFilePath()) });
#elif defined(Q_OS_MACOS)
                                   started = QProcess::startDetached("open", { "-R", path.absoluteFilePath() });
#else
                                   QProcess process;
                                   process.start("gdbus", { "call", "--session", "--dest", "org.freedesktop.FileManager1", "--object-path", "/org/freedesktop/FileManager1",
                                       "--method", "org.freedesktop.FileManager1.ShowItems", "['" + QUrl::fromLocalFile(path.absoluteFilePath()).toString(QUrl::FullyEncoded) + "']", "" });
                                   started = process.waitForFinished(5000) && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
#endif
                                   return started ? OperationService::success(QJsonObject{ { "path", path.absoluteFilePath() }, { "selected", true } }) : OperationService::failure("File manager could not select the item.");
                               }
                               const auto opened = DesktopServices::openPath(path);
                               return opened ? OperationService::success(QJsonObject{ { "path", path.absoluteFilePath() }, { "opened", true } }) : OperationService::failure("The desktop handler could not open the path.");
                           });
    api.registerOperation({ "desktop.open-url", "Open a URL using the operating system browser.",
                            objectSchema({ { "url", stringProperty("HTTP(S) or supported desktop URL.") } }, { "url" }) },
                           [](const QJsonObject& p, UserInteraction&) {
                               const QUrl url(p.value("url").toString());
                               if (!url.isValid() || url.scheme().isEmpty()) return OperationService::failure(QObject::tr("URL is invalid."), 2);
                               if (url.scheme() != "https" && url.scheme() != "http") return OperationService::failure("Only HTTP(S) URLs are accepted.", 2);
                               return DesktopServices::openUrl(url) ? OperationService::success(QJsonObject{ { "url", url.toString() }, { "opened", true } }) : OperationService::failure("The desktop handler could not open the URL.");
                           });
    api.registerOperation({ "desktop.clipboard.write", "Copy text, an image, or local file URLs to the system clipboard.",
        objectSchema({ { "text", stringProperty("Text to copy.") }, { "imagePath", stringProperty("Local image path.") },
            { "files", QJsonObject{ { "type", "array" }, { "items", QJsonObject{ { "type", "string" } } }, { "minItems", 1 }, { "maxItems", 256 } } } }), "desktop", true },
        [](const QJsonObject& p, UserInteraction&) {
            if (int(p.contains("text")) + int(p.contains("imagePath")) + int(p.contains("files")) != 1)
                return OperationService::failure("Provide exactly one of text, imagePath, or files.", 2);
            if (QGuiApplication::platformName() == "offscreen" || QGuiApplication::platformName() == "minimal")
                return OperationService::failure("System clipboard is unavailable on this headless platform; use the frontend clipboard.", 2);
            const auto clipboard = QGuiApplication::clipboard();
            if (!clipboard) return OperationService::failure("System clipboard is unavailable.", 2);
            if (p.contains("text")) clipboard->setText(p.value("text").toString());
            else if (p.contains("imagePath")) {
                QImageReader reader(p.value("imagePath").toString());
                const auto size = reader.size();
                if (!size.isValid() || qint64(size.width()) * size.height() > 32 * 1024 * 1024) return OperationService::failure("Image is invalid or too large.", 2);
                const auto image = reader.read();
                if (image.isNull()) return OperationService::failure(reader.errorString());
                clipboard->setImage(image);
            } else {
                QList<QUrl> urls;
                for (const auto& entry : p.value("files").toArray()) {
                    const QFileInfo file(entry.toString());
                    if (!file.exists()) return OperationService::failure("Clipboard file does not exist.", 2);
                    urls.append(QUrl::fromLocalFile(file.absoluteFilePath()));
                }
                auto mime = new QMimeData; mime->setUrls(urls); clipboard->setMimeData(mime);
            }
            return OperationService::success(QJsonObject{ { "copied", true } });
        });

    api.registerOperation({ "account.move", "Move an account by a relative offset or to an absolute list position.",
                            objectSchema({ { "account", stringProperty("Account ID or profile name.") },
                                           { "delta", QJsonObject{ { "type", "integer" }, { "description", "Relative movement." } } },
                                           { "index", QJsonObject{ { "type", "integer" }, { "description", "Absolute list position." } } } },
                                          { "account" }) },
                           [](const QJsonObject& parameters, UserInteraction&) {
                               int row = -1;
                               const auto account = findAccount(parameters.value("account").toString(), &row);
                               if (!account)
                                   return OperationService::failure(QObject::tr("Account not found: %1").arg(parameters.value("account").toString()), 2);
                               int delta = parameters.value("delta").toInt(0);
                               if (parameters.contains("index"))
                                   delta = parameters.value("index").toInt() - row;
                               if (delta == 0)
                                   return OperationService::success(QJsonObject{ { "id", account->profileId() }, { "index", row }, { "changed", false } });
                               if (row + delta < 0 || row + delta >= APPLICATION->accounts()->count())
                                   return OperationService::failure(QObject::tr("The requested account position is outside the account list."), 2);
                               APPLICATION->accounts()->moveAccount(APPLICATION->accounts()->index(row, 0), delta);
                               APPLICATION->accounts()->saveList();
                               return OperationService::success(QJsonObject{ { "id", account->profileId() },
                                                                               { "index", row + delta },
                                                                               { "changed", true } });
                           });

    api.registerOperation({ "account.profile", "Read the selected Minecraft profile, skin, and capes for an account.",
                            objectSchema({ { "account", stringProperty("Account ID or profile name.") } }, { "account" }) },
                           [](const QJsonObject& parameters, UserInteraction&) {
                               const auto account = findAccount(parameters.value("account").toString());
                               if (!account)
                                   return OperationService::failure(QObject::tr("Account not found: %1").arg(parameters.value("account").toString()), 2);

                               const auto* data = account->accountData();
                               const auto& profile = data->minecraftProfile;
                               QJsonObject skin{ { "id", profile.skin.id }, { "url", profile.skin.url }, { "variant", profile.skin.variant } };
                               QJsonArray capes;
                               for (auto it = profile.capes.constBegin(); it != profile.capes.constEnd(); ++it)
                                   capes.append(QJsonObject{ { "id", it.value().id }, { "url", it.value().url }, { "alias", it.value().alias },
                                                             { "current", it.key() == profile.currentCape } });

                               QString state;
                               switch (data->accountState) {
                                   case AccountState::Unchecked:
                                       state = "unchecked";
                                       break;
                                   case AccountState::Offline:
                                       state = "offline";
                                       break;
                                   case AccountState::Working:
                                       state = "working";
                                       break;
                                   case AccountState::Online:
                                       state = "online";
                                       break;
                                   case AccountState::Disabled:
                                       state = "disabled";
                                       break;
                                   case AccountState::Errored:
                                       state = "errored";
                                       break;
                                   case AccountState::Expired:
                                       state = "expired";
                                       break;
                                   case AccountState::Gone:
                                       state = "gone";
                                       break;
                               }
                               return OperationService::success(QJsonObject{ { "id", account->profileId() },
                                                                               { "name", account->profileName() },
                                                                               { "type", account->typeString() },
                                                                               { "state", state },
                                                                               { "ownsMinecraft", data->minecraftEntitlement.ownsMinecraft },
                                                                               { "canPlayMinecraft", data->minecraftEntitlement.canPlayMinecraft },
                                                                               { "skin", skin },
                                                                               { "currentCape", profile.currentCape },
                                                                               { "capes", capes } });
                           });
    api.registerOperation({ "account.snapshot", "Read a complete snapshot of all accounts and the selected account.", objectSchema({}) },
                           [](const QJsonObject&, UserInteraction&) {
                               return OperationService::success(launcherAccountSnapshot());
                           });

    const auto accountRef = stringProperty("Account ID or profile name.");
    api.registerOperation({ "account.skin.list", "List locally stored skins for an account.",
                            objectSchema({ { "account", accountRef } }, { "account" }), "account.skins" },
                           [](const QJsonObject& parameters, UserInteraction&) { return listAccountSkins(parameters); });
    api.registerOperation({ "account.skin.install", "Add a local PNG skin to the launcher skin library.",
                            objectSchema({ { "account", accountRef }, { "path", stringProperty("Skin PNG path.") },
                                           { "name", stringProperty("Optional skin file name.") } },
                                          { "account", "path" }), "account.skins" },
                           [](const QJsonObject& parameters, UserInteraction&) { return installAccountSkin(parameters); });
    api.registerOperation({ "account.skin.delete", "Delete a local skin from the launcher skin library.",
                            objectSchema({ { "account", accountRef }, { "skin", stringProperty("Skin name or path.") },
                                           { "confirm", boolProperty("Confirm deletion.") }, { "trash", boolProperty("Move to trash.") } },
                                          { "account", "skin", "confirm" }),
                            "account.skins", true },
                           [](const QJsonObject& parameters, UserInteraction&) { return deleteAccountSkin(parameters); });
    api.registerOperation({ "account.skin.upload", "Upload a local skin and optionally equip a cape.",
                            objectSchema({ { "account", accountRef }, { "skin", stringProperty("Skin name or path.") },
                                           { "variant", stringProperty("classic or slim.") }, { "cape", stringProperty("Cape id, or empty to remove it.") } },
                                          { "account", "skin" }),
                            "account.skins", true },
                           [&api](const QJsonObject& parameters, UserInteraction& interaction) {
                               return uploadAccountSkin(api, parameters, interaction);
                           });
    api.registerOperation({ "account.skin.reset", "Remove the active remote skin.",
                            objectSchema({ { "account", accountRef }, { "confirm", boolProperty("Confirm reset.") } }, { "account", "confirm" }),
                            "account.skins", true },
                           [&api](const QJsonObject& parameters, UserInteraction& interaction) {
                               return resetAccountSkin(api, parameters, interaction);
                           });
    api.registerOperation({ "account.cape.select", "Equip or remove an account cape.",
                            objectSchema({ { "account", accountRef }, { "cape", stringProperty("Cape id, or empty to remove it.") } },
                                          { "account" }),
                            "account.skins", true },
                           [&api](const QJsonObject& parameters, UserInteraction& interaction) {
                               return selectAccountCape(api, parameters, interaction);
                           });

    api.registerOperation({ "instance.world.list", "List worlds stored in a Minecraft instance.",
                            objectSchema({ { "instance", stringProperty("Instance ID or name.") } }, { "instance" }) },
                           [](const QJsonObject& parameters, UserInteraction&) {
                               auto instance = dynamic_cast<MinecraftInstance*>(findInstance(parameters.value("instance").toString()));
                               if (!instance)
                                   return OperationService::failure(QObject::tr("Minecraft instance not found: %1").arg(parameters.value("instance").toString()), 2);
                               auto worlds = instance->worldList();
                               worlds->update();
                               QJsonArray result;
                               for (const auto& world : worlds->allWorlds())
                                   result.append(worldJson(world));
                               return OperationService::success(result);
                           });

    api.registerOperation({ "instance.world.rename", "Rename a world folder and its level name.",
                            objectSchema({ { "instance", stringProperty("Instance ID or name.") },
                                           { "world", stringProperty("World folder or display name.") },
                                           { "name", stringProperty("New world name.") } },
                                          { "instance", "world", "name" }) },
                           [](const QJsonObject& parameters, UserInteraction&) {
                               auto instance = dynamic_cast<MinecraftInstance*>(findInstance(parameters.value("instance").toString()));
                               if (!instance)
                                   return OperationService::failure(QObject::tr("Minecraft instance not found."), 2);
                               if (instance->isRunning())
                                   return OperationService::failure(QObject::tr("Worlds cannot be modified while the instance is running."), 2);
                               auto worlds = instance->worldList();
                               worlds->update();
                               auto world = findWorld(worlds.get(), parameters.value("world").toString());
                               const auto name = parameters.value("name").toString().trimmed();
                               if (!world || name.isEmpty())
                                   return OperationService::failure(QObject::tr("A world and non-empty name are required."), 2);
                               if (!world->rename(name))
                                   return OperationService::failure(QObject::tr("The world could not be renamed."));
                               worlds->update();
                               return OperationService::success(QJsonObject{ { "name", name }, { "changed", true } });
                           });

    api.registerOperation({ "instance.world.delete", "Delete a world from a Minecraft instance.",
                            objectSchema({ { "instance", stringProperty("Instance ID or name.") },
                                           { "world", stringProperty("World folder or display name.") },
                                           { "confirm", boolProperty("Confirm deletion.") } },
                                          { "instance", "world", "confirm" }), {}, true },
                           [](const QJsonObject& parameters, UserInteraction&) {
                               if (!parameters.value("confirm").toBool())
                                   return OperationService::failure(QObject::tr("World deletion requires confirm=true."), 2);
                               auto instance = dynamic_cast<MinecraftInstance*>(findInstance(parameters.value("instance").toString()));
                               if (!instance)
                                   return OperationService::failure(QObject::tr("Minecraft instance not found."), 2);
                               if (instance->isRunning())
                                   return OperationService::failure(QObject::tr("Worlds cannot be modified while the instance is running."), 2);
                               auto worlds = instance->worldList();
                               worlds->update();
                               auto world = findWorld(worlds.get(), parameters.value("world").toString());
                               if (!world || !world->destroy())
                                   return OperationService::failure(QObject::tr("The world could not be deleted."));
                               worlds->update();
                               return OperationService::success(QJsonObject{ { "deleted", true } });
                           });

    api.registerOperation({ "instance.world.reset-icon", "Remove the icon associated with a world.",
                            objectSchema({ { "instance", stringProperty("Instance ID or name.") },
                                           { "world", stringProperty("World folder or display name.") } },
                                          { "instance", "world" }) },
                           [](const QJsonObject& parameters, UserInteraction&) {
                               auto instance = dynamic_cast<MinecraftInstance*>(findInstance(parameters.value("instance").toString()));
                               if (!instance)
                                   return OperationService::failure(QObject::tr("Minecraft instance not found."), 2);
                               auto worlds = instance->worldList();
                               worlds->update();
                               auto world = findWorld(worlds.get(), parameters.value("world").toString());
                               if (!world || !world->resetIcon())
                                   return OperationService::failure(QObject::tr("The world icon could not be removed."));
                               worlds->update();
                               return OperationService::success(QJsonObject{ { "reset", true } });
                           });

    api.registerOperation({ "instance.world.import", "Import a world directory or zip archive into a stopped Minecraft instance.",
                            objectSchema({ { "instance", stringProperty("Instance ID or name.") },
                                           { "source", stringProperty("Local world directory or zip archive.") },
                                           { "name", stringProperty("Optional world name override.") },
                                           { "replace", boolProperty("Replace an existing destination when possible.") } },
                                          { "instance", "source" }),
                            "worlds", true },
                           [](const QJsonObject& parameters, UserInteraction&) {
                               auto instance = dynamic_cast<MinecraftInstance*>(findInstance(parameters.value("instance").toString()));
                               if (!instance) return OperationService::failure(QObject::tr("Minecraft instance not found."), 2);
                               if (instance->isRunning()) return OperationService::failure(QObject::tr("Worlds cannot be imported while the instance is running."), 2);
                               const QFileInfo source(parameters.value("source").toString());
                               if (!source.exists() || (!source.isDir() && !source.isFile())) return OperationService::failure(QObject::tr("World source does not exist."), 2);
                               return installWorldSafely(instance, source, parameters.value("name").toString().trimmed(), parameters.value("replace").toBool());
                           });

    api.registerOperation({ "instance.world.copy", "Copy a world inside an instance to a new world name.",
                            objectSchema({ { "instance", stringProperty("Instance ID or name.") },
                                           { "world", stringProperty("Source world folder or display name.") },
                                           { "name", stringProperty("Destination world name.") },
                                           { "replace", boolProperty("Replace an existing destination.") } },
                                          { "instance", "world", "name" }) },
                           [](const QJsonObject& parameters, UserInteraction&) {
                               auto instance = dynamic_cast<MinecraftInstance*>(findInstance(parameters.value("instance").toString()));
                               if (!instance) return OperationService::failure(QObject::tr("Minecraft instance not found."), 2);
                               if (instance->isRunning()) return OperationService::failure(QObject::tr("Worlds cannot be copied while the instance is running."), 2);
                               const auto name = parameters.value("name").toString().trimmed();
                               if (name.isEmpty() || name == "." || name == ".." || name.contains('/') || name.contains('\\'))
                                   return OperationService::failure(QObject::tr("Invalid destination world name."), 2);
                               auto worlds = instance->worldList(); worlds->update();
                               auto source = findWorld(worlds.get(), parameters.value("world").toString());
                               if (!source || !source->isOnFS()) return OperationService::failure(QObject::tr("Source world was not found."), 2);
                               return installWorldSafely(instance, source->container(), name, parameters.value("replace").toBool());
                           });
    api.registerOperation({ "instance.world.create", "Create a world from a local world template or archive.",
                            objectSchema({ { "instance", stringProperty("Instance ID or name.") }, { "template", stringProperty("Valid world directory or zip archive.") },
                                           { "name", stringProperty("New world name.") }, { "replace", boolProperty("Replace an existing world.") } },
                                          { "instance", "template", "name" }) },
                           [&api](const QJsonObject& p, UserInteraction& i) {
                               QJsonObject import{ { "instance", p.value("instance") }, { "source", p.value("template") }, { "name", p.value("name") }, { "replace", p.value("replace") } };
                               return api.execute("instance.world.import", import, i);
                           });
    api.registerOperation({ "instance.world.copy-to", "Copy a world from one Minecraft instance into another.",
                            objectSchema({ { "sourceInstance", stringProperty("Source instance ID or name.") }, { "targetInstance", stringProperty("Target instance ID or name.") },
                                           { "world", stringProperty("Source world folder or display name.") }, { "name", stringProperty("Destination world name.") },
                                           { "replace", boolProperty("Replace an existing destination.") } },
                                          { "sourceInstance", "targetInstance", "world", "name" }) },
                           [](const QJsonObject& p, UserInteraction&) {
                               auto source = dynamic_cast<MinecraftInstance*>(findInstance(p.value("sourceInstance").toString()));
                               auto target = dynamic_cast<MinecraftInstance*>(findInstance(p.value("targetInstance").toString()));
                               if (!source || !target) return OperationService::failure(QObject::tr("Source or target instance was not found."), 2);
                               if (source->isRunning() || target->isRunning()) return OperationService::failure(QObject::tr("Both instances must be stopped."), 2);
                               const auto name = p.value("name").toString().trimmed();
                               if (name.isEmpty() || name == "." || name == ".." || name.contains('/') || name.contains('\\')) return OperationService::failure(QObject::tr("Invalid destination world name."), 2);
                               auto worlds = source->worldList(); worlds->update();
                               auto world = findWorld(worlds.get(), p.value("world").toString());
                               if (!world || !world->isOnFS()) return OperationService::failure(QObject::tr("Source world was not found."), 2);
                               auto result = installWorldSafely(target, world->container(), name, p.value("replace").toBool());
                               if (result.value("ok").toBool()) {
                                   auto data = result.value("data").toObject();
                                   data.insert("sourceInstance", source->id()); data.insert("targetInstance", target->id());
                                   result.insert("data", data);
                               }
                               return result;
                           });

    api.registerOperation({ "instance.world.export", "Export a world directory as a zip archive.",
                            objectSchema({ { "instance", stringProperty("Instance ID or name.") },
                                           { "world", stringProperty("World folder or display name.") },
                                           { "output", stringProperty("Destination zip path.") },
                                           { "overwrite", boolProperty("Replace an existing output file.") } },
                                          { "instance", "world", "output" }),
                            "worlds", true },
                           [&api](const QJsonObject& parameters, UserInteraction& interaction) {
                               auto instance = dynamic_cast<MinecraftInstance*>(findInstance(parameters.value("instance").toString()));
                               if (!instance) return OperationService::failure(QObject::tr("Minecraft instance not found."), 2);
                               if (instance->isRunning()) return OperationService::failure(QObject::tr("Worlds cannot be exported while the instance is running."), 2);
                               auto worlds = instance->worldList();
                               worlds->update();
                               auto world = findWorld(worlds.get(), parameters.value("world").toString());
                               if (!world || !world->isOnFS()) return OperationService::failure(QObject::tr("World directory was not found."), 2);
                               const QFileInfo output(parameters.value("output").toString());
                               if (output.exists() && !parameters.value("overwrite").toBool()) return OperationService::failure(QObject::tr("Output exists; set overwrite=true."), 2);
                               if (!output.absoluteDir().exists() && !QDir().mkpath(output.absoluteDir().absolutePath())) return OperationService::failure(QObject::tr("Output directory could not be created."));
                               QFileInfoList files;
                               if (!MMCZip::collectFileListRecursively(world->container().absoluteFilePath(), nullptr, &files, {})) return OperationService::failure(QObject::tr("World files could not be enumerated."));
                               auto task = makeShared<MMCZip::ExportToZipTask>(output.absoluteFilePath(), world->container().absoluteFilePath(), files, "", true);
                               QString error;
                               if (!waitForAccountTask(task.get(), interaction, &error, api)) return OperationService::failure(error);
                               return OperationService::success(QJsonObject{ { "instance", instance->id() }, { "world", world->folderName() },
                                   { "path", output.absoluteFilePath() }, { "files", files.size() }, { "changed", true } });
                           });

    api.registerOperation({ "instance.log.list", "List log files available for an instance.",
                            objectSchema({ { "instance", stringProperty("Instance ID or name.") } }, { "instance" }) },
                           [](const QJsonObject& parameters, UserInteraction&) { return listLogs(parameters); });
    api.registerOperation({ "instance.log.read", "Read a bounded portion of an instance log file.",
                            objectSchema({ { "instance", stringProperty("Instance ID or name.") },
                                           { "file", stringProperty("Log file name or path returned by log.list.") },
                                           { "maxBytes", QJsonObject{ { "type", "integer" }, { "default", 1048576 } } } },
                                          { "instance", "file" }) },
                           [](const QJsonObject& parameters, UserInteraction&) { return readLog(parameters); });
    api.registerOperation({ "instance.log.upload", "Upload an instance log or supplied text to the configured paste service.",
                            objectSchema({ { "instance", stringProperty("Instance ID or name, required when file is used.") },
                                           { "file", stringProperty("Log file name or path returned by log.list.") },
                                           { "content", stringProperty("Log text to upload.") },
                                           { "pasteType", QJsonObject{ { "type", "integer" }, { "description", "0=0x0.st, 1=hastebin, 2=paste.gg, 3=mclo.gs" } } },
                                           { "baseUrl", stringProperty("Optional paste service API base URL.") } }) },
                           [&api](const QJsonObject& parameters, UserInteraction& interaction) { return uploadLog(api, parameters, interaction); });
    api.registerOperation({ "instance.log.clear", "Truncate one or all instance log files.",
                            objectSchema({ { "instance", stringProperty("Instance ID or name.") }, { "file", stringProperty("Optional log file.") },
                                           { "confirm", boolProperty("Confirm truncation.") } }, { "instance", "confirm" }) },
                           [](const QJsonObject& parameters, UserInteraction&) { return deleteLog(parameters, true); });
    api.registerOperation({ "instance.log.delete", "Delete one or all instance log files.",
                            objectSchema({ { "instance", stringProperty("Instance ID or name.") }, { "file", stringProperty("Optional log file.") },
                                           { "confirm", boolProperty("Confirm deletion.") } }, { "instance", "confirm" }) },
                           [](const QJsonObject& parameters, UserInteraction&) { return deleteLog(parameters, false); });
    for (const QString action : { "read", "import", "rename" }) {
        QJsonObject properties{ { "instance", stringProperty("Instance ID.") }, { "file", stringProperty("Existing screenshot name or path.") },
            { "source", stringProperty("Local image to import.") }, { "name", stringProperty("Destination screenshot filename.") } };
        QJsonArray required{ "instance" };
        if (action == "import") required.append("source"); else required.append("file");
        if (action != "read") required.append("name");
        api.registerOperation({ "instance.screenshot." + action, "Screenshot operation: " + action + ".", objectSchema(properties, required), "screenshots", action != "read" },
            [action](const QJsonObject& p, UserInteraction&) {
                auto instance = findInstance(p.value("instance").toString());
                if (!instance) return OperationService::failure("Instance not found.", 2);
                const auto root = QDir(instance->gameRoot()).filePath("screenshots");
                if (QFileInfo(root).isSymLink()) return OperationService::failure("Screenshot directory must not be a link.", 2);
                const auto source = action == "import" ? QFileInfo(p.value("source").toString()) : safeFileInRoots(p.value("file").toString(), { root });
                if (!source.isFile() || source.size() > 32 * 1024 * 1024) return OperationService::failure("Screenshot not found or exceeds 32 MiB.", 2);
                QImageReader reader(source.absoluteFilePath());
                const auto dimensions = reader.size();
                if (!reader.canRead() || !dimensions.isValid() || qint64(dimensions.width()) * dimensions.height() > 32 * 1024 * 1024)
                    return OperationService::failure("Invalid or oversized screenshot.", 2);
                QFile input(source.absoluteFilePath());
                if (!input.open(QIODevice::ReadOnly)) return OperationService::failure(input.errorString());
                if (action == "read") return OperationService::success(QJsonObject{ { "content", QString::fromLatin1(input.readAll().toBase64()) },
                    { "encoding", "base64" }, { "format", QString::fromLatin1(reader.format()) }, { "width", dimensions.width() }, { "height", dimensions.height() } });
                const auto name = p.value("name").toString();
                static const QRegularExpression invalid(R"([<>:"/\\|?*\x00-\x1f])");
                static const QRegularExpression reserved(R"(^(CON|PRN|AUX|NUL|COM[0-9]|LPT[0-9])($|\.))", QRegularExpression::CaseInsensitiveOption);
                if (name.isEmpty() || name == "." || name == ".." || name.endsWith('.') || name.endsWith(' ') || invalid.match(name).hasMatch() || reserved.match(name).hasMatch() ||
                    !QStringList{ "png", "jpg", "jpeg" }.contains(QFileInfo(name).suffix().toLower())) return OperationService::failure("Invalid screenshot filename.", 2);
                const auto destination = QDir(root).filePath(name);
                if (QFileInfo::exists(destination) || QFileInfo(destination).isSymLink() || !QDir().mkpath(root)) return OperationService::failure("Screenshot destination exists or cannot be created.", 2);
                if (action == "rename") {
                    input.close();
                    reader.setDevice(nullptr);
                    if (!QFile::rename(source.absoluteFilePath(), destination)) return OperationService::failure("Could not rename screenshot.");
                } else {
                    QSaveFile output(destination);
                    if (!output.open(QIODevice::WriteOnly)) return OperationService::failure(output.errorString());
                    while (!input.atEnd()) { const auto bytes = input.read(1024 * 1024); if (bytes.isEmpty() || output.write(bytes) != bytes.size()) return OperationService::failure("Could not copy screenshot."); }
                    if (!output.commit()) return OperationService::failure(output.errorString());
                }
                return OperationService::success(QJsonObject{ { "path", destination }, { "name", name } });
            });
    }
    api.registerOperation({ "instance.screenshot.list", "List screenshots in an instance.",
                            objectSchema({ { "instance", stringProperty("Instance ID or name.") } }, { "instance" }) },
                           [](const QJsonObject& parameters, UserInteraction&) { return listScreenshots(parameters); });
    api.registerOperation({ "instance.screenshot.delete", "Delete an instance screenshot.",
                            objectSchema({ { "instance", stringProperty("Instance ID or name.") },
                                           { "file", stringProperty("Screenshot name or path returned by screenshot.list.") },
                                           { "confirm", boolProperty("Confirm deletion.") } },
                                          { "instance", "file", "confirm" }), {}, true },
                           [](const QJsonObject& parameters, UserInteraction&) { return deleteScreenshot(parameters); });
    api.registerOperation({ "instance.screenshot.upload", "Upload an instance screenshot to Imgur.",
                            objectSchema({ { "instance", stringProperty("Instance ID or name.") },
                                           { "file", stringProperty("Screenshot name or path returned by screenshot.list.") } },
                                          { "instance", "file" }) },
                           [&api](const QJsonObject& parameters, UserInteraction& interaction) { return uploadScreenshot(api, parameters, interaction); });
}
