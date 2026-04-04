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

#include "YggdrasilAuthStep.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>

#include "Application.h"
#include "minecraft/auth/Parsers.h"
#include "net/NetUtils.h"

YggdrasilAuthStep::YggdrasilAuthStep(AccountData* data, RequestType type) : AuthStep(data), m_requestType(type) {}

QString YggdrasilAuthStep::describe()
{
    switch (m_requestType) {
        case RequestType::Authenticate:
            return tr("Logging in with Yggdrasil account");
        case RequestType::Refresh:
            return tr("Refreshing Yggdrasil token");
        case RequestType::Validate:
            return tr("Validating Yggdrasil token");
        default:
            return QString();
    }
}

QUrl YggdrasilAuthStep::getAuthServerEndpoint(const QString& endpoint) const
{
    QString baseUrl = m_data->yggdrasilConfig.authServerUrl;
    if (baseUrl.endsWith('/')) {
        baseUrl.chop(1);
    }
    // Handle different API URL formats
    // If baseUrl already contains /authserver, don't add it again
    QString fullUrl = baseUrl;
    if (!baseUrl.endsWith("/authserver") && endpoint.startsWith("/authserver")) {
        // Standard Yggdrasil API format (like Mojang)
        fullUrl = baseUrl + endpoint;
    } else if (baseUrl.endsWith("/authserver") && !endpoint.startsWith("/authserver")) {
        // authlib-injector format with /authserver at the end
        fullUrl = baseUrl + endpoint;
    } else {
        // Direct format (like LittleSkin: https://littleskin.cn/api/yggdrasil)
        // endpoint should be like "/authenticate", not "/authserver/authenticate"
        fullUrl = baseUrl + endpoint;
    }
    return QUrl(fullUrl);
}

QUrl YggdrasilAuthStep::getSessionServerEndpoint(const QString& endpoint) const
{
    QString baseUrl = m_data->yggdrasilConfig.sessionServerUrl;
    if (baseUrl.endsWith('/')) {
        baseUrl.chop(1);
    }
    // Handle different API URL formats
    QString fullUrl = baseUrl;
    if (!baseUrl.endsWith("/sessionserver") && endpoint.startsWith("/sessionserver")) {
        // Standard Yggdrasil API format (like Mojang)
        fullUrl = baseUrl + endpoint;
    } else if (baseUrl.endsWith("/sessionserver") && !endpoint.startsWith("/sessionserver")) {
        // authlib-injector format with /sessionserver at the end
        fullUrl = baseUrl + endpoint;
    } else {
        // Direct format (like LittleSkin: https://littleskin.cn/api/yggdrasil)
        // endpoint should be like "/refresh", "/validate", etc.
        fullUrl = baseUrl + endpoint;
    }
    return QUrl(fullUrl);
}

