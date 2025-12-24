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

#include "YggdrasilPresets.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

static const QString CUSTOM_PRESETS_FILE = "yggdrasil-presets.json";

QVector<YggdrasilPreset> YggdrasilPresets::s_defaults;

void YggdrasilPresets::initDefaults()
{
    if (!s_defaults.isEmpty()) {
        return;
    }

    // LittleSkin - 国内常用的 Yggdrasil 皮肤站
    // LittleSkin uses /authserver/ prefix for refresh/validate, not /sessionserver/
    YggdrasilPreset littleSkin;
    littleSkin.name = QCoreApplication::translate("YggdrasilPresets", "LittleSkin");
    littleSkin.authUrl = "https://littleskin.cn/api/yggdrasil";
    littleSkin.sessionUrl = "https://littleskin.cn/api/yggdrasil";
    littleSkin.refreshEndpoint = "/authserver/refresh";
    littleSkin.validateEndpoint = "/authserver/validate";
    littleSkin.authenticateEndpoint = "/authserver/authenticate";
    littleSkin.profileEndpoint = "/sessionserver/session/minecraft/profile/{uuid}";
    s_defaults.append(littleSkin);

    // authlib-injector 官方示例服务器
    s_defaults.append({ QCoreApplication::translate("YggdrasilPresets", "authlib-injector Official Example"),
                        "https://authserver.localhost:23345/authserver",
                        "https://sessionserver.localhost:23345/sessionserver" });
}

const QVector<YggdrasilPreset>& YggdrasilPresets::getDefaults()
{
    initDefaults();
    return s_defaults;
}

static QString getPresetsFilePath()
{
    auto dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(dir).absoluteFilePath(CUSTOM_PRESETS_FILE);
}

static QJsonArray loadCustomPresets()
{
    QFile file(getPresetsFilePath());
    if (!file.exists()) {
        return QJsonArray();
    }

    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open Yggdrasil presets file for reading:" << file.errorString();
        return QJsonArray();
    }

    QJsonParseError error;
    auto doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse Yggdrasil presets file:" << error.errorString();
        return QJsonArray();
    }

    if (!doc.isArray()) {
        qWarning() << "Yggdrasil presets file root is not an array";
        return QJsonArray();
    }

    return doc.array();
}

static bool saveCustomPresets(const QJsonArray& array)
{
    QFile file(getPresetsFilePath());
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open Yggdrasil presets file for writing:" << file.errorString();
        return false;
    }

    QJsonDocument doc(array);
    file.write(doc.toJson());
    return true;
}

bool YggdrasilPresets::addCustomPreset(const YggdrasilPreset& preset)
{
    auto array = loadCustomPresets();

    // Check if a preset with the same name already exists
    for (const auto& value : array) {
        auto obj = value.toObject();
        if (obj.value("name").toString() == preset.name) {
            return false;
        }
    }

    QJsonObject obj;
    obj["name"] = preset.name;
    obj["authUrl"] = preset.authUrl;
    obj["sessionUrl"] = preset.sessionUrl;
    if (!preset.authenticateEndpoint.isEmpty()) {
        obj["authenticateEndpoint"] = preset.authenticateEndpoint;
    }
    if (!preset.refreshEndpoint.isEmpty()) {
        obj["refreshEndpoint"] = preset.refreshEndpoint;
    }
    if (!preset.validateEndpoint.isEmpty()) {
        obj["validateEndpoint"] = preset.validateEndpoint;
    }
    if (!preset.profileEndpoint.isEmpty()) {
        obj["profileEndpoint"] = preset.profileEndpoint;
    }
    if (!preset.oauthTokenEndpoint.isEmpty()) {
        obj["oauthTokenEndpoint"] = preset.oauthTokenEndpoint;
    }
    // Store token type as string for JSON serialization
    if (preset.tokenType == YggdrasilTokenType::OAuth) {
        obj["tokenType"] = "OAuth";
    } else {
        obj["tokenType"] = "Standard";
    }
    array.append(obj);

    return saveCustomPresets(array);
}

bool YggdrasilPresets::removeCustomPreset(const QString& name)
{
    auto array = loadCustomPresets();
    QJsonArray newArray;

    for (const auto& value : array) {
        auto obj = value.toObject();
        if (obj.value("name").toString() != name) {
            newArray.append(obj);
        }
    }

    if (array.size() == newArray.size()) {
        return false;  // Nothing was removed
    }

    return saveCustomPresets(newArray);
}

QVector<YggdrasilPreset> YggdrasilPresets::getCustomPresets()
{
    QVector<YggdrasilPreset> presets;
    auto array = loadCustomPresets();

    for (const auto& value : array) {
        auto obj = value.toObject();
        YggdrasilPreset preset;
        preset.name = obj.value("name").toString();
        preset.authUrl = obj.value("authUrl").toString();
        preset.sessionUrl = obj.value("sessionUrl").toString();
        preset.authenticateEndpoint = obj.value("authenticateEndpoint").toString();
        preset.refreshEndpoint = obj.value("refreshEndpoint").toString();
        preset.validateEndpoint = obj.value("validateEndpoint").toString();
        preset.profileEndpoint = obj.value("profileEndpoint").toString();
        preset.oauthTokenEndpoint = obj.value("oauthTokenEndpoint").toString();
        // Parse token type from string
        QString tokenTypeStr = obj.value("tokenType").toString();
        if (tokenTypeStr == "OAuth") {
            preset.tokenType = YggdrasilTokenType::OAuth;
        } else {
            preset.tokenType = YggdrasilTokenType::Standard;
        }
        presets.append(preset);
    }

    return presets;
}

QVector<YggdrasilPreset> YggdrasilPresets::getAllPresets()
{
    QVector<YggdrasilPreset> presets;
    presets.append(getDefaults());
    presets.append(getCustomPresets());
    return presets;
}
