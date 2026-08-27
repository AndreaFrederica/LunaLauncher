// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QTemporaryDir>

#include "cli/UserInteraction.h"
#include "minecraft/auth/MinecraftAccount.h"

class BaseInstance;
class SettingsObject;
class Task;

class OperationService final : public QObject {
    Q_OBJECT

   public:
    explicit OperationService(QObject* parent = nullptr);

    QJsonObject execute(const QString& operation, const QJsonObject& parameters, UserInteraction& interaction);
    void cancelCurrent();

    static QJsonObject success(const QJsonValue& data = QJsonObject());
    static QJsonObject failure(const QString& message, int exitCode = 1);

   private:
    QJsonObject listInstances();
    QJsonObject listAccounts();
    QJsonObject setDefaultAccount(const QJsonObject& parameters);
    QJsonObject removeAccount(const QJsonObject& parameters);
    QJsonObject refreshAccount(const QJsonObject& parameters, UserInteraction& interaction);
    QJsonObject instanceInfo(const QJsonObject& parameters);
    QJsonObject renameInstance(const QJsonObject& parameters);
    QJsonObject groupInstance(const QJsonObject& parameters);
    QJsonObject copyInstance(const QJsonObject& parameters, UserInteraction& interaction);
    QJsonObject updateInstance(const QJsonObject& parameters, UserInteraction& interaction);
    QJsonObject deleteInstance(const QJsonObject& parameters);
    QJsonObject undoDeleteInstance();
    QJsonObject listResources(const QJsonObject& parameters);
    QJsonObject installResource(const QJsonObject& parameters, UserInteraction& interaction);
    QJsonObject setResourceEnabled(const QJsonObject& parameters, bool enabled);
    QJsonObject removeResource(const QJsonObject& parameters);
    QJsonObject listJava(UserInteraction& interaction);
    QJsonObject listSettings(const QJsonObject& parameters);
    QJsonObject getSetting(const QJsonObject& parameters);
    QJsonObject setSetting(const QJsonObject& parameters, UserInteraction& interaction);
    QJsonObject resetSetting(const QJsonObject& parameters);
    QJsonObject loginAccount(const QJsonObject& parameters, UserInteraction& interaction);
    QJsonObject importInstance(const QJsonObject& parameters, UserInteraction& interaction);
    QJsonObject launchInstance(const QJsonObject& parameters, UserInteraction& interaction);
    QJsonObject setupMicrosoftProfile(const MinecraftAccountPtr& account, const QString& name, UserInteraction& interaction);
    QJsonObject resolvePackSource(const QString& source, UserInteraction& interaction);
    QJsonObject fetchJson(const QUrl& url, const QString& taskName, QJsonDocument* document, UserInteraction& interaction);
    bool waitForTask(Task* task, UserInteraction& interaction, QString* error);
    SettingsObject* resolveSettings(const QJsonObject& parameters, QString* error, BaseInstance** instance = nullptr) const;

    QTemporaryDir m_downloads;
    QPointer<Task> m_currentTask;
};
