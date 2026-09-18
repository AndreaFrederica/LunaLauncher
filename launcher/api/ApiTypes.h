// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QJsonObject>
#include <QString>

/**
 * Metadata for one operation exposed by the launcher API.
 *
 * The metadata is deliberately made up of Qt Core value types only. UI adapters can
 * serialize it to MCP, CLI JSON, or another transport without exposing widgets,
 * models, or page classes.
 */
struct ApiOperation {
    QString name;
    QString description;
    QJsonObject inputSchema;
    QString capability;
    bool destructive = false;

    QJsonObject toJson() const;
};
