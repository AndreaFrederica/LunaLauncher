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

#include "AccountData.h"
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

namespace {
void tokenToJSONV3(QJsonObject& parent, const Token& t, const char* tokenName)
{
    if (!t.persistent) {
        return;
    }
    QJsonObject out;
    if (t.issueInstant.isValid()) {
        out["iat"] = QJsonValue(t.issueInstant.toMSecsSinceEpoch() / 1000);
    }

    if (t.notAfter.isValid()) {
        out["exp"] = QJsonValue(t.notAfter.toMSecsSinceEpoch() / 1000);
    }

    bool save = false;
    if (!t.token.isEmpty()) {
        out["token"] = QJsonValue(t.token);
        save = true;
    }
    if (!t.refresh_token.isEmpty()) {
        out["refresh_token"] = QJsonValue(t.refresh_token);
        save = true;
    }
    if (t.extra.size()) {
        // Encrypt sensitive fields before saving
        QJsonObject extraObj;
        for (auto it = t.extra.begin(); it != t.extra.end(); ++it) {
            const QString& key = it.key();
            const QVariant& value = it.value();

            // Skip empty values
            if (value.isNull() || !value.isValid()) {
                continue;
            }

            // Use obfuscated name for credentials field
            if (key == "credentials") {
                // Simple XOR encryption with hardcoded salt for credentials
                QString cred = value.toString();
                QByteArray data = cred.toUtf8();
                static const QByteArray salt = "PrismYggdrasilAuth2024";  // hardcoded salt
                QByteArray encrypted;
                for (int i = 0; i < data.size(); i++) {
                    encrypted.append(data[i] ^ salt[i % salt.size()]);
                }
                extraObj["cred"] = QString::fromLatin1(encrypted.toBase64());
            } else {
                extraObj[key] = QJsonValue::fromVariant(value);
            }
        }
        if (!extraObj.isEmpty()) {
            out["extra"] = extraObj;
            save = true;
        }
    }
    if (save) {
        parent[tokenName] = out;
    }
}

Token tokenFromJSONV3(const QJsonObject& parent, const char* tokenName)
{
    Token out;
    auto tokenObject = parent.value(tokenName).toObject();
    if (tokenObject.isEmpty()) {
        return out;
    }
    auto issueInstant = tokenObject.value("iat");
    if (issueInstant.isDouble()) {
        out.issueInstant = QDateTime::fromMSecsSinceEpoch(((int64_t)issueInstant.toDouble()) * 1000);
    }

    auto notAfter = tokenObject.value("exp");
    if (notAfter.isDouble()) {
        out.notAfter = QDateTime::fromMSecsSinceEpoch(((int64_t)notAfter.toDouble()) * 1000);
    }

    auto token = tokenObject.value("token");
    if (token.isString()) {
        out.token = token.toString();
        out.validity = Validity::Assumed;
    }

    auto refresh_token = tokenObject.value("refresh_token");
    if (refresh_token.isString()) {
        out.refresh_token = refresh_token.toString();
    }

    auto extra = tokenObject.value("extra");
    if (extra.isObject()) {
        QJsonObject extraObj = extra.toObject();
        QVariantMap extraMap;
        for (auto it = extraObj.begin(); it != extraObj.end(); ++it) {
            const QString& key = it.key();
            const QJsonValue& value = it.value();

            // Skip null values
            if (value.isNull()) {
                continue;
            }

            // Decrypt obfuscated credentials field
            if (key == "cred") {
                QString encryptedB64 = value.toString();
                if (!encryptedB64.isEmpty()) {
                    QByteArray encrypted = QByteArray::fromBase64(encryptedB64.toLatin1());
                    static const QByteArray salt = "PrismYggdrasilAuth2024";  // hardcoded salt
                    QByteArray decrypted;
                    for (int i = 0; i < encrypted.size(); i++) {
                        decrypted.append(encrypted[i] ^ salt[i % salt.size()]);
                    }
                    extraMap["credentials"] = QString::fromUtf8(decrypted);
                }
            } else if (key == "password") {
                // Legacy: old accounts use "password" field, migrate to "credentials"
                if (value.isString()) {
                    extraMap["credentials"] = value.toString();
                } else {
                    extraMap["credentials"] = value.toVariant();
                }
            } else if (key == "credentials") {
                // Legacy: if still using "credentials" key directly, keep as-is
                extraMap[key] = value.toVariant();
            } else {
                extraMap[key] = value.toVariant();
            }
        }
        out.extra = extraMap;
    }
    return out;
}

void profileToJSONV3(QJsonObject& parent, MinecraftProfile p, const char* tokenName)
{
    if (p.id.isEmpty()) {
        return;
    }
    QJsonObject out;
    out["id"] = QJsonValue(p.id);
    out["name"] = QJsonValue(p.name);
    if (!p.currentCape.isEmpty()) {
        out["cape"] = p.currentCape;
    }

    {
        QJsonObject skinObj;
        skinObj["id"] = p.skin.id;
        skinObj["url"] = p.skin.url;
        skinObj["variant"] = p.skin.variant;
        if (p.skin.data.size()) {
            skinObj["data"] = QString::fromLatin1(p.skin.data.toBase64());
        }
        out["skin"] = skinObj;
    }

    QJsonArray capesArray;
    for (auto& cape : p.capes) {
        QJsonObject capeObj;
        capeObj["id"] = cape.id;
        capeObj["url"] = cape.url;
        capeObj["alias"] = cape.alias;
        if (cape.data.size()) {
            capeObj["data"] = QString::fromLatin1(cape.data.toBase64());
        }
        capesArray.push_back(capeObj);
    }
    out["capes"] = capesArray;
    parent[tokenName] = out;
}

MinecraftProfile profileFromJSONV3(const QJsonObject& parent, const char* tokenName)
{
    MinecraftProfile out;
    auto tokenObject = parent.value(tokenName).toObject();
    if (tokenObject.isEmpty()) {
        return out;
    }
    {
        auto idV = tokenObject.value("id");
        auto nameV = tokenObject.value("name");
        if (!idV.isString() || !nameV.isString()) {
            qWarning() << "mandatory profile attributes are missing or of unexpected type";
            return MinecraftProfile();
        }
        out.name = nameV.toString();
        out.id = idV.toString();
    }

    {
        auto skinV = tokenObject.value("skin");
        if (!skinV.isObject()) {
            qWarning() << "skin is missing";
            return MinecraftProfile();
        }
        auto skinObj = skinV.toObject();
        auto idV = skinObj.value("id");
        auto urlV = skinObj.value("url");
        auto variantV = skinObj.value("variant");
        if (!idV.isString() || !urlV.isString() || !variantV.isString()) {
            qWarning() << "mandatory skin attributes are missing or of unexpected type";
            return MinecraftProfile();
        }
        out.skin.id = idV.toString();
        out.skin.url = urlV.toString();
        out.skin.url.replace("http://textures.minecraft.net", "https://textures.minecraft.net");
        out.skin.variant = variantV.toString();

        // data for skin is optional
        auto dataV = skinObj.value("data");
        if (dataV.isString()) {
            // TODO: validate base64
            out.skin.data = QByteArray::fromBase64(dataV.toString().toLatin1());
        } else if (!dataV.isUndefined()) {
            qWarning() << "skin data is something unexpected";
            return MinecraftProfile();
        }
    }

    {
        auto capesV = tokenObject.value("capes");
        if (!capesV.isArray()) {
            qWarning() << "capes is not an array!";
            return MinecraftProfile();
        }
        auto capesArray = capesV.toArray();
        for (auto capeV : capesArray) {
            if (!capeV.isObject()) {
                qWarning() << "cape is not an object!";
                return MinecraftProfile();
            }
            auto capeObj = capeV.toObject();
            auto idV = capeObj.value("id");
            auto urlV = capeObj.value("url");
            auto aliasV = capeObj.value("alias");
            if (!idV.isString() || !urlV.isString() || !aliasV.isString()) {
                qWarning() << "mandatory skin attributes are missing or of unexpected type";
                return MinecraftProfile();
            }
            Cape cape;
            cape.id = idV.toString();
            cape.url = urlV.toString();
            cape.url.replace("http://textures.minecraft.net", "https://textures.minecraft.net");
            cape.alias = aliasV.toString();

            // data for cape is optional.
            auto dataV = capeObj.value("data");
            if (dataV.isString()) {
                // TODO: validate base64
                cape.data = QByteArray::fromBase64(dataV.toString().toLatin1());
            } else if (!dataV.isUndefined()) {
                qWarning() << "cape data is something unexpected";
                return MinecraftProfile();
            }
            out.capes[cape.id] = cape;
        }
    }
    // current cape
    {
        auto capeV = tokenObject.value("cape");
        if (capeV.isString()) {
            auto currentCape = capeV.toString();
            if (out.capes.contains(currentCape)) {
                out.currentCape = currentCape;
            }
        }
    }
    out.validity = Validity::Assumed;
    return out;
}

void entitlementToJSONV3(QJsonObject& parent, MinecraftEntitlement p)
{
    if (p.validity == Validity::None) {
        return;
    }
    QJsonObject out;
    out["ownsMinecraft"] = QJsonValue(p.ownsMinecraft);
    out["canPlayMinecraft"] = QJsonValue(p.canPlayMinecraft);
    parent["entitlement"] = out;
}

bool entitlementFromJSONV3(const QJsonObject& parent, MinecraftEntitlement& out)
{
    auto entitlementObject = parent.value("entitlement").toObject();
    if (entitlementObject.isEmpty()) {
        return false;
    }
    {
        auto ownsMinecraftV = entitlementObject.value("ownsMinecraft");
        auto canPlayMinecraftV = entitlementObject.value("canPlayMinecraft");
        if (!ownsMinecraftV.isBool() || !canPlayMinecraftV.isBool()) {
            qWarning() << "mandatory attributes are missing or of unexpected type";
            return false;
        }
        out.canPlayMinecraft = canPlayMinecraftV.toBool(false);
        out.ownsMinecraft = ownsMinecraftV.toBool(false);
        out.validity = Validity::Assumed;
    }
    return true;
}

}  // namespace

