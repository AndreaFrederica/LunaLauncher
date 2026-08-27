// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QDebug>
#include <QString>

struct BlockedMod {
    QString name;
    QString websiteUrl;
    QString hash;
    bool matched;
    QString localPath;
    QString targetFolder;
    bool disabled = false;
    bool move = false;
};

inline QDebug operator<<(QDebug debug, const BlockedMod& mod)
{
    QDebugStateSaver saver(debug);
    debug.nospace() << "{ name: " << mod.name << ", websiteUrl: " << mod.websiteUrl << ", hash: " << mod.hash
                    << ", matched: " << mod.matched << ", localPath: " << mod.localPath << "}";
    return debug;
}
