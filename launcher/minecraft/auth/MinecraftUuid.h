// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QString>
#include <QUuid>

inline QUuid parseMinecraftUuid(QString value)
{
    // Minecraft Services uses 32 hexadecimal digits; QUuid expects separators.
    if (value.size() == 32) {
        value.insert(20, '-');
        value.insert(16, '-');
        value.insert(12, '-');
        value.insert(8, '-');
    }
    return QUuid(value);
}
