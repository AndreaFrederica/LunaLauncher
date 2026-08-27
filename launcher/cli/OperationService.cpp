// SPDX-License-Identifier: GPL-3.0-only

#include "OperationService.h"

#include <QCryptographicHash>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrlQuery>

#include "Application.h"
#include "BaseInstance.h"
#include "FileSystem.h"
#include "InstanceImportTask.h"
#include "InstanceList.h"
#include "LaunchController.h"
#include "minecraft/auth/AccountData.h"
#include "minecraft/auth/AccountList.h"
#include "minecraft/auth/AuthFlow.h"
#include "minecraft/auth/MinecraftAccount.h"
#include "minecraft/launch/MinecraftTarget.h"
#include "modplatform/ModApiMirror.h"
#include "modplatform/flame/CurseForgeDownloadPageService.h"
#include "net/ApiDownload.h"
#include "net/NetJob.h"
#include "net/RawHeaderProxy.h"
#include "net/Upload.h"
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
    if (operation == "instance.list")
        return listInstances();
    if (operation == "account.list")
        return listAccounts();
    if (operation == "account.login")
        return loginAccount(parameters, interaction);
    if (operation == "instance.import")
        return importInstance(parameters, interaction);
    if (operation == "instance.launch")
        return launchInstance(parameters, interaction);
    return failure(tr("Unknown operation: %1").arg(operation), 2);
}

QJsonObject OperationService::listInstances()
{
    QJsonArray instances;
    auto list = APPLICATION->instances();
    for (int i = 0; i < list->count(); ++i) {
        const auto instance = list->at(i);
        instances.append(QJsonObject{ { "id", instance->id() },
                                      { "name", instance->name() },
                                      { "group", list->getInstanceGroup(instance->id()) },
                                      { "running", instance->isRunning() },
                                      { "lastLaunch", instance->lastLaunch() },
                                      { "playTimeSeconds", static_cast<qint64>(instance->totalTimePlayed()) } });
    }
    return success(instances);
}

QJsonObject OperationService::listAccounts()
{
    QJsonArray accounts;
    const auto list = APPLICATION->accounts();
    const auto defaultAccount = list->defaultAccount();
    for (int i = 0; i < list->count(); ++i) {
        const auto account = list->at(i);
        accounts.append(QJsonObject{ { "id", account->profileId() },
                                     { "profileName", account->profileName() },
                                     { "type", accountTypeName(account->accountType()) },
                                     { "state", accountStateName(account->accountState()) },
                                     { "default", account == defaultAccount },
                                     { "ownsMinecraft", account->ownsMinecraft() } });
    }
    return success(accounts);
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
    connect(task, &Task::status, this, [&interaction](const QString& status) { interaction.status(status); });
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
    const auto type = parameters.value("type").toString();
    auto username = parameters.value("username").toString();
    if (username.isEmpty() && type != "microsoft") {
        const auto value = interaction.input(tr("Username"), false);
        if (!value || value->trimmed().isEmpty())
            return failure(tr("A username is required."), 2);
        username = value->trimmed();
    }

    MinecraftAccountPtr account;
    if (type == "offline") {
        account = MinecraftAccount::createOffline(username);
    } else if (type == "microsoft") {
        account = MinecraftAccount::createBlankMSA();
    } else if (type == "yggdrasil" || type == "unified-pass") {
        const auto password = interaction.input(tr("Password"), true);
        if (!password)
            return failure(tr("A password is required; use an interactive terminal or --password-stdin."), 2);
        if (type == "yggdrasil") {
            const auto authUrl = parameters.value("authUrl").toString();
            const auto sessionUrl = parameters.value("sessionUrl").toString();
            if (authUrl.isEmpty() || sessionUrl.isEmpty())
                return failure(tr("Yggdrasil login requires authUrl and sessionUrl."), 2);
            account =
                MinecraftAccount::createYggdrasil(username, *password, authUrl, sessionUrl, parameters.value("sourceName").toString());
        } else {
            const auto serverId = parameters.value("serverId").toString();
            if (serverId.isEmpty())
                return failure(tr("UnifiedPass login requires serverId."), 2);
            account = MinecraftAccount::createUnifiedPass(username, *password, serverId);
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
    return success(QJsonObject{
        { "id", account->profileId() }, { "profileName", account->profileName() }, { "type", accountTypeName(account->accountType()) } });
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
    auto task = APPLICATION->instances()->wrapInstanceTask(importTask);
    QString error;
    if (!waitForTask(task, interaction, &error))
        return failure(error);
    APPLICATION->instances()->saveNow();
    return success(QJsonObject{ { "name", name }, { "source", resolvedUrl.toString() } });
}

QJsonObject OperationService::launchInstance(const QJsonObject& parameters, UserInteraction& interaction)
{
    const auto instanceId = parameters.value("instance").toString();
    auto instance = APPLICATION->instances()->getInstanceById(instanceId);
    if (!instance)
        instance = APPLICATION->instances()->getInstanceByManagedName(instanceId);
    if (!instance) {
        for (int i = 0; i < APPLICATION->instances()->count(); ++i) {
            if (APPLICATION->instances()->at(i)->name().compare(instanceId, Qt::CaseInsensitive) == 0) {
                instance = APPLICATION->instances()->at(i);
                break;
            }
        }
    }
    if (!instance)
        return failure(tr("Instance not found: %1").arg(instanceId), 2);

    MinecraftAccountPtr account;
    const auto profile = parameters.value("profile").toString();
    if (!profile.isEmpty())
        account = APPLICATION->accounts()->getAccountByProfileName(profile);
    else
        account = APPLICATION->accounts()->defaultAccount();

    const auto offlineName = parameters.value("offlineName").toString();
    auto mode = LaunchMode::Normal;
    if (!offlineName.isEmpty()) {
        mode = LaunchMode::Offline;
        account = MinecraftAccount::createOffline(offlineName);
    }
    if (!account)
        return failure(tr("No account is available. Log in or pass an offline player name."), 2);

    MinecraftTarget::Ptr target;
    if (!parameters.value("server").toString().isEmpty())
        target.reset(new MinecraftTarget(MinecraftTarget::parse(parameters.value("server").toString(), false)));
    else if (!parameters.value("world").toString().isEmpty())
        target.reset(new MinecraftTarget(MinecraftTarget::parse(parameters.value("world").toString(), true)));

    auto controller = new LaunchController();
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
    controller->start();
    if (!controller->isFinished() && (!started || parameters.value("wait").toBool()))
        loop.exec();

    const bool wait = parameters.value("wait").toBool();
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
