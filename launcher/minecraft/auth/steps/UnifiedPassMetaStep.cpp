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

#include "UnifiedPassMetaStep.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>

#include "Application.h"
#include "net/NetUtils.h"

UnifiedPassMetaStep::UnifiedPassMetaStep(AccountData* data) : AuthStep(data) {}

QString UnifiedPassMetaStep::describe()
{
    return tr("Fetching UnifiedPass server metadata");
}

QUrl UnifiedPassMetaStep::getBaseUrl() const
{
    // If baseUrl is already set from a previous fetch, use it
    if (!m_data->unifiedPassConfig.baseUrl.isEmpty()) {
        QString baseUrl = m_data->unifiedPassConfig.baseUrl;
        if (baseUrl.endsWith('/')) {
            baseUrl.chop(1);
        }
        return QUrl(baseUrl);
    }

    // Construct default UnifiedPass API URL
    QString serverId = m_data->unifiedPassConfig.serverId;
    QString baseUrl = QString("https://auth.mc-user.com:233/%1/").arg(serverId);
    return QUrl(baseUrl);
}

void UnifiedPassMetaStep::perform()
{
    QUrl url = getBaseUrl();

    qDebug() << "UnifiedPass meta request to:" << url.toString();

    auto headers = QList<Net::HeaderPair>{ { "Accept", "application/json" } };

    auto [request, response] = Net::Download::makeByteArray(url);
    m_request = request;
    m_response = std::shared_ptr<QByteArray>(response, [](QByteArray*) {});
    m_request->addHeaderProxy(new Net::RawHeaderProxy(headers));

    m_task.reset(new NetJob("UnifiedPassMetaStep", APPLICATION->network()));
    m_task->setAskRetry(false);
    m_task->addNetAction(m_request);

    connect(m_task.get(), &Task::finished, this, &UnifiedPassMetaStep::onRequestDone);
    m_task->start();
}

void UnifiedPassMetaStep::onRequestDone()
{
    if (m_request->error() != QNetworkReply::NoError) {
        qWarning() << "UnifiedPass meta request failed:" << m_request->errorString();
        // Continue with default URL even if meta fetch fails
        emit finished(AccountTaskState::STATE_WORKING, tr("Using default UnifiedPass configuration"));
        return;
    }

    processMetaResponse(*m_response);
}

void UnifiedPassMetaStep::processMetaResponse(QByteArray& data)
{
    QJsonParseError jsonError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &jsonError);

    if (jsonError.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse UnifiedPass meta response:" << jsonError.errorString();
        // Continue with default URL
        emit finished(AccountTaskState::STATE_WORKING, tr("Using default UnifiedPass configuration"));
        return;
    }

    auto obj = doc.object();

    qDebug() << "UnifiedPass meta response:" << QString::fromUtf8(data);

    // Get meta object
    auto metaObj = obj.value("meta").toObject();
    if (!metaObj.isEmpty()) {
        // Get server name
        QString serverName = metaObj.value("serverName").toString();
        if (!serverName.isEmpty()) {
            m_data->unifiedPassConfig.serverName = serverName;
            qDebug() << "UnifiedPass server name:" << serverName;
        }

        // Get server IP (optional)
        QString serverIP = metaObj.value("serverIP").toString();
        if (!serverIP.isEmpty()) {
            m_data->unifiedPassConfig.serverIP = serverIP;
            qDebug() << "UnifiedPass server IP:" << serverIP;
        }
    }

    // apiRoot and jarHash are at the top level, not in meta
    QString apiRoot = obj.value("apiRoot").toString();
    if (!apiRoot.isEmpty()) {
        m_data->unifiedPassConfig.baseUrl = apiRoot;
        qDebug() << "Updated UnifiedPass baseUrl from meta:" << apiRoot;
    }

    // Get jarHash for update checking (optional)
    QString jarHash = obj.value("jarHash").toString();
    if (!jarHash.isEmpty()) {
        m_data->unifiedPassConfig.jarHash = jarHash;
        qDebug() << "UnifiedPass jar hash:" << jarHash;
    }

    emit finished(AccountTaskState::STATE_WORKING, tr("Server metadata fetched"));
}
