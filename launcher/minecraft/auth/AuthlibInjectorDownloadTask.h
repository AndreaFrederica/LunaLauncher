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

#pragma once

#include "launch/LaunchStep.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObjectPtr.h>

class AuthlibInjectorDownloadTask : public LaunchStep {
    Q_OBJECT

   public:
    explicit AuthlibInjectorDownloadTask(LaunchTask* parent);
    ~AuthlibInjectorDownloadTask() override = default;

    bool canAbort() const override { return true; }
    bool abort() override;

   protected:
    void executeTask() override;

   private slots:
    void downloadInfoFinished();
    void downloadJarFinished();

   private:
    void startJarDownload(const QString& url, const QString& checksum);

    shared_qobject_ptr<QNetworkAccessManager> m_network;
    QNetworkReply* m_infoReply = nullptr;
    QNetworkReply* m_jarReply = nullptr;

    QString m_jarPath;
    QString m_metadataPath;
    QString m_expectedChecksum;
    QString m_version;
    QJsonObject m_metadataObj;
    qint64 m_totalSize = 0;
    qint64 m_downloadedSize = 0;
};