bool AccountData::resumeStateFromV3(QJsonObject data)
{
    auto typeV = data.value("type");
    if (!typeV.isString()) {
        qWarning() << "Failed to parse account data: type is missing.";
        return false;
    }
    auto typeS = typeV.toString();
    if (typeS == "MSA") {
        type = AccountType::MSA;
    } else if (typeS == "Offline") {
        type = AccountType::Offline;
    } else if (typeS == "Yggdrasil") {
        type = AccountType::Yggdrasil;
    } else if (typeS == "UnifiedPass") {
        type = AccountType::UnifiedPass;
    } else {
        qWarning() << "Failed to parse account data: type is not recognized.";
        return false;
    }

    if (type == AccountType::MSA) {
        auto clientIDV = data.value("msa-client-id");
        if (clientIDV.isString()) {
            msaClientID = clientIDV.toString();
        }  // leave msaClientID empty if it doesn't exist or isn't a string
        msaToken = tokenFromJSONV3(data, "msa");
        userToken = tokenFromJSONV3(data, "utoken");
        xboxApiToken = tokenFromJSONV3(data, "xrp-main");
        mojangservicesToken = tokenFromJSONV3(data, "xrp-mc");
    } else if (type == AccountType::Yggdrasil) {
        auto configV = data.value("yggdrasilConfig");
        if (configV.isObject()) {
            auto configObj = configV.toObject();
            yggdrasilConfig.authServerUrl = configObj.value("authServerUrl").toString();
            yggdrasilConfig.sessionServerUrl = configObj.value("sessionServerUrl").toString();
            yggdrasilConfig.sourceName = configObj.value("sourceName").toString();
            yggdrasilConfig.authenticateEndpoint = configObj.value("authenticateEndpoint").toString();
            yggdrasilConfig.refreshEndpoint = configObj.value("refreshEndpoint").toString();
            yggdrasilConfig.validateEndpoint = configObj.value("validateEndpoint").toString();
            yggdrasilConfig.profileEndpoint = configObj.value("profileEndpoint").toString();
            yggdrasilConfig.oauthTokenEndpoint = configObj.value("oauthTokenEndpoint").toString();
            // Parse token type
            QString tokenTypeStr = configObj.value("tokenType").toString();
            if (tokenTypeStr == "OAuth") {
                yggdrasilConfig.tokenType = YggdrasilTokenType::OAuth;
            } else {
                yggdrasilConfig.tokenType = YggdrasilTokenType::Standard;
            }
        }
    } else if (type == AccountType::UnifiedPass) {
        auto configV = data.value("unifiedPassConfig");
        if (configV.isObject()) {
            auto configObj = configV.toObject();
            unifiedPassConfig.serverId = configObj.value("serverId").toString();
            unifiedPassConfig.baseUrl = configObj.value("baseUrl").toString();
            unifiedPassConfig.serverName = configObj.value("serverName").toString();
            unifiedPassConfig.serverIP = configObj.value("serverIP").toString();
            unifiedPassConfig.jarHash = configObj.value("jarHash").toString();
        }
    }

    // Load the appropriate token based on account type
    if (type == AccountType::UnifiedPass) {
        unifiedPassToken = tokenFromJSONV3(data, "upass");
    } else {
        yggdrasilToken = tokenFromJSONV3(data, "ygg");
        // versions before 7.2 used "offline" as the offline token
        if (yggdrasilToken.token == "offline")
            yggdrasilToken.token = "0";
    }

    minecraftProfile = profileFromJSONV3(data, "profile");
    if (!entitlementFromJSONV3(data, minecraftEntitlement)) {
        if (minecraftProfile.validity != Validity::None) {
            minecraftEntitlement.canPlayMinecraft = true;
            minecraftEntitlement.ownsMinecraft = true;
            minecraftEntitlement.validity = Validity::Assumed;
        }
    }

    validity_ = minecraftProfile.validity;
    return true;
}

