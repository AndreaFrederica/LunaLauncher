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

#include "UnifiedPassAuthStep.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>

#include "Application.h"
#include "minecraft/auth/Parsers.h"
#include "net/NetUtils.h"

UnifiedPassAuthStep::UnifiedPassAuthStep(AccountData* data, RequestType type) : AuthStep(data), m_requestType(type) {}

QString UnifiedPassAuthStep::describe()
{
    switch (m_requestType) {
        case RequestType::Authenticate:
            return tr("Logging in with UnifiedPass account");
        case RequestType::Refresh:
            return tr("Refreshing UnifiedPass token");
        case RequestType::Validate:
            return tr("Validating UnifiedPass token");
        default:
            return QString();
    }
}

QUrl UnifiedPassAuthStep::getEndpoint(const QString& endpoint) const
{
    QString baseUrl = m_data->unifiedPassConfig.baseUrl;
    if (baseUrl.isEmpty()) {
        // Fallback to default UnifiedPass API URL format
        baseUrl = QString("https://auth.mc-user.com:233/%1/").arg(m_data->unifiedPassConfig.serverId);
    }
    // Remove trailing slash from baseUrl if present
    if (baseUrl.endsWith('/')) {
        baseUrl.chop(1);
    }
    // Ensure endpoint starts with /
    QString ep = endpoint;
    if (!ep.startsWith('/')) {
        ep = '/' + ep;
    }
    return QUrl(baseUrl + ep);
}

void UnifiedPassAuthStep::perform()
{
    QUrl url;
    QByteArray requestBody;

    switch (m_requestType) {
        case RequestType::Authenticate: {
            url = getEndpoint("authserver/authenticate");
            QString username = m_data->unifiedPassToken.extra["username"].toString();
            QString password = m_data->unifiedPassToken.extra["credentials"].toString();
            QString clientToken = m_data->unifiedPassToken.extra["clientToken"].toString();

            QJsonObject reqObj;
            QJsonObject agentObj;
            agentObj["name"] = "Luna Launcher";
            agentObj["version"] = 1;
            reqObj["agent"] = agentObj;
            reqObj["username"] = username;
            reqObj["password"] = password;
            if (!clientToken.isEmpty()) {
                reqObj["clientToken"] = clientToken;
            }
            reqObj["requestUser"] = true;

            requestBody = QJsonDocument(reqObj).toJson(QJsonDocument::Compact);

            qDebug() << "UnifiedPass authenticate request to:" << url.toString();
            qDebug() << "Username:" << username;
            qDebug() << "Request body:" << QString::fromUtf8(requestBody);
            break;
        }
        case RequestType::Refresh: {
            url = getEndpoint("authserver/refresh");
            QString accessToken = m_data->unifiedPassToken.token;
            QString clientToken = m_data->unifiedPassToken.extra["clientToken"].toString();

            QJsonObject reqObj;
            reqObj["accessToken"] = accessToken;
            if (!clientToken.isEmpty()) {
                reqObj["clientToken"] = clientToken;
            }
            reqObj["requestUser"] = true;

            requestBody = QJsonDocument(reqObj).toJson(QJsonDocument::Compact);

            qDebug() << "UnifiedPass refresh request to:" << url.toString();
            qDebug() << "Request body:" << QString::fromUtf8(requestBody);
            break;
        }
        case RequestType::Validate: {
            url = getEndpoint("authserver/validate");
            QString accessToken = m_data->unifiedPassToken.token;
            QString clientToken = m_data->unifiedPassToken.extra["clientToken"].toString();

            QJsonObject reqObj;
            reqObj["accessToken"] = accessToken;
            if (!clientToken.isEmpty()) {
                reqObj["clientToken"] = clientToken;
            }

            requestBody = QJsonDocument(reqObj).toJson(QJsonDocument::Compact);

            qDebug() << "UnifiedPass validate request to:" << url.toString();
            qDebug() << "Request body:" << QString::fromUtf8(requestBody);
            break;
        }
    }

    auto headers = QList<Net::HeaderPair>{ { "Content-Type", "application/json" }, { "Accept", "application/json" } };

    auto [request, response] = Net::Upload::makeByteArray(url, requestBody);
    m_request = request;
    m_response = std::shared_ptr<QByteArray>(response, [](QByteArray*) {});
    m_request->addHeaderProxy(new Net::RawHeaderProxy(headers));

    m_task.reset(new NetJob("UnifiedPassAuthStep", APPLICATION->network()));
    m_task->setAskRetry(false);
    m_task->addNetAction(m_request);

    connect(m_task.get(), &Task::finished, this, &UnifiedPassAuthStep::onRequestDone);
    m_task->start();
}

