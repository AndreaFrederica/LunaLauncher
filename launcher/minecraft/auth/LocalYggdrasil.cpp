// SPDX-License-Identifier: GPL-3.0-only
#include "LocalYggdrasil.h"
#include "MinecraftUuid.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkProxyFactory>
#include <QNetworkProxyQuery>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QScopedValueRollback>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include "AccountData.h"
#include "Application.h"
#include "AuthlibInjector.h"
#include "AuthlibInjectorDownloadTask.h"
#include "BuildConfig.h"
#include "settings/SettingsObject.h"
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

LocalYggdrasil& LocalYggdrasil::instance()
{
    static auto* service = new LocalYggdrasil(QCoreApplication::instance());
    return *service;
}

LocalYggdrasil::LocalYggdrasil(QObject* parent) : QObject(parent)
{
    m_cacheNetwork = makeShared<QNetworkAccessManager>();
    m_cacheNetwork->setProxy(QNetworkProxy::NoProxy);
#ifdef Q_OS_WIN
    m_process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) { args->flags |= CREATE_NO_WINDOW; });
#endif
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, &LocalYggdrasil::stop);
    // Do not log stdout: the single ready message contains the management token.
    connect(&m_process, &QProcess::readyReadStandardError, this, [this] { m_process.readAllStandardError(); });
}

void LocalYggdrasil::stop()
{
    m_process.closeWriteChannel();
    if (!m_process.waitForFinished(3000)) {
        m_process.kill();
        m_process.waitForFinished(1000);
    }
    m_managementToken.clear();
    m_url.clear();
    m_cacheConfiguration.clear();
}

bool LocalYggdrasil::ensureRunning(QString& error)
{
    if (m_process.state() == QProcess::Running && !m_url.isEmpty())
        return true;
    if (m_starting) {
        error = tr("The enhanced offline service is starting. Please try again.");
        return false;
    }
    QScopedValueRollback<bool> starting(m_starting, true);
    m_url.clear();
    m_managementToken.clear();
    m_cacheConfiguration.clear();
    auto executable = QDir(QCoreApplication::applicationDirPath())
                          .filePath(
                              "yggdrasil-server"
#ifdef Q_OS_WIN
                              ".exe"
#endif
                          );
    if (!QFileInfo::exists(executable)) {
        error = tr("Enhanced offline service is not installed. Install yggdrasil-server beside the launcher.");
        return false;
    }
    m_process.setProgram(executable);
    // An ephemeral loopback port allows separate portable launcher installations.
    // Account URLs are updated before every local authentication/refresh.
    m_process.setArguments(
        { "--launcher", "--listen", "127.0.0.1:0", "--data-dir", QDir(APPLICATION->dataRoot()).absoluteFilePath("yggdrasil") });
    m_process.start();
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    auto ready = connect(&m_process, &QProcess::readyReadStandardOutput, &loop, [&] {
        if (m_process.canReadLine())
            loop.quit();
    });
    auto failed = connect(&m_process, &QProcess::errorOccurred, &loop, &QEventLoop::quit);
    auto exited = connect(&m_process, &QProcess::finished, &loop, &QEventLoop::quit);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(30000);
    if (!m_process.canReadLine())
        loop.exec(QEventLoop::ExcludeUserInputEvents);
    disconnect(ready);
    disconnect(failed);
    disconnect(exited);
    const auto response = QJsonDocument::fromJson(m_process.readLine()).object();
    const QUrl url(response.value("url").toString());
    const auto token = response.value("managementToken").toString().toLatin1();
    if (!response.value("ready").toBool() || url.scheme() != "http" || url.host() != "127.0.0.1" || token.size() != 64) {
        error = tr("Could not start the enhanced offline service. Check its data directory and installation.");
        stop();
        return false;
    }
    m_url = url.toString();
    m_managementToken = token;
    return true;
}

bool LocalYggdrasil::request(const QByteArray& method,
                             const QString& path,
                             const QByteArray& body,
                             QJsonObject& result,
                             QString& error,
                             const QByteArray& contentType)
{
    if (!ensureRunning(error))
        return false;
    QNetworkAccessManager network;
    network.setProxy(QNetworkProxy::NoProxy);
    QNetworkRequest request(QUrl(m_url + "/api/v1/" + path));
    request.setRawHeader("Authorization", "Bearer " + m_managementToken);
    request.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(15000);
    auto* reply = network.sendCustomRequest(request, method, body);
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec(QEventLoop::ExcludeUserInputEvents);
    result = QJsonDocument::fromJson(reply->readAll()).object();
    const bool success = reply->error() == QNetworkReply::NoError;
    if (!success) {
        error = result.value("error").toObject().value("message").toString();
        if (error.isEmpty())
            error = reply->errorString();
    }
    return success;
}

