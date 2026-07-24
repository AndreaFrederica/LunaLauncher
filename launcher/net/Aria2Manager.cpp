// SPDX-License-Identifier: GPL-3.0-only

#include "net/Aria2Manager.h"

#include <QAbstractSocket>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonValue>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTemporaryDir>
#include <QTimer>

#include "Application.h"
#include "FileSystem.h"
#include "archive/ArchiveReader.h"
#include "net/Logging.h"
#include "settings/SettingsObject.h"

namespace Net {

namespace {
constexpr auto ARIA2_WINDOWS_X64_URL =
    "https://github.com/aria2/aria2/releases/download/release-1.37.0/aria2-1.37.0-win-64bit-build1.zip";
constexpr auto ARIA2_WINDOWS_X64_EXE_SHA256 = "be2099c214f63a3cb4954b09a0becd6e2e34660b886d4c898d260febfe9d70c2";

struct Aria2InstallInfo {
    bool supported = false;
    QString url;
    QString executableName;
    QString executableSha256;
    QString unsupportedReason;
};

Aria2InstallInfo installInfoForPlatform()
{
#ifdef Q_OS_WIN
    if (QSysInfo::currentCpuArchitecture() == "x86_64") {
        return { true, ARIA2_WINDOWS_X64_URL, "aria2c.exe", ARIA2_WINDOWS_X64_EXE_SHA256, {} };
    }
    return { false, {}, {}, {}, QObject::tr("Automatic aria2 download is only available for Windows x64.") };
#elif defined(Q_OS_MACOS)
    return { false, {}, {}, {}, QObject::tr("Automatic aria2 download is not available on macOS. Install aria2 with Homebrew, MacPorts, or set a custom aria2c path.") };
#else
    return { false, {}, {}, {}, QObject::tr("Automatic aria2 download is not available on this platform. Install aria2 with your package manager or set a custom aria2c path.") };
#endif
}

QString executableFileName()
{
#ifdef Q_OS_WIN
    return "aria2c.exe";
#else
    return "aria2c";
#endif
}

QString sha256Hex(const QByteArray& data)
{
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

bool isBundledBuildEnvironmentTool(const QString& candidate)
{
    if (!APPLICATION_DYN) {
        return false;
    }
    const auto candidatePath = QDir::cleanPath(QFileInfo(candidate).canonicalFilePath());
    const auto msysPath = QDir::cleanPath(QFileInfo(FS::PathCombine(APPLICATION->root(), ".msys2")).canonicalFilePath());
    return !candidatePath.isEmpty() && !msysPath.isEmpty() &&
           (candidatePath == msysPath || candidatePath.startsWith(msysPath + "/") || candidatePath.startsWith(msysPath + "\\"));
}

class Aria2InstallTask : public Task {
   public:
    Aria2InstallTask(QString targetPath, Aria2InstallInfo info) : Task(true), m_targetPath(std::move(targetPath)), m_info(std::move(info)) {}

   private:
    void executeTask() override
    {
        if (!m_info.supported) {
            emitFailed(m_info.unsupportedReason);
            return;
        }

        setStatus(tr("Downloading aria2"));
        m_tempDir.reset(new QTemporaryDir(QDir::temp().absoluteFilePath("aria2-install-XXXXXX")));
        if (!m_tempDir->isValid()) {
            emitFailed(tr("Failed to create a temporary directory for aria2."));
            return;
        }

        m_archivePath = m_tempDir->filePath("aria2.zip");
        m_archiveFile.setFileName(m_archivePath);
        if (!m_archiveFile.open(QIODevice::WriteOnly)) {
            emitFailed(tr("Failed to create aria2 download file: %1").arg(m_archiveFile.errorString()));
            return;
        }

        QNetworkRequest request(QUrl(m_info.url));
        request.setHeader(QNetworkRequest::UserAgentHeader, APPLICATION->getUserAgent().toUtf8());
        request.setTransferTimeout(APPLICATION->settings()->get("RequestTimeout").toInt() * 1000);
        m_reply = APPLICATION->network()->get(request);
        connect(m_reply, &QNetworkReply::readyRead, this, [this] {
            if (m_archiveFile.write(m_reply->readAll()) < 0) {
                m_writeError = m_archiveFile.errorString();
                m_reply->abort();
            }
        });
        connect(m_reply, &QNetworkReply::downloadProgress, this, &Task::setProgress);
        connect(m_reply, &QNetworkReply::finished, this, [this] { downloadFinished(); });
    }

    bool abort() override
    {
        if (m_reply) {
            m_reply->abort();
            return true;
        }
        return Task::abort();
    }

    void downloadFinished()
    {
        m_archiveFile.write(m_reply->readAll());
        const auto replyError = m_reply->error();
        const auto replyErrorString = m_reply->errorString();
        m_reply->deleteLater();
        m_reply = nullptr;
        m_archiveFile.close();

        if (!m_writeError.isEmpty()) {
            emitFailed(tr("Failed to write aria2 archive: %1").arg(m_writeError));
            return;
        }
        if (replyError != QNetworkReply::NoError) {
            emitFailed(tr("Failed to download aria2: %1").arg(replyErrorString));
            return;
        }

        setStatus(tr("Installing aria2"));
        tryInstallArchive();
    }

    void tryInstallArchive()
    {
        MMCZip::ArchiveReader archive(m_archivePath);
        if (!archive.collectFiles()) {
            emitFailed(tr("Downloaded aria2 archive does not contain %1.").arg(m_info.executableName));
            return;
        }
        QString executableInArchive;
        const auto files = archive.getFiles();
        for (const auto& file : files) {
            if (file.endsWith("/" + m_info.executableName) || file == m_info.executableName) {
                executableInArchive = file;
                break;
            }
        }
        if (executableInArchive.isEmpty()) {
            emitFailed(tr("Downloaded aria2 archive does not contain %1.").arg(m_info.executableName));
            return;
        }

        auto archiveFile = archive.goToFile(executableInArchive);
        if (!archiveFile) {
            emitFailed(tr("Failed to open %1 from the aria2 archive.").arg(executableInArchive));
            return;
        }

        int readStatus = 0;
        const auto executableData = archiveFile->readAll(&readStatus);
        if (readStatus < 0 || executableData.isEmpty()) {
            emitFailed(tr("Failed to read %1 from the aria2 archive.").arg(executableInArchive));
            return;
        }

        const auto actualSha256 = sha256Hex(executableData);
        if (!m_info.executableSha256.isEmpty() && actualSha256 != m_info.executableSha256) {
            emitFailed(tr("Downloaded aria2 executable did not match the expected checksum."));
            return;
        }

        if (!FS::ensureFilePathExists(m_targetPath)) {
            emitFailed(tr("Failed to create aria2 install directory."));
            return;
        }

        QSaveFile output(m_targetPath);
        if (!output.open(QIODevice::WriteOnly)) {
            emitFailed(tr("Failed to open %1 for writing: %2").arg(m_targetPath, output.errorString()));
            return;
        }
        if (output.write(executableData) != executableData.size()) {
            emitFailed(tr("Failed to write %1: %2").arg(m_targetPath, output.errorString()));
            return;
        }
        if (!output.commit()) {
            emitFailed(tr("Failed to install %1: %2").arg(m_targetPath, output.errorString()));
            return;
        }

        QFile::setPermissions(m_targetPath,
                              QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner | QFileDevice::ReadUser |
                                  QFileDevice::ExeUser | QFileDevice::ReadGroup | QFileDevice::ExeGroup | QFileDevice::ReadOther |
                                  QFileDevice::ExeOther);
        emitSucceeded();
    }

    QString m_targetPath;
    Aria2InstallInfo m_info;
    std::unique_ptr<QTemporaryDir> m_tempDir;
    QString m_archivePath;
    QFile m_archiveFile;
    QNetworkReply* m_reply = nullptr;
    QString m_writeError;
};
}  // namespace

Aria2Manager* Aria2Manager::instance()
{
    static Aria2Manager s_instance;
    return &s_instance;
}

Aria2Manager::Aria2Manager(QObject* parent) : QObject(parent)
{
    connect(&m_pollTimer, &QTimer::timeout, this, &Aria2Manager::poll);
    connect(&m_webSocket, &QWebSocket::textMessageReceived, this, &Aria2Manager::handleWebSocketTextMessage);
}

Aria2Manager::~Aria2Manager()
{
    shutdown();
}

bool Aria2Manager::isEnabledBySettings() const
{
    return APPLICATION_DYN && APPLICATION->settings()->get("Aria2Enabled").toBool();
}

bool Aria2Manager::fallbackToQtEnabled() const
{
    return !APPLICATION_DYN || APPLICATION->settings()->get("Aria2FallbackToQt").toBool();
}

bool Aria2Manager::followLauncherDownloadLimits() const
{
    return APPLICATION_DYN && APPLICATION->settings()->get("Aria2FollowLauncherDownloadLimits").toBool();
}

int Aria2Manager::maxConcurrentDownloads() const
{
    if (!APPLICATION_DYN) {
        return 6;
    }
    if (followLauncherDownloadLimits()) {
        return qMax(1, APPLICATION->settings()->get("NumberOfConcurrentDownloads").toInt());
    }
    return qMax(1, APPLICATION->settings()->get("Aria2MaxConcurrentDownloads").toInt());
}

QString Aria2Manager::bundledExecutablePath() const
{
    const QString executable = executableFileName();
    const QDir appDir(QCoreApplication::applicationDirPath());
    QStringList candidates = { appDir.absoluteFilePath(FS::PathCombine("tools", FS::PathCombine("aria2", executable))),
                               appDir.absoluteFilePath(executable) };

#ifdef Q_OS_MACOS
    QDir macResources(appDir);
    if (macResources.cdUp() && macResources.dirName() == "MacOS" && macResources.cdUp() && macResources.cd("Resources")) {
        candidates.prepend(macResources.absoluteFilePath(FS::PathCombine("tools", FS::PathCombine("aria2", executable))));
    }
#endif

    for (const auto& candidate : candidates) {
        QFileInfo info(candidate);
        if (info.exists() && info.isFile() && info.isExecutable()) {
            return info.absoluteFilePath();
        }
    }
    return candidates.first();
}

QString Aria2Manager::managedExecutablePath() const
{
    if (!APPLICATION_DYN) {
        return {};
    }
    return FS::PathCombine(APPLICATION->dataRoot(), "tools", "aria2", executableFileName());
}

QString Aria2Manager::findSystemExecutable() const
{
    const QString executable = executableFileName();
    const QString fromPath = QStandardPaths::findExecutable(executable);
    if (!fromPath.isEmpty() && !isBundledBuildEnvironmentTool(fromPath)) {
        return fromPath;
    }

#if defined(Q_OS_MACOS)
    const QStringList commonPaths = { "/opt/homebrew/bin/aria2c", "/usr/local/bin/aria2c", "/opt/local/bin/aria2c", "/usr/bin/aria2c" };
#elif defined(Q_OS_UNIX)
    const QStringList commonPaths = { "/usr/bin/aria2c", "/usr/local/bin/aria2c", "/app/bin/aria2c", "/snap/bin/aria2c" };
#else
    const QStringList commonPaths;
#endif
    for (const auto& candidate : commonPaths) {
        QFileInfo info(candidate);
        if (info.exists() && info.isFile() && info.isExecutable() && !isBundledBuildEnvironmentTool(candidate)) {
            return info.absoluteFilePath();
        }
    }

    return {};
}

QString Aria2Manager::findExecutable() const
{
    if (APPLICATION_DYN) {
        QString configured = APPLICATION->settings()->get("Aria2ExecutablePath").toString();
        if (!configured.isEmpty()) {
            QFileInfo info(configured);
            if (info.exists() && info.isFile() && info.isExecutable()) {
                return info.absoluteFilePath();
            }
        }
    }

    const auto system = findSystemExecutable();
    if (!system.isEmpty()) {
        return system;
    }

    QFileInfo managed(managedExecutablePath());
    if (managed.exists() && managed.isFile() && managed.isExecutable()) {
        return managed.absoluteFilePath();
    }

    QFileInfo bundled(bundledExecutablePath());
    if (bundled.exists() && bundled.isFile() && bundled.isExecutable()) {
        return bundled.absoluteFilePath();
    }

    return {};
}

bool Aria2Manager::canInstallManagedExecutable(QString* reason) const
{
    const auto info = installInfoForPlatform();
    if (!info.supported && reason) {
        *reason = info.unsupportedReason;
    }
    return info.supported;
}

Task::Ptr Aria2Manager::createInstallTask() const
{
    return makeShared<Aria2InstallTask>(managedExecutablePath(), installInfoForPlatform());
}

bool Aria2Manager::removeManagedExecutable(QString* reason)
{
    if (isRunning()) {
        if (reason) {
            *reason = tr("Stop aria2 before removing the downloaded executable.");
        }
        return false;
    }

    const auto path = managedExecutablePath();
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        return true;
    }
    if (!QFile::remove(path)) {
        if (reason) {
            *reason = tr("Failed to remove %1.").arg(path);
        }
        return false;
    }
    return true;
}

bool Aria2Manager::isRunning() const
{
    return m_process && m_process->state() != QProcess::NotRunning;
}

QString Aria2Manager::statusText() const
{
    return m_status;
}

bool Aria2Manager::ensureStarted(QString* error)
{
    if (isRunning()) {
        connectWebSocket();
        return true;
    }

    const QString executable = findExecutable();
    if (executable.isEmpty()) {
        if (error) {
            *error = tr("aria2c was not found.");
        }
        m_status = tr("Not found");
        emit statusChanged();
        return false;
    }

    if (m_process) {
        m_webSocket.close();
        m_process->deleteLater();
    }

    m_secret =
        QString::number(QRandomGenerator::global()->generate64(), 16) + QString::number(QRandomGenerator::global()->generate64(), 16);
    m_port = APPLICATION->settings()->get("Aria2RpcPort").toInt();
    if (m_port <= 0) {
        m_port = 20000 + int(QRandomGenerator::global()->bounded(20000));
    }

    QStringList args;
    args << "--enable-rpc=true";
    args << "--rpc-listen-all=false";
    args << QString("--rpc-listen-port=%1").arg(m_port);
    args << QString("--rpc-secret=%1").arg(m_secret);
    args << "--rpc-allow-origin-all=false";
    args << "--quiet=true";
    args << "--console-log-level=warn";
    args << "--summary-interval=0";
    args << "--auto-file-renaming=false";
    args << "--allow-overwrite=true";
    args << "--no-want-digest-header=true";
    args << QString("--max-concurrent-downloads=%1").arg(maxConcurrentDownloads());

    const auto proxy = proxyOptions();
    for (auto it = proxy.constBegin(); it != proxy.constEnd(); ++it) {
        args << QString("--%1=%2").arg(it.key(), it.value().toString());
    }

    args << splitExtraArgs(APPLICATION->settings()->get("Aria2ExtraArgs").toString());

    m_process = new QProcess(this);
    connect(m_process, &QProcess::finished, this, &Aria2Manager::processFinished);
    m_process->setProgram(executable);
    m_process->setArguments(args);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    m_process->start();

    if (!m_process->waitForStarted(3000)) {
        const QString reason = m_process->errorString();
        if (error) {
            *error = reason;
        }
        m_status = tr("Failed to start: %1").arg(reason);
        emit statusChanged();
        return false;
    }

    m_status = tr("Running on 127.0.0.1:%1").arg(m_port);
    emit statusChanged();
    connectWebSocket();
    m_pollTimer.start(qMax(250, APPLICATION->settings()->get("Aria2PollInterval").toInt()));
    return true;
}

void Aria2Manager::connectWebSocket()
{
    if (!isRunning() || m_port <= 0 || m_webSocket.state() != QAbstractSocket::UnconnectedState) {
        return;
    }
    m_webSocket.open(QUrl(QString("ws://127.0.0.1:%1/jsonrpc").arg(m_port)));
}

void Aria2Manager::handleWebSocketTextMessage(const QString& message)
{
    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(message.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        qCWarning(taskDownloadLogC) << "Failed to parse aria2 websocket message:" << parseError.errorString();
        return;
    }

    const auto object = doc.object();
    const QString method = object.value("method").toString();
    if (!method.startsWith("aria2.onDownload") && method != "aria2.onBtDownloadComplete") {
        return;
    }

    const auto params = object.value("params").toArray();
    if (params.isEmpty()) {
        return;
    }

    const QString gid = params.first().toObject().value("gid").toString();
    if (!gid.isEmpty()) {
        refreshDownload(gid);
    }
}

QStringList Aria2Manager::splitExtraArgs(const QString& raw) const
{
    QStringList out;
    static const QRegularExpression lineEnd("[\\r\\n]+");
    for (const QString& line : raw.split(lineEnd, Qt::SkipEmptyParts)) {
        const QString trimmed = line.trimmed();
        if (!trimmed.isEmpty()) {
            out << trimmed;
        }
    }
    return out;
}

void Aria2Manager::processFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_pollTimer.stop();
    m_webSocket.close();

