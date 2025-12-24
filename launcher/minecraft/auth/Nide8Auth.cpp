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

#include "Nide8Auth.h"

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include "Application.h"

Nide8Auth* Nide8Auth::s_instance = nullptr;

Nide8Auth& Nide8Auth::instance()
{
    if (!s_instance) {
        s_instance = new Nide8Auth();
    }
    return *s_instance;
}

QString Nide8Auth::getLocalPath() const
{
    return FS::PathCombine(APPLICATION->root(), "nide8auth.jar");
}

bool Nide8Auth::checkCache() const
{
    QString path = getLocalPath();
    if (!QFile::exists(path)) {
        return false;
    }

    // Check file size - nide8auth jar should be at least 50KB
    QFileInfo info(path);
    if (info.size() < 50 * 1024) {
        qDebug() << "Nide8Auth file too small, re-downloading";
        return false;
    }

    return true;
}

QString Nide8Auth::download()
{
    QString cachePath = getLocalPath();

    // If cache is valid, return it
    if (checkCache()) {
        qDebug() << "Using cached nide8auth.jar at" << cachePath;
        return cachePath;
    }

    // Ensure cache directory exists
    QDir().mkpath(QFileInfo(cachePath).absolutePath());

    // TODO: This is a non-official mirror link. Replace with official source when available.
    // Current source: https://gitee.com/LostCityTeam/LostCity-Server-Resources/raw/master/nide8auth.jar
    QString downloadUrl = "https://gitee.com/LostCityTeam/LostCity-Server-Resources/raw/master/nide8auth.jar";

    QNetworkAccessManager manager;
    QNetworkRequest request((QUrl(downloadUrl)));
    request.setRawHeader("User-Agent", APPLICATION->getUserAgent().toUtf8());

    QNetworkReply* reply = manager.get(request);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "Failed to download nide8auth.jar:" << reply->errorString();
        reply->deleteLater();
        return QString();
    }

    QByteArray jarData = reply->readAll();
    reply->deleteLater();

    // Check if we got any data
    if (jarData.isEmpty()) {
        qWarning() << "Downloaded nide8auth.jar is empty";
        return QString();
    }

    // Write to cache
    QFile file(cachePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open nide8auth.jar for writing:" << file.errorString();
        return QString();
    }

    file.write(jarData);
    file.close();

    qDebug() << "Successfully downloaded nide8auth.jar to" << cachePath;
    return cachePath;
}
