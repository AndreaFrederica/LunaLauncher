// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <QJsonObject>
#include <QObject>
#include <QByteArray>
#include <QQueue>
#include <QThreadPool>

class QEventLoop;

#include "api/LauncherApi.h"

class McpServer final : public QObject {
    Q_OBJECT

   public:
    explicit McpServer(QObject* parent = nullptr);

   public slots:
    void start();

   private slots:
    void readMessage();

   private:
    void handleMessage(const QJsonObject& request);
    void writeMessage(const QJsonObject& message);
    void writeResult(const QJsonValue& id, const QJsonValue& result);
    void writeError(const QJsonValue& id, int code, const QString& message);
    QJsonArray tools() const;
    void executeOperation(const QJsonValue& id, const QString& operation, const QJsonObject& arguments,
                          const QJsonObject& metadata, bool native);
    void enqueueOperation(const QJsonValue& id, const QString& operation, const QJsonObject& arguments,
                          const QJsonObject& metadata, bool native);
    void runWrite(const QJsonValue& id, const QString& operation, const QJsonObject& arguments,
                  const QJsonObject& metadata, bool native, bool control);
    void runRead(const QString& operation, const QJsonObject& arguments, const QJsonValue& id,
                 const QJsonObject& metadata, bool native);
    void startNext();
    QJsonObject schedulerStatus() const;
    QJsonValue requestInteraction(const QString& prompt, bool secret, const QJsonArray* choices);
    void disconnectInput();

    QObject* m_notifier = nullptr;
    QByteArray m_input;
    bool m_disconnected = false;
    bool m_discardingLine = false;
    struct PendingOperation {
        QJsonValue id;
        QString operation;
        QJsonObject arguments;
        QJsonObject metadata;
        bool native = false;
        bool highPriority = false;
        QString conflictKey;
    };

    LauncherApi m_service;
    QQueue<PendingOperation> m_pending;
    QThreadPool m_readPool;
    bool m_writeActive = false;
    QString m_activeWriteKey;
    int m_activeReads = 0;
    qint64 m_completedReads = 0;
    qint64 m_completedWrites = 0;
    LauncherApi* m_activeService = nullptr;
    QJsonValue m_activeRequestId;
    bool m_cancelled = false;
    QEventLoop* m_interactionLoop = nullptr;
    QString m_interactionId;
    QJsonValue m_interactionAnswer;
    int m_choiceCount = -1;
};
