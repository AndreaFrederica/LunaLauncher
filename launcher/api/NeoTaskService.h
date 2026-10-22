// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QTemporaryDir>
#include <QHash>
#include <QQueue>
#include <QSet>
#include <QStringList>

#include "tasks/Task.h"

class BaseInstance;
class UserInteraction;

/**
 * Independent asynchronous task lane for the Neo UI API.
 *
 * This service deliberately does not route through OperationService: it owns
 * its own queue, conflict keys, priority and task snapshots. Long-running work
 * is started as a Qt Task and returns immediately, so independent instances can
 * progress in parallel.
 */
class NeoTaskService final : public QObject {
   public:
    static NeoTaskService& instance();

    QJsonObject start(const QString& operation, const QJsonObject& parameters, UserInteraction& interaction);
    QJsonObject status(const QString& taskId) const;
    QJsonObject list() const;
    QJsonObject cancel(const QString& taskId);

   private:
    struct Record {
        QString id;
        QString operation;
        QJsonObject parameters;
        QString conflictKey;
        QString state = "queued";
        QString status;
        QString error;
        QString instanceId;
        QString createdInstanceId;
        qint64 progress = 0;
        qint64 total = 0;
        bool finished = false;
        bool highPriority = false;
        QPointer<Task> task;
        Task::Ptr holder;
        QSet<QString> instancesBefore;
    };

    struct Pending {
        QString id;
        QString operation;
        QJsonObject parameters;
        QString conflictKey;
        bool highPriority = false;
    };

    explicit NeoTaskService(QObject* parent = nullptr);
    void pump();
    void startRecord(const Record& record);
    void finish(const QString& taskId, bool success, const QString& error = {}, const QString& instanceId = {});
    QJsonObject snapshot(const Record& record) const;
    QString conflictKeyFor(const QString& operation, const QJsonObject& parameters) const;

    QHash<QString, Record> m_records;
    QStringList m_order;
    QQueue<Pending> m_pending;
    QSet<QString> m_activeKeys;
    int m_active = 0;
    QTemporaryDir m_temp;
    UserInteraction* m_interaction = nullptr;
};