    const QString reason = exitStatus == QProcess::CrashExit ? tr("aria2 crashed") : tr("aria2 stopped");
    if (!m_shutdownRequested && !m_activeGids.isEmpty()) {
        restartActiveDownloads(tr("%1 (exit code %2)").arg(reason).arg(exitCode));
        return;
    }

    failActiveDownloads(tr("%1 (exit code %2)").arg(reason).arg(exitCode));
    m_activeGids.clear();
    m_status = tr("Stopped (exit code %1)").arg(exitCode);
    m_shutdownRequested = false;
    m_restartAttempts = 0;
    emit statusChanged();
    emit downloadsChanged();
}

void Aria2Manager::shutdown()
{
    if (!isRunning()) {
        return;
    }
    m_shutdownRequested = true;
    m_pollTimer.stop();
    m_webSocket.close();
    m_process->terminate();
    if (!m_process->waitForFinished(3000)) {
        m_process->kill();
        m_process->waitForFinished(1000);
    }
}

void Aria2Manager::restartActiveDownloads(const QString& reason)
{
    constexpr int MAX_RESTART_ATTEMPTS = 3;
    if (m_restartAttempts >= MAX_RESTART_ATTEMPTS) {
        failActiveDownloads(tr("%1. Restart limit reached.").arg(reason));
        m_activeGids.clear();
        m_status = tr("Stopped");
        emit statusChanged();
        emit downloadsChanged();
        return;
    }

    m_restartAttempts++;
    m_status = tr("%1. Restarting aria2...").arg(reason);
    emit statusChanged();

    QString error;
    if (!ensureStarted(&error)) {
        failActiveDownloads(tr("%1. Restart failed: %2").arg(reason, error));
        m_activeGids.clear();
        emit downloadsChanged();
        return;
    }

    const auto restartingGids = m_activeGids.values();
    for (const auto& gid : restartingGids) {
        auto info = m_downloads.value(gid);
        info.status = "restarting";
        info.errorMessage.clear();
        m_downloads.insert(gid, info);
        emit downloadChanged(gid, info);

        if (m_requests.contains(gid)) {
            readdDownload(gid, m_requests.value(gid));
        } else {
            info.status = "error";
            info.errorMessage = tr("Cannot restart aria2 download: missing request data.");
            m_downloads.insert(gid, info);
            m_activeGids.remove(gid);
            emit downloadChanged(gid, info);
        }
    }
    emit downloadsChanged();
}