QJsonObject AccountData::saveState() const
{
    QJsonObject output;
    if (type == AccountType::MSA) {
        output["type"] = "MSA";
        output["msa-client-id"] = msaClientID;
        tokenToJSONV3(output, msaToken, "msa");
        tokenToJSONV3(output, userToken, "utoken");
        tokenToJSONV3(output, xboxApiToken, "xrp-main");
        tokenToJSONV3(output, mojangservicesToken, "xrp-mc");
    } else if (type == AccountType::Offline) {
        output["type"] = "Offline";
    } else if (type == AccountType::Yggdrasil) {
        output["type"] = "Yggdrasil";
        QJsonObject configObj;
        configObj["authServerUrl"] = yggdrasilConfig.authServerUrl;
        configObj["sessionServerUrl"] = yggdrasilConfig.sessionServerUrl;
        if (!yggdrasilConfig.sourceName.isEmpty()) {
            configObj["sourceName"] = yggdrasilConfig.sourceName;
        }
        if (!yggdrasilConfig.authenticateEndpoint.isEmpty()) {
            configObj["authenticateEndpoint"] = yggdrasilConfig.authenticateEndpoint;
        }
        if (!yggdrasilConfig.refreshEndpoint.isEmpty()) {
            configObj["refreshEndpoint"] = yggdrasilConfig.refreshEndpoint;
        }
        if (!yggdrasilConfig.validateEndpoint.isEmpty()) {
            configObj["validateEndpoint"] = yggdrasilConfig.validateEndpoint;
        }
        if (!yggdrasilConfig.profileEndpoint.isEmpty()) {
            configObj["profileEndpoint"] = yggdrasilConfig.profileEndpoint;
        }
        if (!yggdrasilConfig.oauthTokenEndpoint.isEmpty()) {
            configObj["oauthTokenEndpoint"] = yggdrasilConfig.oauthTokenEndpoint;
        }
        // Save token type as string
        if (yggdrasilConfig.tokenType == YggdrasilTokenType::OAuth) {
            configObj["tokenType"] = "OAuth";
        } else {
            configObj["tokenType"] = "Standard";
        }
        output["yggdrasilConfig"] = configObj;
    } else if (type == AccountType::UnifiedPass) {
        output["type"] = "UnifiedPass";
        QJsonObject configObj;
        configObj["serverId"] = unifiedPassConfig.serverId;
        if (!unifiedPassConfig.baseUrl.isEmpty()) {
            configObj["baseUrl"] = unifiedPassConfig.baseUrl;
        }
        if (!unifiedPassConfig.serverName.isEmpty()) {
            configObj["serverName"] = unifiedPassConfig.serverName;
        }
        if (!unifiedPassConfig.serverIP.isEmpty()) {
            configObj["serverIP"] = unifiedPassConfig.serverIP;
        }
        if (!unifiedPassConfig.jarHash.isEmpty()) {
            configObj["jarHash"] = unifiedPassConfig.jarHash;
        }
        output["unifiedPassConfig"] = configObj;
    }

    // Save the appropriate token based on account type
    if (type == AccountType::UnifiedPass) {
        tokenToJSONV3(output, unifiedPassToken, "upass");
    } else {
        tokenToJSONV3(output, yggdrasilToken, "ygg");
    }
    profileToJSONV3(output, minecraftProfile, "profile");
    entitlementToJSONV3(output, minecraftEntitlement);
    return output;
}

