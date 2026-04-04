// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
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
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *      Copyright 2013-2021 MultiMC Contributors
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QString>

#include <QDateTime>
#include <QMap>
#include <QVariantMap>

enum class Validity { None, Assumed, Certain };

struct Token {
    QDateTime issueInstant;
    QDateTime notAfter;
    QString token;
    QString refresh_token;
    QVariantMap extra;

    Validity validity = Validity::None;
    bool persistent = true;
};

struct Skin {
    QString id;
    QString url;
    QString variant;

    QByteArray data;
};

struct Cape {
    QString id;
    QString url;
    QString alias;

    QByteArray data;
};

struct MinecraftEntitlement {
    bool ownsMinecraft = false;
    bool canPlayMinecraft = false;
    Validity validity = Validity::None;
};

struct MinecraftProfile {
    QString id;
    QString name;
    Skin skin;
    QString currentCape;
    QMap<QString, Cape> capes;
    Validity validity = Validity::None;
};

enum class AccountType { MSA, Offline, Yggdrasil, UnifiedPass };

enum class YggdrasilTokenType { Standard, OAuth };

enum class AccountState { Unchecked, Offline, Working, Online, Disabled, Errored, Expired, Gone };

struct UnifiedPassServerConfig {
    QString serverId;    // 32-character server ID
    QString baseUrl;     // API base URL (updated from meta)
    QString serverName;  // Server name (from meta)
    QString serverIP;    // Server IP (from meta, optional)
    QString jarHash;     // Client component SHA1 (from meta, optional)
};

struct YggdrasilServerConfig {
    QString authServerUrl;
    QString sessionServerUrl;
    QString sourceName;  // e.g., "LittleSkin", "Custom Server", etc.
    // Custom endpoints (empty means use default)
    QString authenticateEndpoint;  // default: "/authserver/authenticate"
    QString refreshEndpoint;       // default: "/sessionserver/refresh"
    QString validateEndpoint;      // default: "/sessionserver/validate"
    QString profileEndpoint;       // default: "/sessionserver/session/minecraft/profile/{uuid}"
    // OAuth 2.0 endpoints (for Yggdrasil Connect / OAuth-based authentication)
    QString oauthTokenEndpoint;    // default: "/oauth2/token"
    // The token type (Standard Yggdrasil or OAuth JWT)
    YggdrasilTokenType tokenType = YggdrasilTokenType::Standard;
};

struct AccountData {
    QJsonObject saveState() const;
    bool resumeStateFromV3(QJsonObject data);

    //! Yggdrasil access token, as passed to the game.
    QString accessToken() const;

    QString profileId() const;
    QString profileName() const;
    QString accountDisplayString() const;

    QString lastError() const;

    AccountType type = AccountType::MSA;

    // Yggdrasil service configuration
    YggdrasilServerConfig yggdrasilConfig;

    // UnifiedPass service configuration
    UnifiedPassServerConfig unifiedPassConfig;

    QString msaClientID;
    Token msaToken;
    Token userToken;
    Token xboxApiToken;
    Token mojangservicesToken;

    Token yggdrasilToken;

    // UnifiedPass token
    Token unifiedPassToken;

    MinecraftProfile minecraftProfile;
    MinecraftEntitlement minecraftEntitlement;
    Validity validity_ = Validity::None;

    // runtime only information (not saved with the account)
    QString internalId;
    QString errorString;
    AccountState accountState = AccountState::Unchecked;
};
