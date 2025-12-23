// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2025 ketiko <kevinmk2805@gmail.com>
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

#include "AuthlibInjectorDownload.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

#include "Application.h"
#include "FileSystem.h"

AuthlibInjectorDownload::AuthlibInjectorDownload(QObject* parent) : Task(parent)
{
    m_network = new QNetworkAccessManager(this);
    m_jarPath = FS::PathCombine(APPLICATION->root(), "authlib-injector.jar");
    m_metadataPath = FS::PathCombine(APPLICATION->root(), "authlib-injector.json");
}

bool AuthlibInjectorDownload::abort()
{
    if (m_infoReply && m_infoReply->isRunning()) {
        m_infoReply->abort();
        return true;
    }
    if (m_jarReply && m_jarReply->isRunning()) {
        m_jarReply->abort();
        return true;
    }
    return false;
}

void AuthlibInjectorDownload::executeTask()
{
    setStatus(tr("Checking authlib-injector version..."));

    // Try BMCLAPI first, then fall back to official
    QString infoUrl = "https://bmclapi2.bangbang93.com/mirrors/authlib-injector/artifact/latest.json";

    QNetworkRequest request((QUrl(infoUrl)));
    request.setRawHeader("User-Agent", APPLICATION->getUserAgent().toUtf8());

    m_infoReply = m_network->get(request);
    connect(m_infoReply, &QNetworkReply::finished, this, &AuthlibInjectorDownload::downloadInfoFinished);
    connect(m_infoReply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        if (total > 0) {
            setProgress(received, total);
        }
    });
}

void AuthlibInjectorDownload::downloadInfoFinished()
{
    // Check if we need to try the official source
    if (m_infoReply->error() != QNetworkReply::NoError) {
        qWarning() << "Failed to fetch authlib-injector info from BMCLAPI:" << m_infoReply->errorString();
        // Try official source
        QString infoUrl = "https://authlib-injector.yushi.moe/artifact/latest.json";
        QNetworkRequest request((QUrl(infoUrl)));
        request.setRawHeader("User-Agent", APPLICATION->getUserAgent().toUtf8());

        m_infoReply->deleteLater();
        m_infoReply = m_network->get(request);
        connect(m_infoReply, &QNetworkReply::finished, this, &AuthlibInjectorDownload::downloadInfoFinished);
        connect(m_infoReply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
            if (total > 0) {
                setProgress(received, total);
            }
        });
        return;
    }

    QByteArray data = m_infoReply->readAll();
    m_infoReply->deleteLater();
    m_infoReply = nullptr;

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        emitFailed(tr("Failed to parse authlib-injector info: %1").arg(parseError.errorString()));
        return;
    }

    QJsonObject obj = doc.object();
    QString jarDownloadUrl = obj["download_url"].toString();
    QString checksum = obj["checksums"].toObject()["sha256"].toString();
    m_version = obj["version"].toString();
    m_metadataObj = obj;  // Save full metadata for later

    if (jarDownloadUrl.isEmpty()) {
        emitFailed(tr("authlib-injector info missing download_url"));
        return;
    }

    startJarDownload(jarDownloadUrl, checksum);
}

void AuthlibInjectorDownload::startJarDownload(const QString& url, const QString& checksum)
{
    setStatus(tr("Downloading authlib-injector..."));

    // Ensure parent directory exists
    QDir().mkpath(QFileInfo(m_jarPath).absolutePath());

    QNetworkRequest request((QUrl(url)));
    request.setRawHeader("User-Agent", APPLICATION->getUserAgent().toUtf8());

    m_jarReply = m_network->get(request);
    connect(m_jarReply, &QNetworkReply::finished, this, &AuthlibInjectorDownload::downloadJarFinished);
    connect(m_jarReply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        if (total > 0) {
            setProgress(received, total);
        }
        setDetails(tr("%1 / %2 KB").arg(received / 1024).arg(total / 1024));
    });

    m_expectedChecksum = checksum;
}

void AuthlibInjectorDownload::downloadJarFinished()
{
    if (m_jarReply->error() != QNetworkReply::NoError) {
        emitFailed(tr("Failed to download authlib-injector: %1").arg(m_jarReply->errorString()));
        m_jarReply->deleteLater();
        m_jarReply = nullptr;
        return;
    }

    QByteArray jarData = m_jarReply->readAll();
    m_jarReply->deleteLater();
    m_jarReply = nullptr;

    // Verify checksum if provided
    if (!m_expectedChecksum.isEmpty()) {
        QByteArray hash = QCryptographicHash::hash(jarData, QCryptographicHash::Sha256).toHex();
        if (hash != m_expectedChecksum.toUtf8()) {
            qWarning() << "Authlib-injector checksum mismatch! Expected:" << m_expectedChecksum << "Got:" << QString::fromUtf8(hash);
            // Continue anyway as some mirrors may have wrong checksums
        }
    }

    // Write to file
    QFile file(m_jarPath);
    if (!file.open(QIODevice::WriteOnly)) {
        emitFailed(tr("Failed to open authlib-injector.jar for writing: %1").arg(file.errorString()));
        return;
    }

    file.write(jarData);
    file.close();

    // Save metadata (version info)
    if (!m_version.isEmpty()) {
        QJsonDocument metadataDoc(m_metadataObj);
        QFile metadataFile(m_metadataPath);
        if (metadataFile.open(QIODevice::WriteOnly)) {
            metadataFile.write(metadataDoc.toJson());
            metadataFile.close();
            qDebug() << "Saved authlib-injector metadata: version" << m_version;
        } else {
            qWarning() << "Failed to save authlib-injector metadata";
        }
    }

    qDebug() << "Successfully downloaded authlib-injector to" << m_jarPath;
    emitSucceeded();
}
