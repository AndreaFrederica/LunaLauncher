#include "net/PclDownloadBatchTask.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>
#include "StringUtils.h"

#include "net/PclDownloadLibrary.h"
#if defined(LAUNCHER_APPLICATION)
#include "Application.h"
#include "settings/SettingsObject.h"
#endif

namespace Net {

PclDownloadBatchTask::PclDownloadBatchTask(QList<PclDownloadTask::Ptr> tasks) : Task(), m_tasks(std::move(tasks))
{
    setObjectName(tr("Downloading files"));
    for (const auto& task : m_tasks) {
        auto step = std::make_shared<TaskStepProgress>(task->getUid());
        step->label = QFileInfo(task->batchDescriptor().value("localPath").toString()).fileName();
        step->progressUnit = "bytes";
        step->status = tr("Waiting to download");
        m_step_progress.append(step);
    }
    setProgress(0, m_tasks.size());
    connect(&m_progressTimer, &QTimer::timeout, this, &PclDownloadBatchTask::pollProgress);
}

void PclDownloadBatchTask::executeTask()
{
    m_done = false;
    m_batchId = -1;
    m_lastDownloaded = 0;
    for (const auto& task : m_tasks)
        task->startBatchProxy();
    for (const auto& step : m_step_progress) {
        step->state = TaskStepState::Waiting;
        step->current = 0;
        step->total = -1;
        step->transferRate = 0;
        step->status = tr("Waiting to download");
        step->details.clear();
    }
    setProgress(0, m_tasks.size());
    setStatus(tr("Downloading files"));
    setDetails({});
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
    Q_UNUSED(downloaded)
    Q_UNUSED(total)
    Q_UNUSED(speed)
    if (m_done || taskId != m_batchId)
        return;
    if (eventType == static_cast<int>(PclDownloadEvent::Progress)) {
        // Native progress events describe an individual file, not the batch.
        // Poll the complete file snapshot for consistent counts and total speed.
        return;
    }
    pollProgress();
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
    updateFiles(object.value("files").toArray());
}

void PclDownloadBatchTask::updateFiles(const QJsonArray& files)
{
    qint64 downloaded = 0;
    qint64 speed = 0;
    int completed = 0;
    for (qsizetype i = 0; i < files.size() && i < m_tasks.size(); ++i) {
        const auto file = files.at(i).toObject();
        const auto current = qMax<qint64>(0, file.value("downloaded").toInteger());
        const auto total = file.value("totalSize").toInteger(-1);
        const int nativeState = file.value("state").toInt(-1);
        const auto& task = m_tasks.at(i);
        // Validate each finished file before marking it successful. When one
        // download fails, keep successful files out of the manual recovery list.
        if (nativeState == 5 && !task->isFinished())
            task->finishBatchProxy();
        auto& step = *m_step_progress.at(i);
        const auto state = task->isFinished() ? (task->wasSuccessful() ? TaskStepState::Succeeded : TaskStepState::Failed) :
            nativeState == 6 ? TaskStepState::Failed : nativeState > 0 ? TaskStepState::Running : TaskStepState::Waiting;
        const auto rate = state == TaskStepState::Running ? qMax<qint64>(0, file.value("speed").toInteger()) : 0;
        const auto status = state == TaskStepState::Succeeded ? tr("Downloaded") :
            state == TaskStepState::Failed ? tr("Download failed") : state == TaskStepState::Waiting ? tr("Waiting to download") :
            nativeState == 1 ? tr("Connecting") : nativeState == 4 ? tr("Verifying file") : tr("Downloading");
        const auto details = state == TaskStepState::Failed ? task->errorString() :
            tr("%1 / %2").arg(StringUtils::humanReadableFileSize(current), total > 0 ? StringUtils::humanReadableFileSize(total) : tr("Size unknown"));
        if (step.current != current || step.total != total || step.state != state || step.transferRate != rate || step.status != status || step.details != details) {
            step.current = current;
            step.total = total;
            step.state = state;
            step.transferRate = rate;
            step.status = status;
            step.details = details;
            emit stepProgress(step);
        }
        task->setProgress(current, total);
        task->setTransferRate(rate);
        downloaded += current;
        speed += rate;
        completed += state == TaskStepState::Succeeded;
    }
    if (downloaded != m_lastDownloaded) {
        m_lastDownloaded = downloaded;
        m_stallClock.restart();
    } else if (m_stallTimeoutMs > 0 && m_stallClock.isValid() && m_stallClock.elapsed() >= m_stallTimeoutMs) {
        PclDownloadLibrary::instance().abortBatch(m_batchId);
        complete(false, tr("PCL.Download received no data for %1 s").arg(m_stallTimeoutMs / 1000));
        return;
    }
    setProgress(completed, m_tasks.size());
    setTransferRate(speed);
}

void PclDownloadBatchTask::complete(bool success, const QString& error)
{
    if (m_done)
        return;
    m_done = true;
    m_progressTimer.stop();
    disconnect(PclDownloadLibrary::instance().bridge(), nullptr, this, nullptr);
    bool allSucceeded = success;
    for (qsizetype i = 0; i < m_tasks.size(); ++i) {
        const auto& task = m_tasks.at(i);
        if (success)
            allSucceeded = task->finishBatchProxy() && allSucceeded;
        else
            task->failBatchProxy(error);
        auto& step = *m_step_progress.at(i);
        step.state = task->wasSuccessful() ? TaskStepState::Succeeded : TaskStepState::Failed;
        step.transferRate = 0;
        step.status = task->wasSuccessful() ? tr("Downloaded") : tr("Download failed");
        if (!task->wasSuccessful()) step.details = task->errorString();
        emit stepProgress(step);
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
