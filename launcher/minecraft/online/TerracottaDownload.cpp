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

#include "TerracottaDownload.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QTemporaryDir>

#include "Application.h"
#include "FileSystem.h"
#include "MMCZip.h"
#include "TerracottaReleaseUtils.h"

namespace {
#ifdef Q_OS_WIN32
constexpr auto kTerracottaOs = TerracottaReleaseUtils::Os::Windows;
#elif defined(Q_OS_MACOS)
constexpr auto kTerracottaOs = TerracottaReleaseUtils::Os::MacOS;
#else
constexpr auto kTerracottaOs = TerracottaReleaseUtils::Os::Linux;
#endif
}

TerracottaDownload::TerracottaDownload(bool useMirror, QObject* parent) : Task(parent), m_useMirror(useMirror)
{
    m_network = new QNetworkAccessManager(this);
#ifdef Q_OS_WIN32
    m_exePath = FS::PathCombine(APPLICATION->root(), "terracotta.exe");
#else
    m_exePath = FS::PathCombine(APPLICATION->root(), "terracotta");
#endif
    m_metadataPath = FS::PathCombine(APPLICATION->root(), "terracotta.json");
}

bool TerracottaDownload::abort()
{
    if (m_infoReply && m_infoReply->isRunning()) {
        m_infoReply->abort();
        return true;
    }
    if (m_exeReply && m_exeReply->isRunning()) {
        m_exeReply->abort();
        return true;
    }
    return false;
}

void TerracottaDownload::executeTask()
{
    setStatus(tr("Checking Terracotta version..."));

    // Determine download URL based on mirror preference
    const QString infoUrl = m_useMirror ? "https://gitee.com/api/v5/repos/burningtnt/Terracotta/releases/latest"
                                        : "https://api.github.com/repos/burningtnt/Terracotta/releases/latest";

    QNetworkRequest request((QUrl(infoUrl)));
    request.setRawHeader("User-Agent", APPLICATION->getUserAgent().toUtf8());
    request.setRawHeader("Accept", "application/json");

    m_infoReply = m_network->get(request);
    connect(m_infoReply, &QNetworkReply::finished, this, &TerracottaDownload::downloadInfoFinished);
    connect(m_infoReply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        if (total > 0) {
            setProgress(received, total);
        }
    });
}

void TerracottaDownload::downloadInfoFinished()
{
    if (m_infoReply->error() != QNetworkReply::NoError) {
        emitFailed(tr("Failed to fetch Terracotta release info: %1").arg(m_infoReply->errorString()));
        m_infoReply->deleteLater();
        m_infoReply = nullptr;
        return;
    }

    const QByteArray data = m_infoReply->readAll();
    m_infoReply->deleteLater();
    m_infoReply = nullptr;

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        emitFailed(tr("Failed to parse Terracotta release info: %1").arg(parseError.errorString()));
        return;
    }

    const QJsonObject obj = doc.object();
    m_version = obj["tag_name"].toString();

    QString exeDownloadUrl;

    const QJsonArray assets = obj["assets"].toArray();
    exeDownloadUrl = TerracottaReleaseUtils::pickDownloadUrl(assets, kTerracottaOs);

    if (exeDownloadUrl.isEmpty()) {
        emitFailed(tr("Could not find Terracotta download for current platform"));
        return;
    }

    m_metadataObj = obj;

    startExecutableDownload(exeDownloadUrl, m_version);
}

void TerracottaDownload::startExecutableDownload(const QString& url, const QString& version)
{
    setStatus(tr("Downloading Terracotta (%1)...").arg(version));

    QDir().mkpath(QFileInfo(m_exePath).absolutePath());

    m_isArchive = TerracottaReleaseUtils::isArchiveUrl(url);

    QNetworkRequest request((QUrl(url)));
    request.setRawHeader("User-Agent", APPLICATION->getUserAgent().toUtf8());

    m_exeReply = m_network->get(request);
    connect(m_exeReply, &QNetworkReply::finished, this, &TerracottaDownload::downloadExecutableFinished);
    connect(m_exeReply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        if (total > 0) {
            setProgress(received, total);
        }
        setDetails(tr("%1 / %2 MB").arg(received / (1024 * 1024)).arg(total / (1024 * 1024)));
    });
}

void TerracottaDownload::downloadExecutableFinished()
{
    if (m_exeReply->error() != QNetworkReply::NoError) {
        emitFailed(tr("Failed to download Terracotta: %1").arg(m_exeReply->errorString()));
        m_exeReply->deleteLater();
        m_exeReply = nullptr;
        return;
    }

    const QByteArray exeData = m_exeReply->readAll();
    m_exeReply->deleteLater();
    m_exeReply = nullptr;

    if (m_isArchive) {
        QTemporaryDir tempDir;
        if (!tempDir.isValid()) {
            emitFailed(tr("Failed to create temporary directory"));
            return;
        }
        QString archivePath = FS::PathCombine(tempDir.path(), "download.archive");
        QFile file(archivePath);
        if (!file.open(QIODevice::WriteOnly)) {
            emitFailed(tr("Failed to open temporary file for writing: %1").arg(file.errorString()));
            return;
        }
        file.write(exeData);
        file.close();

        QString extractPath = FS::PathCombine(tempDir.path(), "extracted");
        QDir().mkpath(extractPath);

        if (!MMCZip::extractDir(archivePath, extractPath)) {
            emitFailed(tr("Failed to extract Terracotta archive"));
            return;
        }

        const QString foundExePath = TerracottaReleaseUtils::findExecutableInDir(extractPath, kTerracottaOs);
        if (foundExePath.isEmpty()) {
            emitFailed(tr("Could not find Terracotta executable in the downloaded archive"));
            return;
        }

        QFile::remove(m_exePath);
        if (!QFile::copy(foundExePath, m_exePath)) {
            emitFailed(tr("Failed to move Terracotta executable to target location"));
            return;
        }
    } else {
        QFile file(m_exePath);
        if (!file.open(QIODevice::WriteOnly)) {
            emitFailed(tr("Failed to open Terracotta for writing: %1").arg(file.errorString()));
            return;
        }

        file.write(exeData);
        file.close();
    }

#ifndef Q_OS_WIN32
    QFile::setPermissions(m_exePath, QFile::permissions(m_exePath) | QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther);
#endif

    if (!m_version.isEmpty()) {
        QJsonObject metadataObj;
        metadataObj["version"] = m_version;
        metadataObj["download_time"] = QDateTime::currentDateTime().toString(Qt::ISODate);

        if (!m_metadataObj.isEmpty()) {
            metadataObj["tag_name"] = m_metadataObj["tag_name"].toString();
            metadataObj["html_url"] = m_metadataObj["html_url"].toString();
        }

        const QJsonDocument metadataDoc(metadataObj);
        QFile metadataFile(m_metadataPath);
        if (metadataFile.open(QIODevice::WriteOnly)) {
            metadataFile.write(metadataDoc.toJson());
            metadataFile.close();
            qDebug() << "Saved Terracotta metadata: version" << m_version;
        } else {
            qWarning() << "Failed to save Terracotta metadata";
        }
    }

    qDebug() << "Successfully downloaded Terracotta to" << m_exePath;
    emitSucceeded();
}
