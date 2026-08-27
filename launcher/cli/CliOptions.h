// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QString>
#include <QStringList>

struct CliOptions {
    bool cli = false;
    bool mcp = false;
    bool tui = false;
    bool json = false;
    bool nonInteractive = false;
    bool passwordStdin = false;
    bool wait = false;
    bool detach = false;
    bool revealSecrets = false;

    QStringList command;
    QString name;
    QString profile;
    QString offlineName;
    QString server;
    QString world;
    QString username;
    QString authUrl;
    QString sessionUrl;
    QString sourceName;
    QString serverId;
    QString profileId;
    QString minecraftProfileName;

    bool isHeadless() const { return cli || mcp || tui; }
};
