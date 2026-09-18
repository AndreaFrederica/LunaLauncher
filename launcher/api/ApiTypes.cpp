// SPDX-License-Identifier: GPL-3.0-only

#include "ApiTypes.h"

QJsonObject ApiOperation::toJson() const
{
    QJsonObject result{ { "name", name }, { "description", description }, { "apiVersion", 1 }, { "destructive", destructive } };
    if (!inputSchema.isEmpty()) {
        // Adapters may provide a compact key/description map while they are
        // being migrated from GUI code. Normalize it to the JSON Schema shape
        // consumed by MCP and future transports.
        if (inputSchema.contains("type")) {
            result.insert("inputSchema", inputSchema);
        } else {
            QJsonObject properties;
            for (auto it = inputSchema.constBegin(); it != inputSchema.constEnd(); ++it) {
                if (it.value().isObject()) {
                    // A compact adapter may provide a schema fragment without a
                    // type (for example an arbitrary JSON value). Preserve the
                    // fragment; a schema with only description/constraints is
                    // valid JSON Schema and accepts every JSON type.
                    properties.insert(it.key(), it.value());
                } else {
                    properties.insert(it.key(), QJsonObject{ { "type", "string" }, { "description", it.value().toString() } });
                }
            }
            result.insert("inputSchema", QJsonObject{ { "type", "object" }, { "properties", properties }, { "additionalProperties", false } });
        }
    }
    if (!capability.isEmpty())
        result.insert("capability", capability);
    return result;
}
