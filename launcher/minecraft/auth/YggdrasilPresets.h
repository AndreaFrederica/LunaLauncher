// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <QVector>
#include <QString>
#include "AccountData.h"

struct YggdrasilPreset {
    QString name;
    QString authUrl;
    QString sessionUrl;
    // Custom endpoints (empty means use default)
    QString authenticateEndpoint;  // default: "/authserver/authenticate"
    QString refreshEndpoint;       // default: "/sessionserver/refresh"
    QString validateEndpoint;      // default: "/sessionserver/validate"
    QString profileEndpoint;       // default: "/sessionserver/session/minecraft/profile/{uuid}"
    // OAuth 2.0 endpoints
    QString oauthTokenEndpoint;    // default: "/oauth2/token"
    // The token type to use
    YggdrasilTokenType tokenType = YggdrasilTokenType::Standard;
};

class YggdrasilPresets {
public:
    static const QVector<YggdrasilPreset>& getDefaults();
    static bool addCustomPreset(const YggdrasilPreset& preset);
    static bool removeCustomPreset(const QString& name);
    static QVector<YggdrasilPreset> getCustomPresets();
    static QVector<YggdrasilPreset> getAllPresets();

private:
    static QVector<YggdrasilPreset> s_defaults;
    static void initDefaults();
};