void UnifiedPassAuthStep::onRequestDone()
{
    if (m_request->error() != QNetworkReply::NoError) {
        if (m_request->error() == QNetworkReply::ContentNotFoundError) {
            // Validate request returns 204 No Content on success
            if (m_requestType == RequestType::Validate) {
                emit finished(AccountTaskState::STATE_WORKING, tr("Token is valid"));
                return;
            }
        }
        qWarning() << "UnifiedPass API request failed:" << m_request->errorString();
        emit finished(AccountTaskState::STATE_FAILED_SOFT,
                     tr("UnifiedPass authentication failed: %1").arg(m_request->errorString()));
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
        qWarning() << "Failed to parse UnifiedPass response:" << jsonError.errorString();
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Failed to parse response"));
        return;
    }

    if (m_requestType == RequestType::Authenticate) {
        processAuthenticateResponse(*m_response);
    } else if (m_requestType == RequestType::Refresh) {
        processRefreshResponse(*m_response);
    }
}

void UnifiedPassAuthStep::processAuthenticateResponse(QByteArray& data)
{
    QJsonParseError jsonError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &jsonError);

    if (jsonError.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse authenticate response:" << jsonError.errorString();
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Failed to parse authenticate response"));
        return;
    }

    auto obj = doc.object();

    qDebug() << "UnifiedPass authenticate response:" << QString::fromUtf8(data);

    // Check for error
    if (obj.contains("error")) {
        QString errorMessage = obj.value("errorMessage").toString();
        if (errorMessage.isEmpty()) {
            errorMessage = obj.value("error").toString();
        }
        qWarning() << "UnifiedPass authenticate error:" << errorMessage;
        emit finished(AccountTaskState::STATE_FAILED_HARD, tr("Authentication failed: %1").arg(errorMessage));
        return;
    }

    // Save access token and client token
    QString accessToken = obj.value("accessToken").toString();
    m_data->unifiedPassToken.token = accessToken;
    m_data->unifiedPassToken.extra["clientToken"] = obj.value("clientToken").toString();

    // Get user info
    auto userObj = obj.value("user").toObject();
    if (!userObj.isEmpty()) {
        m_data->unifiedPassToken.extra["userId"] = userObj.value("id").toString();
        m_data->unifiedPassToken.extra["userName"] = userObj.value("username").toString();
    }

    // Clear credentials from memory after successful auth (security best practice)
    m_data->unifiedPassToken.extra.remove("credentials");

    // Check available profiles
    auto availableProfiles = obj.value("availableProfiles").toArray();
    auto selectedProfile = obj.value("selectedProfile");

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
        m_data->unifiedPassToken.extra["availableProfiles"] = profilesArray;

        emit finished(AccountTaskState::STATE_WORKING, tr("Profile selection required"));
    } else {
        // No profiles available
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("No profiles available for this account"));
    }
}

void UnifiedPassAuthStep::processRefreshResponse(QByteArray& data)
{
    QJsonParseError jsonError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &jsonError);

    if (jsonError.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse refresh response:" << jsonError.errorString();
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Failed to parse refresh response"));
        return;
    }

    auto obj = doc.object();

    // Check for error
    if (obj.contains("error")) {
        QString errorMessage = obj.value("errorMessage").toString();
        if (errorMessage.isEmpty()) {
            errorMessage = obj.value("error").toString();
        }
        qWarning() << "UnifiedPass refresh error:" << errorMessage;
        emit finished(AccountTaskState::STATE_FAILED_HARD, tr("Token refresh failed: %1").arg(errorMessage));
        return;
    }

    // Save new access token
    QString newAccessToken = obj.value("accessToken").toString();
    m_data->unifiedPassToken.token = newAccessToken;

    // Update clientToken if present
    if (obj.contains("clientToken")) {
        m_data->unifiedPassToken.extra["clientToken"] = obj.value("clientToken").toString();
    }

    // Get selected profile
    auto selectedProfile = obj.value("selectedProfile");
    if (!selectedProfile.isUndefined() && !selectedProfile.toObject().isEmpty()) {
        auto profileObj = selectedProfile.toObject();
        m_data->minecraftProfile.id = profileObj.value("id").toString();
        m_data->minecraftProfile.name = profileObj.value("name").toString();
        m_data->minecraftProfile.validity = Validity::Certain;
    }

    // Update user info if present
    auto userObj = obj.value("user").toObject();
    if (!userObj.isEmpty()) {
        m_data->unifiedPassToken.extra["userId"] = userObj.value("id").toString();
        m_data->unifiedPassToken.extra["userName"] = userObj.value("username").toString();
    }

    m_data->unifiedPassToken.validity = Validity::Certain;
    emit finished(AccountTaskState::STATE_WORKING, tr("Token refreshed"));
}