void YggdrasilAuthStep::perform()
{
    QUrl url;
    QByteArray requestBody;

    switch (m_requestType) {
        case RequestType::Authenticate: {
            QString endpoint = m_data->yggdrasilConfig.authenticateEndpoint.isEmpty()
                ? "/authserver/authenticate"
                : m_data->yggdrasilConfig.authenticateEndpoint;
            url = getAuthServerEndpoint(endpoint);
            QString username = m_data->yggdrasilToken.extra["username"].toString();
            QString password = m_data->yggdrasilToken.extra["credentials"].toString();
            QString clientToken = m_data->yggdrasilToken.extra["clientToken"].toString();

            QJsonObject reqObj;
            QJsonObject agentObj;
            agentObj["name"] = "Minecraft";
            agentObj["version"] = 1;
            reqObj["agent"] = agentObj;
            reqObj["username"] = username;
            reqObj["password"] = password;
            if (!clientToken.isEmpty()) {
                reqObj["clientToken"] = clientToken;
            }
            reqObj["requestUser"] = true;

            requestBody = QJsonDocument(reqObj).toJson(QJsonDocument::Compact);

            qDebug() << "Yggdrasil authenticate request to:" << url.toString();
            qDebug() << "Username:" << username;
            qDebug() << "Password length:" << password.length();
            qDebug() << "ClientToken:" << clientToken;
            qDebug() << "Request body:" << QString::fromUtf8(requestBody);
            break;
        }
        case RequestType::Refresh: {
            QString accessToken = m_data->yggdrasilToken.token;
            QString clientToken = m_data->yggdrasilToken.extra["clientToken"].toString();

            // Check if we have a refresh_token available
            QString refreshToken = m_data->yggdrasilToken.extra["refreshToken"].toString();
            bool hasRefreshToken = !refreshToken.isEmpty();

            // For OAuth accounts with refresh_token, use OAuth refresh endpoint
            if (m_data->yggdrasilConfig.tokenType == YggdrasilTokenType::OAuth && hasRefreshToken) {
                // OAuth 2.0 token refresh with refresh_token
                QString endpoint = m_data->yggdrasilConfig.oauthTokenEndpoint.isEmpty()
                    ? "/oauth2/token"
                    : m_data->yggdrasilConfig.oauthTokenEndpoint;
                url = getAuthServerEndpoint(endpoint);

                // OAuth 2.0 refresh request format (form-encoded)
                requestBody = QString("grant_type=refresh_token&refresh_token=%1").arg(refreshToken).toUtf8();
                if (!clientToken.isEmpty()) {
                    requestBody += QString("&client_id=%1").arg(clientToken).toUtf8();
                }

                qDebug() << "OAuth refresh request to:" << url.toString();
                qDebug() << "Request body:" << QString::fromUtf8(requestBody);
            } else if (m_data->yggdrasilConfig.tokenType == YggdrasilTokenType::OAuth && !hasRefreshToken) {
                // OAuth without refresh_token (like LittleSkin) - use standard authenticate endpoint with password
                QString password = m_data->yggdrasilToken.extra["credentials"].toString();
                if (password.isEmpty()) {
                    qWarning() << "OAuth refresh failed: no refresh_token or credentials available";
                    emit finished(AccountTaskState::STATE_FAILED_HARD, tr("OAuth token expired, please re-login"));
                    return;
                }

                QString endpoint = m_data->yggdrasilConfig.authenticateEndpoint.isEmpty()
                    ? "/authserver/authenticate"
                    : m_data->yggdrasilConfig.authenticateEndpoint;
                url = getAuthServerEndpoint(endpoint);

                QString username = m_data->yggdrasilToken.extra["username"].toString();

                QJsonObject reqObj;
                QJsonObject agentObj;
                agentObj["name"] = "Minecraft";
                agentObj["version"] = 1;
                reqObj["agent"] = agentObj;
                reqObj["username"] = username;
                reqObj["password"] = password;
                if (!clientToken.isEmpty()) {
                    reqObj["clientToken"] = clientToken;
                }
                reqObj["requestUser"] = true;

                requestBody = QJsonDocument(reqObj).toJson(QJsonDocument::Compact);
                qDebug() << "OAuth re-authentication request to:" << url.toString();
                qDebug() << "Request body:" << QString::fromUtf8(requestBody);
            } else {
                // Standard Yggdrasil token refresh
                QString endpoint = m_data->yggdrasilConfig.refreshEndpoint.isEmpty()
                    ? "/sessionserver/refresh"
                    : m_data->yggdrasilConfig.refreshEndpoint;
                url = getSessionServerEndpoint(endpoint);

                QJsonObject reqObj;
                reqObj["accessToken"] = accessToken;
                if (!clientToken.isEmpty()) {
                    reqObj["clientToken"] = clientToken;
                }
                // Note: requestUser is NOT part of the standard Yggdrasil refresh endpoint
                // It's only used in authenticate endpoint

                requestBody = QJsonDocument(reqObj).toJson(QJsonDocument::Compact);
                qDebug() << "Yggdrasil refresh request to:" << url.toString();
                qDebug() << "Request body:" << QString::fromUtf8(requestBody);
            }
            qDebug() << "accessToken length:" << accessToken.length();
            qDebug() << "clientToken length:" << clientToken.length();
            break;
        }
        case RequestType::Validate: {
            QString endpoint = m_data->yggdrasilConfig.validateEndpoint.isEmpty()
                ? "/sessionserver/validate"
                : m_data->yggdrasilConfig.validateEndpoint;
            url = getSessionServerEndpoint(endpoint);
            QString accessToken = m_data->yggdrasilToken.token;

            QJsonObject reqObj;
            reqObj["accessToken"] = accessToken;
            if (!m_data->yggdrasilToken.extra["clientToken"].toString().isEmpty()) {
                reqObj["clientToken"] = m_data->yggdrasilToken.extra["clientToken"].toString();
            }

            requestBody = QJsonDocument(reqObj).toJson(QJsonDocument::Compact);
            break;
        }
    }

    // Determine Content-Type based on request type
    QByteArray contentType;
    if (m_requestType == RequestType::Refresh) {
        // Check if this is OAuth with refresh_token (uses form-encoded)
        QString refreshToken = m_data->yggdrasilToken.extra["refreshToken"].toString();
        bool hasRefreshToken = !refreshToken.isEmpty();
        bool isOAuthWithRefreshToken = m_data->yggdrasilConfig.tokenType == YggdrasilTokenType::OAuth && hasRefreshToken;

        if (isOAuthWithRefreshToken) {
            contentType = "application/x-www-form-urlencoded";
        } else {
            // Standard Yggdrasil or OAuth without refresh_token (uses authenticate endpoint with JSON)
            contentType = "application/json";
        }
    } else {
        contentType = "application/json";
    }

    auto headers = QList<Net::HeaderPair>{ { "Content-Type", contentType }, { "Accept", "application/json" } };

    auto [request, response] = Net::Upload::makeByteArray(url, requestBody);
    m_request = request;
    m_response = std::shared_ptr<QByteArray>(response, [](QByteArray*) {});
    m_request->addHeaderProxy(new Net::RawHeaderProxy(headers));

    m_task.reset(new NetJob("YggdrasilAuthStep", APPLICATION->network()));
    m_task->setAskRetry(false);
    m_task->addNetAction(m_request);

    connect(m_task.get(), &Task::finished, this, &YggdrasilAuthStep::onRequestDone);
    m_task->start();
}

