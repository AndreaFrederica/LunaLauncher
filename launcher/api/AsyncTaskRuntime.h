// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QQueue>
#include <QSet>
#include <QStringList>
#include <QTemporaryDir>
#include <QTimer>

#include "tasks/Task.h"

class UserInteraction;

/**
 * Event-driven task runtime for the Neo UI.
 *
 * All state transitions happen on the Qt main thread. Background work is
 * represented by ordinary Qt Tasks, while the runtime owns admission control,
 * resource leases, cancellation, deadlines, task snapshots and a small crash
 * journal. It intentionally does not route through OperationService.
 */
class AsyncTaskRuntime final : public QObject {
    Q_OBJECT

   public:
    static AsyncTaskRuntime& instance();

    QJsonObject start(const QString& operation, const QJsonObject& parameters, UserInteraction& interaction);
    QJsonObject status(const QString& taskId) const;
    QJsonObject list() const;
    QJsonObject cancel(const QString& taskId);

    static constexpr int maxActive = 6;

   signals:
    void taskChanged(const QJsonObject& snapshot);

   private:
    struct Pending {
        QString id;
    };

    struct Record {
        QString id;
        QString operation;
        QJsonObject parameters;
        QSet<QString> reads;
        QSet<QString> writes;
        QString conflictKey;
        QString idempotencyKey;
        QString state = "queued";
        QString status;
        QString error;
        QString instanceId;
        QString createdInstanceId;
        qint64 pid = -1;
        QString deviceCode;
        QString deviceUrl;
        int deviceExpiresIn = 0;
        qint64 progress = 0;
        qint64 total = 0;
        bool finished = false;
        bool cancelRequested = false;
        int priority = 0;
        int deadlineMs = 0;
        qint64 queuedAt = 0;
        qint64 startedAt = 0;
        qint64 finishedAt = 0;
        QPointer<Task> task;
        Task::Ptr holder;
        QSet<QString> instancesBefore;
        QTimer* deadline = nullptr;
    };

    explicit AsyncTaskRuntime(QObject* parent = nullptr);

    void pump();
    bool canAcquire(const Record& record) const;
    void acquire(const Record& record);
    void release(const Record& record);
    void startRecord(const Record& record);
    void armDeadline(const QString& id);
    void finish(const QString& taskId, bool success, const QString& error = {}, const QString& instanceId = {});
    void emitChanged(const Record& record);
    QJsonObject snapshot(const Record& record) const;
    void addResourceKeys(Record& record) const;
    QString conflictKeyFor(const QString& operation, const QJsonObject& parameters) const;
    int deadlineFor(const QString& operation) const;
    void loadJournal();
    void saveJournal() const;

    QHash<QString, Record> m_records;
    QStringList m_order;
    QQueue<Pending> m_pending;
    QHash<QString, int> m_readerCounts;
    QSet<QString> m_writers;
    int m_active = 0;
    QTemporaryDir m_temp;
};