void Aria2Manager::readdDownload(const QString& oldGid, const AddRequest& request)
{
    QJsonObject options = downloadOptions(request.extraOptions);
    options.insert("continue", "true");
    options.insert("dir", QDir::toNativeSeparators(request.dir));
    options.insert("out", request.out);

    QJsonArray params;
    params << "token:" + m_secret;
    params << QJsonArray{ request.url.toString() };
    params << options;

    rpc("aria2.addUri", params, this, [this, oldGid, request](bool ok, const QJsonValue& result, const QString& error) {
        if (!m_activeGids.contains(oldGid)) {
            return;
        }
        if (!ok) {
            auto info = m_downloads.value(oldGid);
            info.status = "error";
            info.errorMessage = tr("Failed to restart aria2 download: %1").arg(error);
            m_downloads.insert(oldGid, info);
            m_activeGids.remove(oldGid);
            m_requests.remove(oldGid);
            emit downloadChanged(oldGid, info);
            emit downloadsChanged();
            return;
        }

        const QString newGid = result.toString();
        auto info = m_downloads.take(oldGid);
        info.gid = newGid;
        info.status = "waiting";
        info.errorCode.clear();
        info.errorMessage.clear();
        m_downloads.insert(newGid, info);

        m_activeGids.remove(oldGid);
        m_activeGids.insert(newGid);
        m_requests.remove(oldGid);
        m_requests.insert(newGid, request);

        emit downloadGidChanged(oldGid, newGid, info);
        emit downloadChanged(newGid, info);
        emit downloadsChanged();
        refreshDownload(newGid);
    });
}

