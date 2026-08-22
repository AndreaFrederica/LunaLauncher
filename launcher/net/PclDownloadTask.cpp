// SPDX-License-Identifier: GPL-3.0-only
/*
 *  LunaLauncher - Minecraft Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "net/PclDownloadTask.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QNetworkRequest>

#include "Application.h"
#include "BuildConfig.h"
#include "FileSystem.h"
#include "StringUtils.h"
#include "net/ChecksumValidator.h"
#include "net/FileSink.h"
#include "net/Logging.h"
#include "net/MetaCacheSink.h"
#include "net/PclDownloadLibrary.h"
#include "settings/SettingsObject.h"

namespace {
// 1 week in seconds — mirrors MetaCacheSink's private MAX_TIME_TO_EXPIRE.
constexpr qint64 kPclDefaultCacheMaxAge = 1 * 7 * 24 * 60 * 60;

class PclValidationReply final : public QNetworkReply {
   public:
    explicit PclValidationReply(const QUrl& url)
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

PclDownloadTask::PclDownloadTask(QString path) : Download(), m_targetPath(std::move(path))
{
    logCat = taskDownloadLogC;
}

bool PclDownloadTask::shouldUseFor(const QUrl& url)
{
    if (!APPLICATION_DYN || !BuildConfig.PCL_DOWNLOAD_ENABLED) {
        return false;
    }
    if (APPLICATION->settings()->get("DownloadBackend").toInt() != 2) {
        return false;
    }
    if (!PclDownloadLibrary::instance().isLoaded() && !PclDownloadLibrary::instance().load()) {
        return false;
    }
    return url.scheme() == "http" || url.scheme() == "https";
}

Download::Ptr PclDownloadTask::makeFile(QUrl url, QString path, Options options)
{
    auto dl = makeShared<PclDownloadTask>(path);
    dl->m_url = std::move(url);
    dl->setObjectName(QString("PCL_FILE:") + dl->m_url.toString());
    dl->m_options = options;
    dl->m_sink.reset(new FileSink(path));
    return dl;
}

Download::Ptr PclDownloadTask::makeCached(QUrl url, MetaEntryPtr entry, Options options)
{
    auto dl = makeShared<PclDownloadTask>(entry->getFullPath());
    dl->m_url = std::move(url);
    dl->setObjectName(QString("PCL_CACHE:") + dl->m_url.toString());
    dl->m_options = options;
    dl->m_cacheEntry = entry;
    dl->m_isEternal = options.testFlag(Option::MakeEternal);
    auto md5Node = new ChecksumValidator(QCryptographicHash::Md5);
    dl->m_sink.reset(new MetaCacheSink(entry, md5Node, dl->m_isEternal));
    return dl;
}

void PclDownloadTask::executeTask()
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

    if (!FS::ensureFilePathExists(m_targetPath)) {
        emitFailed(tr("Could not create folder for %1").arg(m_targetPath));
        return;
    }

    auto& lib = PclDownloadLibrary::instance();
    auto* bridge = lib.bridge();
    if (!bridge) {
        startQtFallback(tr("PCL.Download event bridge is unavailable"));
        return;
    }

    // Connect before starting so no event is missed. Events are queued to the
    // main thread, so m_taskId is always assigned before any handler runs.
    connect(bridge, &PclDownloadBridge::taskEvent, this, &PclDownloadTask::onTaskEvent);

    m_taskId = lib.startFileDownload(m_url, m_targetPath);
    if (m_taskId < 0) {
        startQtFallback(tr("PCL.Download failed to start the download"));
        return;
    }

    m_usingPcl = true;
    setStatus(tr("Downloading with PCL.Download: %1").arg(StringUtils::truncateUrlHumanFriendly(m_url, 80)));

    // Safety net: handle a terminal state that might have been reached before
    // the connection was fully set up (should not happen in practice).
    const int state = lib.taskState(m_taskId);
    if (state == 1 || state == 2 || state == 3) {
        onTaskEvent(m_taskId, state, 0, -1, 0, 0);
    }
}

void PclDownloadTask::onTaskEvent(int taskId, int eventType, qint64 downloaded, qint64 total, qint64 speed, int threads)
{
    Q_UNUSED(threads)
    if (m_finished || taskId != m_taskId) {
        return;
    }

    auto& lib = PclDownloadLibrary::instance();

    switch (static_cast<PclDownloadEvent>(eventType)) {
        case PclDownloadEvent::Progress: {
            setTransferRate(speed);
            if (total > 0) {
                setProgress(downloaded, total);
            }
            const QString progress = tr("%1 / %2")
                                         .arg(StringUtils::humanReadableFileSize(downloaded))
                                         .arg(total > 0 ? StringUtils::humanReadableFileSize(total) : tr("unknown"));
            const QString speedText = tr("%1 /s").arg(StringUtils::humanReadableFileSize(speed));
            setDetails(progress + "\n" + speedText);
            break;
        }
        case PclDownloadEvent::Finished: {
            m_finished = true;
            lib.releaseTask(m_taskId);
            if (m_state == State::AbortedByUser) {
                emit aborted();
                emit finished();
                return;
            }
            finishDownload();
            break;
        }
        case PclDownloadEvent::Failed:
        case PclDownloadEvent::Aborted: {
            m_finished = true;
            const QString error = lib.taskErrorString(m_taskId);
            lib.releaseTask(m_taskId);
            failOrAbort(error);
            break;
        }
    }
}

void PclDownloadTask::finishDownload()
{
    QFileInfo targetInfo(m_targetPath);
    if (!targetInfo.exists() || !targetInfo.isFile()) {
        m_error = QNetworkReply::UnknownContentError;
        m_errorString = tr("PCL.Download finished but produced no file: %1").arg(m_targetPath);
        emitFailed(m_errorString);
        return;
    }

    m_statusCode = 200;

    if (m_sink->hasValidators()) {
        QFile input(m_targetPath);
        if (!input.open(QIODevice::ReadOnly)) {
            m_error = QNetworkReply::ContentAccessDenied;
            m_errorString = tr("Could not open PCL.Download output file %1: %2").arg(m_targetPath, input.errorString());
            emitFailed(m_errorString);
            return;
        }

        QNetworkRequest request(m_url);
        for (const auto& headerProxy : m_headerProxies) {
            headerProxy->writeHeaders(request);
        }
        PclValidationReply reply(m_url);
        if (!m_sink->validateExternalData(input, request, reply)) {
            input.close();
            QFile::remove(m_targetPath);
            if (m_cacheEntry) {
                m_cacheEntry->setStale(true);
                APPLICATION->metacache()->updateEntry(m_cacheEntry);
            }
            m_error = QNetworkReply::UnknownContentError;
            m_errorString = tr("PCL.Download output validation failed for %1: %2").arg(m_targetPath, m_sink->failReason());
            emitFailed(m_errorString);
            return;
        }
    }

    // For the cached (MetaCacheSink) case only: update the cache metadata manually,
    // mirroring Aria2Download::finishFromTempFile. PCL.Download gives us no HTTP
    // headers, so ETag and Last-Modified are left untouched.
    if (m_cacheEntry) {
        if (auto* cacheSink = dynamic_cast<MetaCacheSink*>(m_sink.get())) {
            m_cacheEntry->setMD5Sum(QString::fromLatin1(cacheSink->md5Sum().toHex()));
        }
        m_cacheEntry->setLocalChangedTimestamp(targetInfo.lastModified().toUTC().toMSecsSinceEpoch());
        if (m_isEternal) {
            m_cacheEntry->makeEternal(true);
        } else {
            m_cacheEntry->setMaximumAge(kPclDefaultCacheMaxAge);
        }
        m_cacheEntry->setCurrentAge(0);
        m_cacheEntry->setStale(false);
        APPLICATION->metacache()->updateEntry(m_cacheEntry);
    }

    setProgress(targetInfo.size(), targetInfo.size());
    emitSucceeded();
}

void PclDownloadTask::failOrAbort(const QString& reason)
{
    if (m_state == State::AbortedByUser) {
        emit aborted();
        emit finished();
        return;
    }
    m_error = QNetworkReply::UnknownNetworkError;
    m_errorString = reason.isEmpty() ? tr("PCL.Download failed for %1").arg(m_url.toString()) : reason;
    emitFailed(m_errorString);
}

void PclDownloadTask::startQtFallback(const QString& reason)
{
    qCWarning(taskDownloadLogC) << getUid().toString() << "Falling back to Qt download for" << m_url << "because" << reason;
    if (!APPLICATION->settings()->get("PclDownloadFallbackToQt").toBool()) {
        m_error = QNetworkReply::UnknownNetworkError;
        m_errorString = reason;
        emitFailed(reason);
        return;
    }
    Download::executeTask();
}

bool PclDownloadTask::abort()
{
    if (!m_usingPcl) {
        return Download::abort();
    }
    m_state = State::AbortedByUser;
    if (m_taskId >= 0 && !m_finished) {
        PclDownloadLibrary::instance().abortDownload(m_taskId);
    }
    return true;
}

}  // namespace Net