void YggdrasilAuthStep::onRequestDone()
{
    if (m_request->error() != QNetworkReply::NoError) {
        if (m_request->error() == QNetworkReply::ContentNotFoundError) {
            // Validate request returns 204 No Content on success
            if (m_requestType == RequestType::Validate) {
                emit finished(AccountTaskState::STATE_WORKING, tr("Token is valid"));
                return;
            }
        }
        qWarning() << "Yggdrasil API request failed:" << m_request->errorString();
        emit finished(AccountTaskState::STATE_FAILED_SOFT,
                     tr("Yggdrasil authentication failed: %1").arg(m_request->errorString()));
        return;
    }

    // For validate, 204 No Content means success
    if (m_requestType == RequestType::Validate && m_request->replyStatusCode() == 204) {
        emit finished(AccountTaskState::STATE_WORKING, tr("Token is valid"));
        return;
    }

    QJsonParseError jsonError;
    QJsonDocument doc = QJsonDocument::fromJson(*m_response, &jsonError);

    if (jsonError.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse Yggdrasil response:" << jsonError.errorString();
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Failed to parse response"));
        return;
    }

    if (m_requestType == RequestType::Authenticate) {
        processAuthenticateResponse(*m_response);
    } else if (m_requestType == RequestType::Refresh) {
        processRefreshResponse(*m_response);
    }
}

void YggdrasilAuthStep::processAuthenticateResponse(QByteArray& data)
{
    QJsonParseError jsonError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &jsonError);

    if (jsonError.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse authenticate response:" << jsonError.errorString();
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Failed to parse authenticate response"));
        return;
    }

    auto obj = doc.object();

    // Debug: log the authenticate response to see what fields are present
    qDebug() << "Authenticate response keys:" << obj.keys();
    if (obj.contains("refresh_token")) {
        qDebug() << "Has refresh_token:" << obj.value("refresh_token").toString();
    }
    QJsonDocument debugDoc(obj);
    qDebug() << "Full authenticate response:" << QString::fromUtf8(debugDoc.toJson(QJsonDocument::Compact));

    // Check for error
    if (obj.contains("error")) {
        QString errorMessage = obj.value("errorMessage").toString();
        qWarning() << "Yggdrasil authenticate error:" << errorMessage;
        emit finished(AccountTaskState::STATE_FAILED_HARD, tr("Authentication failed: %1").arg(errorMessage));
        return;
    }

    // Save access token and client token
    QString accessToken = obj.value("accessToken").toString();
    m_data->yggdrasilToken.token = accessToken;
    m_data->yggdrasilToken.extra["clientToken"] = obj.value("clientToken").toString();

    // Detect if this is an OAuth JWT token and auto-configure
    if (accessToken.startsWith("eyJ")) {
        m_data->yggdrasilConfig.tokenType = YggdrasilTokenType::OAuth;
        // Save refresh_token if present
        if (obj.contains("refresh_token")) {
            m_data->yggdrasilToken.extra["refreshToken"] = obj.value("refresh_token").toString();
        }
        qDebug() << "Detected OAuth token, configured tokenType as OAuth";
    } else {
        m_data->yggdrasilConfig.tokenType = YggdrasilTokenType::Standard;
        qDebug() << "Detected Standard Yggdrasil token";
    }

    // Get user info
    auto userObj = obj.value("user").toObject();
    if (!userObj.isEmpty()) {
        m_data->yggdrasilToken.extra["userId"] = userObj.value("id").toString();
        m_data->yggdrasilToken.extra["userName"] = userObj.value("username").toString();
    }

    // Check available profiles
    auto availableProfiles = obj.value("availableProfiles").toArray();
    auto selectedProfile = obj.value("selectedProfile");

    // For OAuth accounts without refresh_token, keep the credentials for re-authentication
    // For standard Yggdrasil or OAuth with refresh_token, clear the credentials
    bool hasRefreshToken = m_data->yggdrasilToken.extra.contains("refreshToken") &&
                           !m_data->yggdrasilToken.extra["refreshToken"].toString().isEmpty();
    bool isOAuthWithoutRefreshToken = m_data->yggdrasilConfig.tokenType == YggdrasilTokenType::OAuth && !hasRefreshToken;

    if (!isOAuthWithoutRefreshToken) {
        // Clear credentials from memory after successful auth (only if we have refresh_token or using standard flow)
        m_data->yggdrasilToken.extra.remove("credentials");
    } else {
        qDebug() << "OAuth account without refresh_token - keeping credentials for re-authentication";
    }

    if (!selectedProfile.isUndefined() && !selectedProfile.toObject().isEmpty()) {
        // Server selected a profile for us
        auto profileObj = selectedProfile.toObject();
        m_data->minecraftProfile.id = profileObj.value("id").toString();
        m_data->minecraftProfile.name = profileObj.value("name").toString();
        m_data->minecraftProfile.validity = Validity::Certain;

        emit finished(AccountTaskState::STATE_WORKING, tr("Authentication succeeded"));
    } else if (availableProfiles.size() > 0) {
        // Multiple profiles available, need to select one
        // Store available profiles for selection step
        QJsonArray profilesArray;
        for (const auto& profileValue : availableProfiles) {
            profilesArray.append(profileValue);
        }
        m_data->yggdrasilToken.extra["availableProfiles"] = profilesArray;

        emit finished(AccountTaskState::STATE_WORKING, tr("Profile selection required"));
    } else {
        // No profiles available
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("No profiles available for this account"));
    }
}

