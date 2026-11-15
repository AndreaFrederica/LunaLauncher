#include "net/PclDownloadBatchTask.h"

#include <QJsonDocument>
#include <QJsonObject>

#include "net/PclDownloadLibrary.h"
#if defined(LAUNCHER_APPLICATION)
#include "Application.h"
#include "settings/SettingsObject.h"
#endif

namespace Net {

PclDownloadBatchTask::PclDownloadBatchTask(QList<PclDownloadTask::Ptr> tasks) : Task(), m_tasks(std::move(tasks))
{
    setObjectName(QStringLiteral("PCL_BATCH:%1").arg(m_tasks.size()));
    connect(&m_progressTimer, &QTimer::timeout, this, &PclDownloadBatchTask::pollProgress);
}

void PclDownloadBatchTask::executeTask()
{
    m_done = false;
    m_batchId = -1;
    m_lastDownloaded = 0;
    m_stallTimeoutMs = 60000;
#if defined(LAUNCHER_APPLICATION)
    if (APPLICATION_DYN) {
        const auto configuredTimeout = APPLICATION->settings()->get("RequestTimeout").toInt();
        m_stallTimeoutMs = configuredTimeout <= 0 ? 0 : qMax(5, configuredTimeout) * 1000;
    }
#endif
    auto& library = PclDownloadLibrary::instance();
    if (!library.isLoaded() && !library.load()) {
        complete(false, library.errorString());
        return;
    }

    auto* bridge = library.bridge();
    if (!bridge) {
        complete(false, tr("PCL.Download event bridge is unavailable"));
        return;
    }
    connect(bridge, &PclDownloadBridge::taskEvent, this, &PclDownloadBatchTask::onNativeEvent);

    QJsonArray files;
    for (const auto& task : m_tasks) {
        task->startBatchProxy();
        files.append(task->batchDescriptor());
    }
    m_batchId = library.startBatchDownload(files);
    if (m_batchId < 0) {
        complete(false, tr("PCL.Download failed to start the batch"));
        return;
    }
    m_progressTimer.start(250);
    m_stallClock.start();
}

void PclDownloadBatchTask::onNativeEvent(int taskId, int eventType, qint64 downloaded, qint64 total, qint64 speed, int threads)
{
    Q_UNUSED(threads)
    if (m_done || taskId != m_batchId)
        return;
    if (eventType == static_cast<int>(PclDownloadEvent::Progress)) {
        if (downloaded != m_lastDownloaded) {
            m_lastDownloaded = downloaded;
            m_stallClock.restart();
        }
        setProgress(total > 0 ? downloaded : 0, total > 0 ? total : 0);
        setTransferRate(speed);
        return;
    }
    complete(eventType == static_cast<int>(PclDownloadEvent::Finished), PclDownloadLibrary::instance().taskErrorString(m_batchId));
}

void PclDownloadBatchTask::pollProgress()
{
    if (m_done || m_batchId < 0)
        return;
    const auto detail = PclDownloadLibrary::instance().batchProgressDetailJson(m_batchId);
    if (detail.isEmpty())
        return;
    const auto doc = QJsonDocument::fromJson(detail);
    if (!doc.isObject())
        return;
    const auto object = doc.object();
    const auto progress = object.value("progress").toDouble();
    qint64 downloaded = 0;
    const auto files = object.value("files").toArray();
    for (qsizetype i = 0; i < files.size(); ++i) {
        const auto file = files.at(i).toObject();
        downloaded += file.value("downloaded").toInteger();
        if (i < m_tasks.size()) {
            const auto current = file.value("downloaded").toInteger();
            const auto total = file.value("totalSize").toInteger(-1);
            m_tasks.at(i)->setProgress(current, total);
            m_tasks.at(i)->setTransferRate(file.value("speed").toInteger());
        }
    }
    if (downloaded != m_lastDownloaded) {
        m_lastDownloaded = downloaded;
        m_stallClock.restart();
    } else if (m_stallTimeoutMs > 0 && m_stallClock.isValid() && m_stallClock.elapsed() >= m_stallTimeoutMs) {
        PclDownloadLibrary::instance().abortBatch(m_batchId);
        complete(false, tr("PCL.Download received no data for %1 s").arg(m_stallTimeoutMs / 1000));
        return;
    }
    setProgress(static_cast<qint64>(progress * 10000), 10000);
}

void PclDownloadBatchTask::complete(bool success, const QString& error)
{
    if (m_done)
        return;
    m_done = true;
    m_progressTimer.stop();
    disconnect(PclDownloadLibrary::instance().bridge(), nullptr, this, nullptr);
    bool allSucceeded = success;
    for (const auto& task : m_tasks) {
        if (success)
            allSucceeded = task->finishBatchProxy() && allSucceeded;
        else
            task->failBatchProxy(error);
    }
    if (m_batchId >= 0)
        PclDownloadLibrary::instance().releaseTask(m_batchId);
    if (allSucceeded)
        emitSucceeded();
    else
        emitFailed(error.isEmpty() ? tr("PCL.Download batch failed") : error);
}

bool PclDownloadBatchTask::abort()
{
    if (!isRunning())
        return true;
    if (m_batchId >= 0)
        PclDownloadLibrary::instance().abortBatch(m_batchId);
    complete(false, tr("PCL.Download batch aborted"));
    return true;
}

} // namespace Net
