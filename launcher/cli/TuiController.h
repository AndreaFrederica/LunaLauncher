// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QJsonObject>
#include <QObject>

#include "cli/CliInteraction.h"

class LauncherApi;

class TuiController final : public QObject {
    Q_OBJECT

   public:
    explicit TuiController(QObject* parent = nullptr);

   public slots:
    void run();

   private:
    bool showInstances(LauncherApi& service);
    bool showAccounts(LauncherApi& service);
    bool loginAccount(LauncherApi& service);
    bool importInstance(LauncherApi& service);
    bool launchInstance(LauncherApi& service);
    bool manageSettings(LauncherApi& service, bool instanceScope);
    bool manageInstances(LauncherApi& service);
    bool manageAccounts(LauncherApi& service);
    bool manageResources(LauncherApi& service);
    bool showJava(LauncherApi& service);
    bool printResult(const QString& operation, const QJsonObject& result);
    bool confirm(const QString& prompt, bool defaultValue = false);
    void waitForEnter();

    CliInteraction m_interaction{ false, false };
};
