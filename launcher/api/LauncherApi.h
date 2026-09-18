// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QStringList>

#include <functional>

#include "api/ApiTypes.h"

class OperationService;
class UserInteraction;
class Task;

/**
 * Stable service boundary for alternate launcher user interfaces.
 *
 * Existing headless operations are registered as compatibility handlers. New GUI
 * domains can register an adapter here without changing the CLI/MCP transport or
 * moving the upstream GUI implementation. The API intentionally exposes only Qt Core
 * values; no QWidget, model, or page type crosses this boundary.
 */
class LauncherApi final : public QObject {
   public:
    using Handler = std::function<QJsonObject(const QJsonObject&, UserInteraction&)>;

    explicit LauncherApi(QObject* parent = nullptr);
    ~LauncherApi() override;

    QJsonObject execute(const QString& operation, const QJsonObject& parameters, UserInteraction& interaction);
    QJsonArray describe() const;
    void registerOperation(ApiOperation operation, Handler handler);
    /** Track a domain task so CLI/MCP cancellation reaches non-legacy adapters. */
    void trackTask(Task* task);
    void clearTrackedTask(Task* task);
    void cancelCurrent();

   private:
    struct RegisteredOperation {
        ApiOperation metadata;
        Handler handler;
    };

    struct TrackedTask {
        QPointer<Task> task;
        QJsonObject snapshot;
    };

    QJsonObject taskStatus(const QJsonObject& parameters) const;
    QJsonObject taskList(const QJsonObject& parameters) const;
    QJsonObject taskCancel(const QJsonObject& parameters);
    QJsonObject snapshotTask(const QString& taskId, Task* task) const;
    Task* currentTask() const;

    QHash<QString, RegisteredOperation> m_operations;
    QHash<QString, TrackedTask> m_tasks;
    QStringList m_taskOrder;
    OperationService* m_legacyService = nullptr;
    QPointer<Task> m_externalTask;
};
