// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QJsonObject>
#include <QObject>

#include "cli/CliInteraction.h"

class OperationService;

class TuiController final : public QObject {
    Q_OBJECT

   public:
    explicit TuiController(QObject* parent = nullptr);

   public slots:
    void run();

   private:
    bool showInstances(OperationService& service);
    bool showAccounts(OperationService& service);
    bool loginAccount(OperationService& service);
    bool importInstance(OperationService& service);
    bool launchInstance(OperationService& service);
    bool printResult(const QString& operation, const QJsonObject& result);
    bool confirm(const QString& prompt, bool defaultValue = false);
    void waitForEnter();

    CliInteraction m_interaction{ false, false };
};