void Aria2Manager::failActiveDownloads(const QString& reason)
{
    const auto activeGids = m_activeGids.values();
    for (const auto& gid : activeGids) {
        auto info = m_downloads.value(gid);
        info.status = "error";
        info.errorMessage = reason;
        m_downloads.insert(gid, info);
        m_requests.remove(gid);
        emit downloadChanged(gid, info);
    }
}

QJsonObject Aria2Manager::proxyOptions() const
{
    QJsonObject options;
    if (!APPLICATION_DYN) {
        return options;
    }

    auto s = APPLICATION->settings();
    QString type;
    QString host;
    QString user;
    QString pass;
    int port = 0;

    if (s->get("Aria2UseLauncherProxy").toBool()) {
        type = s->get("ProxyType").toString();
        host = s->get("ProxyAddr").toString();
        port = s->get("ProxyPort").toInt();
        user = s->get("ProxyUser").toString();
        pass = s->get("ProxyPass").toString();
    } else {
        type = s->get("Aria2ProxyType").toString();
        host = s->get("Aria2ProxyAddr").toString();
        port = s->get("Aria2ProxyPort").toInt();
        user = s->get("Aria2ProxyUser").toString();
        pass = s->get("Aria2ProxyPass").toString();
    }

    const QString uri = makeProxyUri(type, host, port, user, pass);
    if (!uri.isEmpty()) {
        options.insert("all-proxy", uri);
    }
    return options;
}

