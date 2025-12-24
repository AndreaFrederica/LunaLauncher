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
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObjectPtr.h>

class Nide8AuthDownload : public Task {
    Q_OBJECT

   public:
    explicit Nide8AuthDownload(QObject* parent = nullptr);
    ~Nide8AuthDownload() override = default;

    bool canAbort() const override { return true; }
    bool abort() override;

   protected:
    void executeTask() override;

   private slots:
    void downloadJarFinished();

   private:
    QNetworkAccessManager* m_network;
    QNetworkReply* m_jarReply = nullptr;

    QString m_jarPath;
};