bool LocalYggdrasil::createProfile(const QString& name, const QString& password, const QString& uuid, QJsonObject& profile, QString& error)
{
    QJsonObject user;
    if (!request("POST", "users", QJsonDocument(QJsonObject{ { "username", name }, { "password", password } }).toJson(), user, error))
        return false;
    QJsonObject scheme{ { "kind", "offline_player" } };
    if (!uuid.isEmpty())
        scheme = { { "kind", "fixed" }, { "uuid", parseMinecraftUuid(uuid).toString(QUuid::WithoutBraces) } };
    const QJsonObject data{ { "user_id", user.value("id") }, { "name", name }, { "uuid_scheme", scheme } };
    if (request("POST", "profiles", QJsonDocument(data).toJson(), profile, error))
        return true;
    // Undo the new empty user when profile creation fails (for example duplicate UUID).
    QJsonObject ignored;
    QString ignoredError;
    request("DELETE", "users/" + user.value("id").toString(), {}, ignored, ignoredError);
    return false;
}

namespace {
QJsonObject cacheTransport(const QUrl& destination)
{
    auto proxies = QNetworkProxyFactory::proxyForQuery(QNetworkProxyQuery(destination));
    for (const auto& proxy : proxies) {
        if (proxy.type() == QNetworkProxy::NoProxy)
            break;
        const bool socks = proxy.type() == QNetworkProxy::Socks5Proxy;
        if (!socks && proxy.type() != QNetworkProxy::HttpProxy && proxy.type() != QNetworkProxy::HttpCachingProxy)
            continue;
        QUrl url;
        url.setScheme(socks ? "socks5h" : "http");
        url.setHost(proxy.hostName());
        url.setPort(proxy.port());
        if (!proxy.user().isEmpty())
            url.setUserName(proxy.user());
        if (!proxy.password().isEmpty())
            url.setPassword(proxy.password());
        return { { "kind", socks ? "socks5h" : "http_proxy" }, { "proxy_url", url.toString(QUrl::FullyEncoded) } };
    }
    return { { "kind", "direct" } };
}
}  // namespace

bool LocalYggdrasil::ensureCacheService()
{
    if (!APPLICATION->settings()->get("LocalServiceCache").toBool() || QDateTime::currentMSecsSinceEpoch() < m_cacheRetryAfter)
        return false;
    if (m_starting)
        return false;
    QString error;
    if (!ensureRunning(error)) {
        m_cacheRetryAfter = QDateTime::currentMSecsSinceEpoch() + 60000;
        return false;
    }
    return true;
}

bool LocalYggdrasil::ensureAccountCacheService()
{
    auto* settings = APPLICATION->settings();
    const bool enabled = settings->get("LocalServiceCache").toBool() || settings->get("YggdrasilAccountCache").toBool();
    if (!enabled)
        return false;
    QString error;
    return ensureRunning(error);
}

bool LocalYggdrasil::configureCache()
{
    if (!ensureCacheService())
        return false;
    auto source = APPLICATION->settings()->get("MetaURLOverride").toString();
    if (source.isEmpty())
        source = BuildConfig.META_URL;
    const QUrl root(source);
    // Non-HTTP metadata overrides retain their existing behavior.
    if ((root.scheme() != "https" && root.scheme() != "http") || !root.userInfo().isEmpty() || root.hasQuery() || root.hasFragment())
        return false;
    const auto configuration =
        QJsonDocument(QJsonObject{ { "metadata_url", source },
                                   { "transport", cacheTransport(root) },
                                   { "textures_transport", cacheTransport(QUrl("https://textures.minecraft.net/")) } })
            .toJson(QJsonDocument::Compact);
    QString error;
    if (configuration == m_cacheConfiguration)
        return true;
    QJsonObject result;
    if (!request("PUT", "launcher/cache", configuration, result, error)) {
        qWarning() << "Local resource cache configuration failed:" << error;
        m_cacheRetryAfter = QDateTime::currentMSecsSinceEpoch() + 60000;
        return false;
    }
    m_metadataPath = result.value("metadata_path").toString();
    m_texturesPath = result.value("textures_path").toString();
    if (!m_metadataPath.startsWith("/cache/") || !m_texturesPath.startsWith("/cache/"))
        return false;
    m_cacheConfiguration = configuration;
    return true;
}

QUrl LocalYggdrasil::metadataUrl(const QUrl& source)
{
    if (!configureCache())
        return source;
    auto base = APPLICATION->settings()->get("MetaURLOverride").toString();
    if (base.isEmpty())
        base = BuildConfig.META_URL;
    auto root = QUrl(base).resolved(QUrl("."));
    if (!root.isParentOf(source))
        return source;
    QUrl local(m_url + m_metadataPath);
    local.setPath(local.path() + source.path().mid(root.path().size()));
    local.setQuery(source.query());
    return local;
}

