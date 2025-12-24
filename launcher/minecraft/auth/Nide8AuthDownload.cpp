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

#include "Nide8AuthDownload.h"

#include <QDir>
#include <QFileInfo>

#include "Application.h"
#include "FileSystem.h"

Nide8AuthDownload::Nide8AuthDownload(QObject* parent) : Task(parent)
{
    m_network = new QNetworkAccessManager(this);
    m_jarPath = FS::PathCombine(APPLICATION->root(), "nide8auth.jar");
}

bool Nide8AuthDownload::abort()
{
    if (m_jarReply && m_jarReply->isRunning()) {
        m_jarReply->abort();
        return true;
    }
    return false;
}

void Nide8AuthDownload::executeTask()
{
    setStatus(tr("Downloading nide8auth..."));

    // Ensure parent directory exists
    QDir().mkpath(QFileInfo(m_jarPath).absolutePath());

    // TODO: This is a non-official mirror link. Replace with official source when available.
    // Current source: https://gitee.com/LostCityTeam/LostCity-Server-Resources/raw/master/nide8auth.jar
    QString jarUrl = "https://gitee.com/LostCityTeam/LostCity-Server-Resources/raw/master/nide8auth.jar";

    QNetworkRequest request((QUrl(jarUrl)));
    request.setRawHeader("User-Agent", APPLICATION->getUserAgent().toUtf8());

    m_jarReply = m_network->get(request);
    connect(m_jarReply, &QNetworkReply::finished, this, &Nide8AuthDownload::downloadJarFinished);
    connect(m_jarReply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        if (total > 0) {
            setProgress(received, total);
        }
        setDetails(tr("%1 / %2 KB").arg(received / 1024).arg(total / 1024));
    });
}

void Nide8AuthDownload::downloadJarFinished()
{
    if (m_jarReply->error() != QNetworkReply::NoError) {
        emitFailed(tr("Failed to download nide8auth.jar: %1").arg(m_jarReply->errorString()));
        m_jarReply->deleteLater();
        m_jarReply = nullptr;
        return;
    }

    QByteArray jarData = m_jarReply->readAll();
    m_jarReply->deleteLater();
    m_jarReply = nullptr;

    // Check if we got any data
    if (jarData.isEmpty()) {
        emitFailed(tr("Downloaded nide8auth.jar is empty"));
        return;
    }

    // Write to file
    QFile file(m_jarPath);
    if (!file.open(QIODevice::WriteOnly)) {
        emitFailed(tr("Failed to open nide8auth.jar for writing: %1").arg(file.errorString()));
        return;
    }

    file.write(jarData);
    file.close();

    qDebug() << "Successfully downloaded nide8auth.jar to" << m_jarPath;
    emitSucceeded();
}
