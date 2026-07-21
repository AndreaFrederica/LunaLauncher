// SPDX-License-Identifier: GPL-3.0-only

#include "net/Aria2Download.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QNetworkRequest>

#include "Application.h"
#include "BuildConfig.h"
#include "FileSystem.h"
#include "MMCTime.h"
#include "StringUtils.h"
#include "net/FileSink.h"
#include "net/Logging.h"
#include "net/MetaCacheSink.h"
#include "settings/SettingsObject.h"

namespace {
class Aria2NetworkReply final : public QNetworkReply {
   public:
    explicit Aria2NetworkReply(const QUrl& url)
    {
        setUrl(url);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
        open(QIODevice::ReadOnly);
        setFinished(true);
    }

    void abort() override {}

   protected:
    qint64 readData(char*, qint64) override { return -1; }
};
}  // namespace

namespace Net {

Aria2Download::Aria2Download(QString path) : Download(), m_targetPath(std::move(path))
{
    logCat = taskDownloadLogC;
}

bool Aria2Download::shouldUseFor(const QUrl& url)
{
    if (!APPLICATION_DYN || !Aria2Manager::instance()->isEnabledBySettings()) {
        return false;
    }
    return url.scheme() == "http" || url.scheme() == "https";
}

Download::Ptr Aria2Download::makeFile(QUrl url, QString path, Options options)
{
    auto dl = makeShared<Aria2Download>(path);
    dl->m_url = std::move(url);
    dl->setObjectName(QString("ARIA2_FILE:") + dl->m_url.toString());
    dl->m_options = options;
    dl->m_sink.reset(new FileSink(path));
    return dl;
}

Download::Ptr Aria2Download::makeCached(QUrl url, MetaEntryPtr entry, Options options)
{
    auto dl = makeShared<Aria2Download>(entry->getFullPath());
    dl->m_url = std::move(url);
    dl->setObjectName(QString("ARIA2_CACHE:") + dl->m_url.toString());
    dl->m_options = options;
    auto md5Node = new ChecksumValidator(QCryptographicHash::Md5);
    dl->m_sink.reset(new MetaCacheSink(entry, md5Node, options.testFlag(Option::MakeEternal)));
    return dl;
}

void Aria2Download::executeTask()
{
    m_finished = false;
    m_error = QNetworkReply::NoError;
    m_errorString.clear();

    if (!shouldUseFor(m_url)) {
        Download::executeTask();
        return;
    }

    setStatus(tr("Requesting %1").arg(StringUtils::truncateUrlHumanFriendly(m_url, 80)));

    if (getState() == Task::State::AbortedByUser) {
        emit aborted();
        emit finished();
        return;
    }

    QNetworkRequest request(m_url);
#if defined(LAUNCHER_APPLICATION)
    auto userAgent = APPLICATION->getUserAgent();
#else
    auto userAgent = BuildConfig.USER_AGENT;
#endif
    request.setHeader(QNetworkRequest::UserAgentHeader, userAgent.toUtf8());
    for (auto& headerProxy : m_headerProxies) {
        headerProxy->writeHeaders(request);
    }
    m_request = request;

    QString error;
    auto manager = Aria2Manager::instance();
    if (!manager->ensureStarted(&error)) {
        startQtFallback(error);
        return;
    }

    QFileInfo targetInfo(m_targetPath);
    if (!FS::ensureFilePathExists(m_targetPath)) {
        emitFailed(tr("Could not create folder for %1").arg(m_targetPath));
        return;
    }

    m_tempPath = targetInfo.absoluteFilePath() + ".aria2download";
    QFileInfo tempInfo(m_tempPath);
    auto options = buildAria2Options(request);

    connect(manager, &Aria2Manager::downloadChanged, this, &Aria2Download::aria2DownloadChanged, Qt::UniqueConnection);
    connect(manager, &Aria2Manager::downloadGidChanged, this, &Aria2Download::aria2DownloadGidChanged, Qt::UniqueConnection);
    manager->addDownload(m_url, tempInfo.absolutePath(), tempInfo.fileName(), options, this,
                         [this](bool ok, const QString& gid, const QString& error) {
                             if (!ok) {
                                 startQtFallback(error);
                                 return;
                             }
                             m_gid = gid;
                             setStatus(tr("Downloading with aria2: %1").arg(StringUtils::truncateUrlHumanFriendly(m_url, 80)));
                         });
}

QJsonObject Aria2Download::buildAria2Options(QNetworkRequest& request) const
{
    QJsonObject options;
    QJsonArray headers;
    for (const auto& header : request.rawHeaderList()) {
        headers.append(QString::fromUtf8(header + ": " + request.rawHeader(header)));
    }
    if (!headers.isEmpty()) {
        options.insert("header", headers);
    }

    const auto userAgent = request.header(QNetworkRequest::UserAgentHeader).toByteArray();
    if (!userAgent.isEmpty()) {
        options.insert("user-agent", QString::fromUtf8(userAgent));
    }
    return options;
}

void Aria2Download::startQtFallback(const QString& reason)
{
    qCWarning(taskDownloadLogC) << getUid().toString() << "Falling back to Qt download for" << m_url << "because" << reason;
    if (!Aria2Manager::instance()->fallbackToQtEnabled()) {
        m_error = QNetworkReply::UnknownNetworkError;
        m_errorString = reason;
        emitFailed(reason);
        return;
    }
    disconnect(Aria2Manager::instance(), nullptr, this, nullptr);
    cleanupAria2Files();
    m_gid.clear();
    m_error = QNetworkReply::NoError;
    m_errorString.clear();
    Download::executeTask();
}

void Aria2Download::cleanupAria2Files()
{
    if (m_tempPath.isEmpty()) {
        return;
    }
    QFile::remove(m_tempPath);
    QFile::remove(m_tempPath + ".aria2");
}

void Aria2Download::aria2DownloadChanged(const QString& gid, const Aria2DownloadInfo& info)
{
    if (gid != m_gid || m_finished) {
        return;
    }

    setProgress(info.completedLength, info.totalLength);
    QString progress = tr("%1 / %2")
                           .arg(StringUtils::humanReadableFileSize(info.completedLength))
                           .arg(info.totalLength > 0 ? StringUtils::humanReadableFileSize(info.totalLength) : tr("unknown"));
    QString speed = tr("%1 /s").arg(StringUtils::humanReadableFileSize(info.downloadSpeed));
    setDetails(progress + "\n" + speed);

    if (info.status == "complete") {
        m_finished = true;
        m_statusCode = 200;
        finishFromTempFile();
    } else if (info.status == "error" || info.status == "removed") {
        m_finished = true;
        m_error = info.status == "removed" ? QNetworkReply::OperationCanceledError : QNetworkReply::UnknownNetworkError;
        const QString aria2Error = info.errorMessage.isEmpty() ? tr("aria2 download failed with status %1").arg(info.status)
                                                               : info.errorMessage;
        m_errorString = tr("aria2 download failed for %1 (GID %2, error code %3): %4")
                            .arg(info.url.isEmpty() ? m_url.toString() : info.url,
                                 info.gid.isEmpty() ? gid : info.gid,
                                 info.errorCode.isEmpty() ? tr("unknown") : info.errorCode,
                                 aria2Error);
        qCWarning(taskDownloadLogC) << getUid().toString() << m_errorString;
        if (m_state == State::AbortedByUser) {
            cleanupAria2Files();
            emit aborted();
            emit finished();
        } else if (info.status == "error" && Aria2Manager::instance()->fallbackToQtEnabled()) {
            startQtFallback(m_errorString);
        } else {
            cleanupAria2Files();
            emitFailed(m_errorString);
        }
    }
}

void Aria2Download::aria2DownloadGidChanged(const QString& oldGid, const QString& newGid, const Aria2DownloadInfo& info)
{
    if (oldGid != m_gid || m_finished) {
        return;
    }
    m_gid = newGid;
    aria2DownloadChanged(newGid, info);
}

void Aria2Download::finishFromTempFile()
{
    QFile input(m_tempPath);
    if (!input.open(QIODevice::ReadOnly)) {
        m_error = QNetworkReply::ContentAccessDenied;
        m_errorString = tr("Could not open aria2 output file %1: %2").arg(m_tempPath, input.errorString());
        emitFailed(m_errorString);
        return;
    }

    auto fileSink = dynamic_cast<FileSink*>(m_sink.get());
    Aria2NetworkReply reply(m_url);
    if (!fileSink || fileSink->validateExistingFile(m_request, reply, input) != State::Succeeded) {
        m_error = QNetworkReply::UnknownContentError;
        m_errorString = m_sink->failReason().isEmpty() ? tr("Failed to validate aria2 output file %1").arg(m_tempPath)
                                                       : m_sink->failReason();
        input.close();
        QFile::remove(m_tempPath);
        emitFailed(m_errorString);
        return;
    }
    input.close();

    // Place the file: rename aria2's temp to the target. FS::move uses
    // std::filesystem::rename (overwrite-capable on Windows via
    // MoveFileExW + MOVEFILE_REPLACE_EXISTING) with a copy+delete fallback.
    if (!FS::ensureFilePathExists(m_targetPath)) {
        m_error = QNetworkReply::UnknownContentError;
        m_errorString = tr("Could not create folder for %1").arg(m_targetPath);
        QFile::remove(m_tempPath);
        emitFailed(m_errorString);
        return;
    }
    if (!FS::move(m_tempPath, m_targetPath)) {
        m_error = QNetworkReply::UnknownContentError;
        m_errorString = tr("Could not move aria2 output file from %1 to %2").arg(m_tempPath, m_targetPath);
        QFile::remove(m_tempPath);
        emitFailed(m_errorString);
        return;
    }

    if (fileSink->finalizeExistingFile(reply) != State::Succeeded) {
        m_error = QNetworkReply::UnknownContentError;
        m_errorString = m_sink->failReason();
        emitFailed(m_errorString);
        return;
    }

    emitSucceeded();
}

bool Aria2Download::abort()
{
    m_state = State::AbortedByUser;
    if (!m_gid.isEmpty()) {
        Aria2Manager::instance()->removeDownload(m_gid);
    }
    cleanupAria2Files();
    return true;
}

}  // namespace Net