void YggdrasilAuthStep::processRefreshResponse(QByteArray& data)
{
    QJsonParseError jsonError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &jsonError);

    if (jsonError.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse refresh response:" << jsonError.errorString();
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Failed to parse refresh response"));
        return;
    }

    auto obj = doc.object();

    // Check for error (OAuth uses "error" field, Yggdrasil uses "error" and "errorMessage")
    if (obj.contains("error")) {
        QString errorMessage = obj.value("error_description").toString();
        if (errorMessage.isEmpty()) {
            errorMessage = obj.value("errorMessage").toString();
        }
        if (errorMessage.isEmpty()) {
            errorMessage = obj.value("error").toString();
        }
        qWarning() << "Yggdrasil refresh error:" << errorMessage;
        emit finished(AccountTaskState::STATE_FAILED_HARD, tr("Token refresh failed: %1").arg(errorMessage));
        return;
    }

    // Check if this is an OAuth response (has access_token field instead of accessToken)
    QString newAccessToken;
    if (obj.contains("access_token")) {
        // OAuth 2.0 response format
        newAccessToken = obj.value("access_token").toString();
        m_data->yggdrasilToken.token = newAccessToken;
        // Update refresh token if provided
        if (obj.contains("refresh_token")) {
            m_data->yggdrasilToken.extra["refreshToken"] = obj.value("refresh_token").toString();
        }
        // OAuth responses don't include profile info, so we keep the existing one
    } else {
        // Standard Yggdrasil response format (or authenticate endpoint response)
        newAccessToken = obj.value("accessToken").toString();
        m_data->yggdrasilToken.token = newAccessToken;

        // Update clientToken if present (authenticate endpoint returns it)
        if (obj.contains("clientToken")) {
            m_data->yggdrasilToken.extra["clientToken"] = obj.value("clientToken").toString();
        }

        // Get selected profile
        auto selectedProfile = obj.value("selectedProfile");
        if (!selectedProfile.isUndefined() && !selectedProfile.toObject().isEmpty()) {
            auto profileObj = selectedProfile.toObject();
            m_data->minecraftProfile.id = profileObj.value("id").toString();
            m_data->minecraftProfile.name = profileObj.value("name").toString();
            m_data->minecraftProfile.validity = Validity::Certain;
        }

        // Update user info if present (authenticate endpoint returns it)
        auto userObj = obj.value("user").toObject();
        if (!userObj.isEmpty()) {
            m_data->yggdrasilToken.extra["userId"] = userObj.value("id").toString();
            m_data->yggdrasilToken.extra["userName"] = userObj.value("username").toString();
        }
    }

    m_data->yggdrasilToken.validity = Validity::Certain;
    emit finished(AccountTaskState::STATE_WORKING, tr("Token refreshed"));
}
