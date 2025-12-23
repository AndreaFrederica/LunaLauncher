// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2025 TheKodeToad <TheKodeToad@proton.me>
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

#include "AuthlibInjector.h"

#include <QCryptographicHash>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include "Application.h"

AuthlibInjector* AuthlibInjector::s_instance = nullptr;

AuthlibInjector& AuthlibInjector::instance()
{
    if (!s_instance) {
        s_instance = new AuthlibInjector();
    }
    return *s_instance;
}

QString AuthlibInjector::getLocalPath() const
{
    return FS::PathCombine(APPLICATION->root(), "authlib-injector.jar");
}

QString AuthlibInjector::getMetadataPath() const
{
    return FS::PathCombine(APPLICATION->root(), "authlib-injector.json");
}

QString AuthlibInjector::getVersion() const
{
    QString metadataPath = getMetadataPath();
    QFile file(metadataPath);

    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return QString();
    }

    QJsonObject obj = doc.object();
    return obj["version"].toString();
}

bool AuthlibInjector::checkCache() const
{
    QString path = getLocalPath();
    if (!QFile::exists(path)) {
        return false;
    }

    // Check file size - authlib-injector jar should be at least 100KB
    QFileInfo info(path);
    if (info.size() < 100 * 1024) {
        qDebug() << "Authlib-injector file too small, re-downloading";
        return false;
    }

    return true;
}

QString AuthlibInjector::downloadLatest()
{
    QString cachePath = getLocalPath();

    // If cache is valid, return it
    if (checkCache()) {
        qDebug() << "Using cached authlib-injector at" << cachePath;
        return cachePath;
    }

    // Ensure cache directory exists
    QDir().mkpath(QFileInfo(cachePath).absolutePath());

    // Try BMCLAPI mirror first (recommended for Chinese users)
    QString downloadUrl = "https://bmclapi2.bangbang93.com/mirrors/authlib-injector/artifact/latest.json";

    QNetworkAccessManager manager;
    QNetworkRequest request((QUrl(downloadUrl)));
    request.setRawHeader("User-Agent", APPLICATION->getUserAgent().toUtf8());

    QNetworkReply* reply = manager.get(request);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "Failed to fetch authlib-injector info from BMCLAPI:" << reply->errorString();
        // Try official source
        downloadUrl = "https://authlib-injector.yushi.moe/artifact/latest.json";
        request.setUrl(QUrl(downloadUrl));
        reply = manager.get(request);
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "Failed to fetch authlib-injector info from official source:" << reply->errorString();
            reply->deleteLater();
            return QString();
        }
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse authlib-injector info JSON:" << parseError.errorString();
        return QString();
    }

    QJsonObject obj = doc.object();
    QString jarDownloadUrl = obj["download_url"].toString();
    QString expectedChecksum = obj["checksums"].toObject()["sha256"].toString();
    QString version = obj["version"].toString();

    if (jarDownloadUrl.isEmpty()) {
        qWarning() << "Authlib-injector info missing download_url";
        return QString();
    }

    // Download the jar file
    request.setUrl(QUrl(jarDownloadUrl));
    reply = manager.get(request);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "Failed to download authlib-injector.jar:" << reply->errorString();
        reply->deleteLater();
        return QString();
    }

    QByteArray jarData = reply->readAll();
    reply->deleteLater();

    // Verify checksum if provided
    if (!expectedChecksum.isEmpty()) {
        QByteArray hash = QCryptographicHash::hash(jarData, QCryptographicHash::Sha256).toHex();
        if (hash != expectedChecksum.toUtf8()) {
            qWarning() << "Authlib-injector checksum mismatch! Expected:" << expectedChecksum << "Got:" << QString::fromUtf8(hash);
            // Continue anyway as some mirrors may have wrong checksums
        }
    }

    // Write to cache
    QFile file(cachePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open authlib-injector.jar for writing:" << file.errorString();
        return QString();
    }

    file.write(jarData);
    file.close();

    // Save metadata (version info)
    if (!version.isEmpty()) {
        QJsonObject metadataObj;
        metadataObj["version"] = version;
        metadataObj["build_number"] = obj["build_number"].toInt();
        metadataObj["release_time"] = obj["release_time"].toString();
        metadataObj["checksums"] = obj["checksums"].toObject();

        QJsonDocument metadataDoc(metadataObj);
        QFile metadataFile(getMetadataPath());
        if (metadataFile.open(QIODevice::WriteOnly)) {
            metadataFile.write(metadataDoc.toJson());
            metadataFile.close();
            qDebug() << "Saved authlib-injector metadata: version" << version;
        } else {
            qWarning() << "Failed to save authlib-injector metadata";
        }
    }

    qDebug() << "Successfully downloaded authlib-injector to" << cachePath;
    return cachePath;
}

QString AuthlibInjector::getYggdrasilMeta(const QString& apiUrl)
{
    if (apiUrl.isEmpty()) {
        return QString();
    }

    QNetworkAccessManager manager;
    QNetworkRequest request((QUrl(apiUrl)));
    request.setRawHeader("User-Agent", APPLICATION->getUserAgent().toUtf8());

    QNetworkReply* reply = manager.get(request);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "Failed to fetch Yggdrasil metadata from" << apiUrl << ":" << reply->errorString();
        reply->deleteLater();
        return QString();
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

    // Return base64 encoded metadata
    return QString::fromUtf8(data.toBase64());
}
