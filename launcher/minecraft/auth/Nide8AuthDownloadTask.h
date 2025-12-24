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
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObjectPtr.h>

class Nide8AuthDownloadTask : public LaunchStep {
    Q_OBJECT

   public:
    explicit Nide8AuthDownloadTask(LaunchTask* parent);
    ~Nide8AuthDownloadTask() override = default;

    bool canAbort() const override { return true; }
    bool abort() override;

   protected:
    void executeTask() override;

   private slots:
    void downloadJarFinished();

   private:
    shared_qobject_ptr<QNetworkAccessManager> m_network;
    QNetworkReply* m_jarReply = nullptr;

    QString m_jarPath;
    qint64 m_totalSize = 0;
};