QString Aria2Manager::makeProxyUri(const QString& type, const QString& host, int port, const QString& user, const QString& pass) const
{
    if (type == "None" || type == "Default" || host.isEmpty() || port <= 0) {
        return {};
    }

    QUrl url;
    if (type == "SOCKS5") {
        url.setScheme("socks5");
    } else if (type == "HTTP") {
        url.setScheme("http");
    } else {
        return {};
    }
    url.setHost(host);
    url.setPort(port);
    if (!user.isEmpty()) {
        url.setUserName(user);
        url.setPassword(pass);
    }
    return url.toString(QUrl::FullyEncoded);
}

QJsonObject Aria2Manager::downloadOptions(const QJsonObject& extraOptions) const
{
    auto s = APPLICATION->settings();
    QJsonObject options = proxyOptions();
    options.insert("continue", s->get("Aria2Continue").toBool() ? "true" : "false");
    options.insert("max-connection-per-server", QString::number(s->get("Aria2MaxConnectionPerServer").toInt()));
    options.insert("split", QString::number(s->get("Aria2Split").toInt()));
    options.insert("min-split-size", s->get("Aria2MinSplitSize").toString());
    options.insert("file-allocation", s->get("Aria2FileAllocation").toString());
    options.insert("auto-file-renaming", "false");
    options.insert("allow-overwrite", "true");

    for (auto it = extraOptions.constBegin(); it != extraOptions.constEnd(); ++it) {
        options.insert(it.key(), it.value());
    }
    return options;
}

