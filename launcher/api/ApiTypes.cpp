// SPDX-License-Identifier: GPL-3.0-only

#include "ApiTypes.h"

QJsonObject ApiOperation::toJson() const
{
    QJsonObject result{ { "name", name }, { "description", description }, { "destructive", destructive } };
    if (!inputSchema.isEmpty())
        result.insert("inputSchema", inputSchema);
    if (!capability.isEmpty())
        result.insert("capability", capability);
    return result;
}
