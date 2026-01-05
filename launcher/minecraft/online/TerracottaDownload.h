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

#include "tasks/Task.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObjectPtr.h>

class TerracottaDownload : public Task {
    Q_OBJECT

   public:
    explicit TerracottaDownload(bool useMirror = false, QObject* parent = nullptr);
    ~TerracottaDownload() override = default;

    bool canAbort() const override { return true; }
    bool abort() override;

   protected:
    void executeTask() override;

   private slots:
    void downloadInfoFinished();
    void downloadExecutableFinished();

   private:
    void startExecutableDownload(const QString& url, const QString& version);

    QNetworkAccessManager* m_network;
    QNetworkReply* m_infoReply = nullptr;
    QNetworkReply* m_exeReply = nullptr;

    QString m_exePath;
    QString m_metadataPath;
    QString m_version;
    bool m_useMirror;
    bool m_isArchive = false;
    QJsonObject m_metadataObj;
};
