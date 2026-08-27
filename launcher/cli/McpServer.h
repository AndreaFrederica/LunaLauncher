// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QJsonObject>
#include <QObject>
#include <QTextStream>

class OperationService;

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

    QObject* m_notifier = nullptr;
    QTextStream m_input;
    OperationService* m_activeService = nullptr;
};
