// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QJsonObject>
#include <QObject>
#include <QByteArray>

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
    QJsonValue requestInteraction(const QString& prompt, bool secret, const QJsonArray* choices);
    void disconnectInput();

    QObject* m_notifier = nullptr;
    QByteArray m_input;
    bool m_disconnected = false;
    bool m_discardingLine = false;
    LauncherApi m_service;
    LauncherApi* m_activeService = nullptr;
    QJsonValue m_activeRequestId;
    bool m_cancelled = false;
    QEventLoop* m_interactionLoop = nullptr;
    QString m_interactionId;
    QJsonValue m_interactionAnswer;
    int m_choiceCount = -1;
};