QString AccountData::accessToken() const
{
    if (type == AccountType::UnifiedPass) {
        return unifiedPassToken.token;
    }
    return yggdrasilToken.token;
}

QString AccountData::profileId() const
{
    return minecraftProfile.id;
}

QString AccountData::profileName() const
{
    if (minecraftProfile.name.size() == 0) {
        return QObject::tr("No profile (%1)").arg(accountDisplayString());
    } else {
        return minecraftProfile.name;
    }
}

QString AccountData::accountDisplayString() const
{
    switch (type) {
        case AccountType::Offline: {
            return QObject::tr("<Offline>");
        }
        case AccountType::MSA: {
            if (xboxApiToken.extra.contains("gtg")) {
                return xboxApiToken.extra["gtg"].toString();
            }
            return "Xbox profile missing";
        }
        case AccountType::Yggdrasil: {
            QString displayName;
            if (yggdrasilToken.extra.contains("userName")) {
                displayName = yggdrasilToken.extra["userName"].toString();
            } else if (!minecraftProfile.name.isEmpty()) {
                displayName = minecraftProfile.name;
            } else {
                displayName = QObject::tr("<Yggdrasil>");
            }

            // Add token type indicator
            QString typeSuffix;
            if (yggdrasilConfig.tokenType == YggdrasilTokenType::OAuth) {
                typeSuffix = QObject::tr(" (OAuth)");
            } else {
                typeSuffix = QObject::tr(" (Standard)");
            }

            // Add source name if available and not empty
            if (!yggdrasilConfig.sourceName.isEmpty()) {
                return QString("%1 [%2]%3").arg(displayName, yggdrasilConfig.sourceName, typeSuffix);
            }
            return QString("%1%2").arg(displayName, typeSuffix);
        }
        case AccountType::UnifiedPass: {
            QString displayName;
            if (unifiedPassToken.extra.contains("userName")) {
                displayName = unifiedPassToken.extra["userName"].toString();
            } else if (!minecraftProfile.name.isEmpty()) {
                displayName = minecraftProfile.name;
            } else {
                displayName = QObject::tr("<UnifiedPass>");
            }

            // Add server name if available and not empty
            if (!unifiedPassConfig.serverName.isEmpty()) {
                return QString("%1 [%2]").arg(displayName, unifiedPassConfig.serverName);
            }
            return displayName;
        }
        default: {
            return "Invalid Account";
        }
    }
}

QString AccountData::lastError() const
{
    return errorString;
}