void Aria2Manager::addDownload(const QUrl& url,
                               const QString& dir,
                               const QString& out,
                               const QJsonObject& extraOptions,
                               QObject* context,
                               AddCallback callback)
{
    QString error;
    if (!ensureStarted(&error)) {
        callback(false, {}, error);
        return;
    }

    QJsonObject options = downloadOptions(extraOptions);
    options.insert("dir", QDir::toNativeSeparators(dir));
    options.insert("out", out);

    QJsonArray params;
    params << "token:" + m_secret;
    params << QJsonArray{ url.toString() };
    params << options;

    QPointer<QObject> guard(context);
    rpc("aria2.addUri", params, this,
        [this, guard, url, dir, out, extraOptions, callback = std::move(callback)](bool ok, const QJsonValue& result,
                                                                                  const QString& error) {
            if (!ok) {
                if (guard) {
                    callback(false, {}, error);
                }
                return;
            }
            const QString gid = result.toString();
            Aria2DownloadInfo info;
            info.gid = gid;
            info.url = url.toString();
            info.path = QDir(dir).absoluteFilePath(out);
            info.status = "waiting";
            m_downloads.insert(gid, info);
            m_requests.insert(gid, AddRequest{ url, dir, out, extraOptions });
            m_activeGids.insert(gid);
            emit downloadsChanged();
            if (!guard) {
                removeDownload(gid);
                return;
            }
            callback(true, gid, {});
            poll();
        });
}