QUrl LocalYggdrasil::textureUrl(const QUrl& source)
{
    if (source.host() != "textures.minecraft.net" || (source.scheme() != "http" && source.scheme() != "https") || source.port() != -1 ||
        !source.userInfo().isEmpty() || source.hasQuery() || source.hasFragment() || !configureCache())
        return source;
    QUrl local(m_url + m_texturesPath);
    local.setPath(local.path() + source.path().mid(1));
    return local;
}

bool LocalYggdrasil::isCacheUrl(const QUrl& url) const
{
    const QUrl root(m_url);
    return !m_url.isEmpty() && url.scheme() == root.scheme() && url.host() == root.host() && url.port() == root.port() &&
           url.path().startsWith("/cache/");
}

void LocalYggdrasil::saveAccountSnapshot(AccountData& data)
{
    const auto snapshot = data.cachedSnapshot();
    if (snapshot.isEmpty() || !ensureAccountCacheService())
        return;
    QJsonObject result;
    QString error;
    if (!request("PUT", "account-snapshots/" + data.profileId(), QJsonDocument(snapshot).toJson(), result, error))
        qWarning() << "Could not save the local account snapshot:" << error;
    else
        data.cacheState = CacheState::Cached;
    if (!AuthlibInjector::instance().checkCache() && !m_agentPreparation) {
        auto* task = new AuthlibInjectorDownloadTask(nullptr);
        task->setParent(this);
        m_agentPreparation = task;
        connect(task, &Task::finished, task, &QObject::deleteLater);
        connect(task, &Task::failed, this,
                [](const QString& reason) { qWarning() << "Local authentication preparation failed:" << reason; });
        task->start();
    }
}

void LocalYggdrasil::restoreAccountSnapshot(AccountData& data)
{
    if ((data.type != AccountType::MSA && data.type != AccountType::Yggdrasil) || data.profileId().isEmpty() || !ensureAccountCacheService())
        return;
    QJsonObject result;
    QString error;
    if (request("GET", "account-snapshots/" + data.profileId(), {}, result, error))
        data.restoreCachedSnapshot(result);
}

void LocalYggdrasil::removeAccountSnapshot(const QString& uuid)
{
    // Deletion also works when caching has since been disabled.
    QString error;
    QJsonObject result;
    if (!uuid.isEmpty() && ensureRunning(error))
        request("DELETE", "account-snapshots/" + uuid, {}, result, error);
}

bool LocalYggdrasil::hasAccountSnapshot(AccountData& data)
{
    data.cacheState = CacheState::NotCached;
    if ((data.type != AccountType::MSA && data.type != AccountType::Yggdrasil) ||
        data.profileId().isEmpty() ||
        (data.type == AccountType::MSA && (!APPLICATION->settings()->get("LocalServiceCache").toBool() ||
                                           !data.officialVerifiedAt.isValid() || !data.minecraftEntitlement.ownsMinecraft)) ||
        (data.type == AccountType::Yggdrasil && !APPLICATION->settings()->get("YggdrasilAccountCache").toBool()) || !ensureAccountCacheService())
        return false;
    QJsonObject snapshot;
    QString error;
    const bool cached = request("GET", "account-snapshots/" + data.profileId(), {}, snapshot, error) &&
        snapshot.value("profile").toObject().value("name").toString() == data.profileName() &&
        (data.type != AccountType::Yggdrasil || snapshot.value("local_auth").toBool(false));
    data.cacheState = cached ? CacheState::Cached : CacheState::NotCached;
    return cached;
}

void LocalYggdrasil::refreshAccountSnapshotStatus(AccountData& data)
{
    if (data.type == AccountType::MSA || data.type == AccountType::Yggdrasil)
        hasAccountSnapshot(data);
}

bool LocalYggdrasil::createAccountSession(AccountData& data, AuthSessionPtr session, QString& error)
{
    if (!hasAccountSnapshot(data)) {
        error = tr("No verified local snapshot is available for this account. Sign in online once with local caching enabled.");
        return false;
    }
    if (!AuthlibInjector::instance().checkCache()) {
        error = tr("Local authentication needs authlib-injector. Connect to the Internet and let account preparation finish first.");
        return false;
    }
    QJsonObject result;
    if (!request("POST", "account-snapshots/" + data.profileId() + "/session", "{}", result, error))
        return false;
    if (!session->useLocalYggdrasil(result, data.profileId(), apiUrl())) {
        error = tr("The local authentication service returned an invalid session.");
        return false;
    }
    return true;
}
