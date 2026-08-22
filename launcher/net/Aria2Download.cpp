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
// 1 week in seconds — mirrors MetaCacheSink's private MAX_TIME_TO_EXPIRE.
constexpr qint64 kAria2DefaultCacheMaxAge = 1 * 7 * 24 * 60 * 60;
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
    dl->m_cacheEntry = entry;
    dl->m_isEternal = options.testFlag(Option::MakeEternal);
    auto md5Node = new ChecksumValidator(QCryptographicHash::Md5);
    dl->m_sink.reset(new MetaCacheSink(entry, md5Node, dl->m_isEternal));
    return dl;
}

void Aria2Download::executeTask()
{
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
    Download::executeTask();
}

void Aria2Download::aria2DownloadChanged(const QString& gid, const Aria2DownloadInfo& info)
{
    if (gid != m_gid || m_finished) {
        return;
    }

    setProgress(info.completedLength, info.totalLength);
    setTransferRate(info.downloadSpeed);
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
        m_errorString = info.errorMessage.isEmpty() ? tr("aria2 download failed with status %1").arg(info.status) : info.errorMessage;
        m_sink->abort();
        QFile::remove(m_tempPath);
        if (m_state == State::AbortedByUser) {
            emit aborted();
            emit finished();
        } else {
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

    // Stream the temp file once: feed 256KB chunks into the MD5 hash while
    // reporting progress. aria2 has already written the complete file, so we
    // only hash it here — we do NOT push it back through the sink, which would
    // trigger a second full-disk write and a PSaveFile::commit() that fails on
    // Windows when antivirus locks the just-finished large file.
    const qint64 total = input.size();
    setProgress(0, total);

    QCryptographicHash md5(QCryptographicHash::Md5);
    qint64 processed = 0;
    while (!input.atEnd()) {
        if (m_state == State::AbortedByUser) {
            input.close();
            QFile::remove(m_tempPath);
            emit aborted();
            emit finished();
            return;
        }
        QByteArray chunk = input.read(256 * 1024);
        if (chunk.isEmpty() && input.error() != QFile::NoError) {
            m_error = QNetworkReply::UnknownContentError;
            m_errorString = input.errorString();
            input.close();
            QFile::remove(m_tempPath);
            emitFailed(m_errorString);
            return;
        }
        md5.addData(chunk);
        processed += chunk.size();
        setProgress(processed, total);
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

    // For the cached (MetaCacheSink) case only: update the cache metadata
    // manually, mirroring MetaCacheSink::finalizeCache. aria2 gives us no HTTP
    // headers, so ETag and Last-Modified are left untouched — matching the old
    // behavior where a SyntheticNetworkReply carried no such headers.
    if (m_cacheEntry) {
        QFileInfo targetInfo(m_targetPath);
        m_cacheEntry->setMD5Sum(QString::fromLatin1(md5.result().toHex()));
        m_cacheEntry->setLocalChangedTimestamp(targetInfo.lastModified().toUTC().toMSecsSinceEpoch());
        if (m_isEternal) {
            m_cacheEntry->makeEternal(true);
        } else {
            m_cacheEntry->setMaximumAge(kAria2DefaultCacheMaxAge);
        }
        m_cacheEntry->setCurrentAge(0);
        m_cacheEntry->setStale(false);
        APPLICATION->metacache()->updateEntry(m_cacheEntry);
    }

    emitSucceeded();
}

bool Aria2Download::abort()
{
    m_state = State::AbortedByUser;
    if (!m_gid.isEmpty()) {
        Aria2Manager::instance()->removeDownload(m_gid);
    }
    QFile::remove(m_tempPath);
    return true;
}

}  // namespace Net
