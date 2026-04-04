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

#include "YggdrasilProfileStep.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>

#include "Application.h"
#include "minecraft/auth/Parsers.h"

YggdrasilProfileStep::YggdrasilProfileStep(AccountData* data) : AuthStep(data) {}

QString YggdrasilProfileStep::describe()
{
    return tr("Fetching Yggdrasil profile");
}

QUrl YggdrasilProfileStep::getSessionServerEndpoint(const QString& endpoint) const
{
    QString baseUrl;

    // For UnifiedPass, use the UnifiedPass baseUrl
    if (m_data->type == AccountType::UnifiedPass) {
        baseUrl = m_data->unifiedPassConfig.baseUrl;
    } else {
        baseUrl = m_data->yggdrasilConfig.sessionServerUrl;
    }

    if (baseUrl.endsWith('/')) {
        baseUrl.chop(1);
    }
    // Handle different API URL formats
    // If baseUrl already contains /sessionserver, don't add it again
    QString fullUrl = baseUrl;
    if (!baseUrl.endsWith("/sessionserver") && endpoint.startsWith("/sessionserver")) {
        // Standard Yggdrasil API format (like Mojang)
        fullUrl = baseUrl + endpoint;
    } else if (baseUrl.endsWith("/sessionserver") && !endpoint.startsWith("/sessionserver")) {
        // authlib-injector format with /sessionserver at the end
        fullUrl = baseUrl + endpoint;
    } else {
        // Direct format (like LittleSkin: https://littleskin.cn/api/yggdrasil)
        // endpoint should be like "/sessionserver/session/minecraft/profile/{uuid}"
        fullUrl = baseUrl + endpoint;
    }
    return QUrl(fullUrl);
}

void YggdrasilProfileStep::perform()
{
    QString profileId = m_data->minecraftProfile.id;
    if (profileId.isEmpty()) {
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("No profile ID available"));
        return;
    }

    QString endpointTemplate = m_data->yggdrasilConfig.profileEndpoint.isEmpty()
        ? "/sessionserver/session/minecraft/profile/{uuid}"
        : m_data->yggdrasilConfig.profileEndpoint;
    QString endpoint = endpointTemplate.replace("{uuid}", profileId);
    QUrl url = getSessionServerEndpoint(endpoint);

    qDebug() << "YggdrasilProfileStep: Fetching profile from" << url.toString();

    auto headers = QList<Net::HeaderPair>{ { "Accept", "application/json" } };

    auto [request, response] = Net::Download::makeByteArray(url);
    m_request = request;
    m_response = std::shared_ptr<QByteArray>(response, [](QByteArray*) {});
    m_request->addHeaderProxy(new Net::RawHeaderProxy(headers));

    m_task.reset(new NetJob("YggdrasilProfileStep", APPLICATION->network()));
    m_task->setAskRetry(false);
    m_task->addNetAction(m_request);

    connect(m_task.get(), &Task::finished, this, &YggdrasilProfileStep::onRequestDone);
    m_task->start();
}

void YggdrasilProfileStep::onRequestDone()
{
    if (m_request->error() == QNetworkReply::ContentNotFoundError) {
        // Profile not found - this can happen if the profile was deleted
        // We'll keep the basic profile info from auth
        emit finished(AccountTaskState::STATE_WORKING, tr("Using basic profile information"));
        return;
    }

    if (m_request->error() != QNetworkReply::NoError) {
        qWarning() << "Yggdrasil profile request failed:" << m_request->errorString();
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Failed to fetch profile: %1").arg(m_request->errorString()));
        return;
    }

    // Parse response for skin/cape information
    QJsonParseError jsonError;
    QJsonDocument doc = QJsonDocument::fromJson(*m_response, &jsonError);

    if (jsonError.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse profile response:" << jsonError.errorString();
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Failed to parse profile response"));
        return;
    }

    auto obj = doc.object();

    // Check for error
    if (obj.contains("error")) {
        QString errorMessage = obj.value("errorMessage").toString();
        qWarning() << "Yggdrasil profile error:" << errorMessage;
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Profile error: %1").arg(errorMessage));
        return;
    }

    // Update profile ID and name (in case they changed)
    m_data->minecraftProfile.id = obj.value("id").toString();
    m_data->minecraftProfile.name = obj.value("name").toString();

    // Parse properties for skin/cape info
    auto propertiesArray = obj.value("properties").toArray();
    for (const auto& propertyValue : propertiesArray) {
        auto propertyObj = propertyValue.toObject();
        QString name = propertyObj.value("name").toString();
        QString value = propertyObj.value("value").toString();

        if (name == "textures") {
            // Decode base64 and parse textures
            QByteArray texturesData = QByteArray::fromBase64(value.toUtf8());
            QJsonParseError texturesError;
            QJsonDocument texturesDoc = QJsonDocument::fromJson(texturesData, &texturesError);

            if (texturesError.error == QJsonParseError::NoError) {
                auto texturesObj = texturesDoc.object();
                auto textures = texturesObj.value("textures").toObject();

                // Parse skin
                auto skinObj = textures.value("SKIN").toObject();
                if (!skinObj.isEmpty()) {
                    m_data->minecraftProfile.skin.url = skinObj.value("url").toString();
                    auto metadata = skinObj.value("metadata").toObject();
                    if (metadata.value("model").toString() == "slim") {
                        m_data->minecraftProfile.skin.variant = "slim";
                    } else {
                        m_data->minecraftProfile.skin.variant = "classic";
                    }
                }

                // Parse cape
                auto capeObj = textures.value("CAPE").toObject();
                if (!capeObj.isEmpty()) {
                    Cape cape;
                    cape.url = capeObj.value("url").toString();
                    cape.alias = "cape";
                    m_data->minecraftProfile.capes[cape.alias] = cape;
                    m_data->minecraftProfile.currentCape = cape.alias;
                }
            }
        }
    }

    m_data->minecraftProfile.validity = Validity::Certain;
    emit finished(AccountTaskState::STATE_WORKING, tr("Profile fetched"));
}
