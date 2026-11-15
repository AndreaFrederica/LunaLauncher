#pragma once

#include <QJsonArray>
#include <QElapsedTimer>
#include <QTimer>

#include "net/PclDownloadTask.h"

namespace Net {

/** Runs a complete NetJob PCL queue through one native LoaderDownload batch. */
class PclDownloadBatchTask final : public Task {
    Q_OBJECT
public:
    explicit PclDownloadBatchTask(QList<PclDownloadTask::Ptr> tasks);
    bool abort() override;

protected slots:
    void executeTask() override;

private slots:
    void onNativeEvent(int taskId, int eventType, qint64 downloaded, qint64 total, qint64 speed, int threads);
    void pollProgress();

private:
    void complete(bool success, const QString& error = {});

    QList<PclDownloadTask::Ptr> m_tasks;
    QTimer m_progressTimer;
    QElapsedTimer m_stallClock;
    qint64 m_lastDownloaded = 0;
    int m_stallTimeoutMs = 60000;
    int m_batchId = -1;
    bool m_done = false;
};

} // namespace Net