void Aria2Manager::removeDownload(const QString& gid)
{
    if (gid.isEmpty() || !isRunning()) {
        return;
    }
    rpc("aria2.forceRemove", QJsonArray{ "token:" + m_secret, gid }, this, [this, gid](bool, const QJsonValue&, const QString&) {
        if (m_downloads.contains(gid)) {
            auto info = m_downloads.value(gid);
            info.status = "removed";
            m_downloads.insert(gid, info);
        }
        m_activeGids.remove(gid);
        m_requests.remove(gid);
        emit downloadsChanged();
        emit downloadChanged(gid, m_downloads.value(gid));
    });
}

void Aria2Manager::rpc(const QString& method, const QJsonArray& params, QObject* context, RpcCallback callback)
{
    QJsonObject payload;
    payload.insert("jsonrpc", "2.0");
    payload.insert("id", QString::number(QRandomGenerator::global()->generate64(), 16));
    payload.insert("method", method);
    payload.insert("params", params);

    QNetworkRequest request(QUrl(QString("http://127.0.0.1:%1/jsonrpc").arg(m_port)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    auto reply = m_network.post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    QPointer<QObject> guard(context);
    connect(reply, &QNetworkReply::finished, this, [reply, guard, callback = std::move(callback)]() {
        reply->deleteLater();
        if (!guard) {
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            callback(false, {}, reply->errorString());
            return;
        }
        QJsonParseError parseError;
        const auto doc = QJsonDocument::fromJson(reply->readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            callback(false, {}, parseError.errorString());
            return;
        }
        const auto object = doc.object();
        if (object.contains("error")) {
            const auto error = object.value("error").toObject();
            callback(false, {}, error.value("message").toString(QObject::tr("aria2 RPC error")));
            return;
        }
        callback(true, object.value("result"), {});
    });
}

void Aria2Manager::poll()
{
    if (!isRunning() || m_activeGids.isEmpty()) {
        return;
    }

    const auto gids = m_activeGids.values();
    for (const QString& gid : gids) {
        refreshDownload(gid);
    }
}

void Aria2Manager::refreshDownload(const QString& gid)
{
    if (!isRunning() || gid.isEmpty()) {
        return;
    }

    QJsonArray keys{ "gid", "status", "totalLength", "completedLength", "downloadSpeed", "errorCode", "errorMessage", "files" };
    rpc("aria2.tellStatus", QJsonArray{ "token:" + m_secret, gid, keys }, this,
        [this, gid](bool ok, const QJsonValue& result, const QString& error) {
            if (!ok) {
                if (!m_activeGids.contains(gid)) {
                    return;
                }
                auto info = m_downloads.value(gid);
                info.status = "error";
                info.errorMessage = error;
                m_downloads.insert(gid, info);
                m_activeGids.remove(gid);
                emit downloadChanged(gid, info);
                emit downloadsChanged();
                return;
            }
            updateDownloadFromJson(gid, result.toObject());
        });
}

void Aria2Manager::updateDownloadFromJson(const QString& gid, const QJsonObject& object)
{
    auto info = m_downloads.value(gid);
    info.gid = gid;
    info.status = object.value("status").toString(info.status);
    info.totalLength = object.value("totalLength").toString().toLongLong();
    info.completedLength = object.value("completedLength").toString().toLongLong();
    info.downloadSpeed = object.value("downloadSpeed").toString().toInt();
    info.errorCode = object.value("errorCode").toString();
    info.errorMessage = object.value("errorMessage").toString();

    const auto files = object.value("files").toArray();
    if (!files.isEmpty()) {
        const auto file = files.first().toObject();
        info.path = file.value("path").toString(info.path);
        const auto uris = file.value("uris").toArray();
        if (!uris.isEmpty()) {
            info.url = uris.first().toObject().value("uri").toString(info.url);
        }
    }

    m_downloads.insert(gid, info);
    if (info.status == "complete" || info.status == "error" || info.status == "removed") {
        m_activeGids.remove(gid);
        m_requests.remove(gid);
    }
    emit downloadChanged(gid, info);
    emit downloadsChanged();
}

QList<Aria2DownloadInfo> Aria2Manager::downloads() const
{
    return m_downloads.values();
}

void Aria2Manager::clearFinished()
{
    for (auto it = m_downloads.begin(); it != m_downloads.end();) {
        if (!m_activeGids.contains(it.key())) {
            m_requests.remove(it.key());
            it = m_downloads.erase(it);
        } else {
            ++it;
        }
    }
    emit downloadsChanged();
}

}  // namespace Net
