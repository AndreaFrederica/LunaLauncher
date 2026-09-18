// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>

#include <functional>

#include "api/ApiTypes.h"

class OperationService;
class UserInteraction;

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
    void cancelCurrent();

   private:
    struct RegisteredOperation {
        ApiOperation metadata;
        Handler handler;
    };

    QHash<QString, RegisteredOperation> m_operations;
    OperationService* m_legacyService = nullptr;
};
