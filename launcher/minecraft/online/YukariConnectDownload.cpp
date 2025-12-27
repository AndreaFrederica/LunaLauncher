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

#include "YukariConnectDownload.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>

#include "Application.h"
#include "FileSystem.h"

YukariConnectDownload::YukariConnectDownload(bool useMirror, QObject* parent) : Task(parent), m_useMirror(useMirror)
{
    m_network = new QNetworkAccessManager(this);
#ifdef Q_OS_WIN32
    m_exePath = FS::PathCombine(APPLICATION->root(), "yukari-connect.exe");
#else
    m_exePath = FS::PathCombine(APPLICATION->root(), "yukari-connect");
#endif
    m_metadataPath = FS::PathCombine(APPLICATION->root(), "yukari-connect.json");
}

bool YukariConnectDownload::abort()
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

void YukariConnectDownload::executeTask()
{
    setStatus(tr("Checking YukariConnect version..."));

    // Determine download URL based on mirror preference
    const QString infoUrl = m_useMirror ? "https://gitee.com/api/v5/repos/YukariC/YukariConnect/releases/latest"
                                        : "https://api.github.com/repos/YukariC/YukariConnect/releases/latest";

    QNetworkRequest request((QUrl(infoUrl)));
    request.setRawHeader("User-Agent", APPLICATION->getUserAgent().toUtf8());
    request.setRawHeader("Accept", "application/json");

    m_infoReply = m_network->get(request);
    connect(m_infoReply, &QNetworkReply::finished, this, &YukariConnectDownload::downloadInfoFinished);
    connect(m_infoReply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        if (total > 0) {
            setProgress(received, total);
        }
    });
}

void YukariConnectDownload::downloadInfoFinished()
{
    if (m_infoReply->error() != QNetworkReply::NoError) {
        emitFailed(tr("Failed to fetch YukariConnect release info: %1").arg(m_infoReply->errorString()));
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
        emitFailed(tr("Failed to parse YukariConnect release info: %1").arg(parseError.errorString()));
        return;
    }

    const QJsonObject obj = doc.object();
    m_version = obj["tag_name"].toString();

    // Get the download URL for the current platform
    QString exeDownloadUrl;

    const QJsonArray assets = obj["assets"].toArray();
    for (const QJsonValue& assetVal : assets) {
        const QJsonObject asset = assetVal.toObject();
        const QString name = asset["name"].toString();

        // Match platform-specific files
#ifdef Q_OS_WIN32
        if (name.contains("windows") && name.endsWith(".exe")) {
            // Prefer x86_64, fallback to arm64
            if (name.contains("x86_64")) {
                exeDownloadUrl = asset["browser_download_url"].toString();
                break;
            } else if (name.contains("arm64") && exeDownloadUrl.isEmpty()) {
                exeDownloadUrl = asset["browser_download_url"].toString();
            }
        }
#elif defined(Q_OS_MACOS)
        if (name.contains("macos") && !name.endsWith(".pkg") && !name.endsWith(".tar.gz")) {
            // Prefer arm64 (Apple Silicon), fallback to x86_64
            if (name.contains("arm64")) {
                exeDownloadUrl = asset["browser_download_url"].toString();
                break;
            } else if (name.contains("x86_64") && exeDownloadUrl.isEmpty()) {
                exeDownloadUrl = asset["browser_download_url"].toString();
            }
        }
#else
        if (name.contains("linux") && !name.endsWith(".tar.gz")) {
            // Prefer x86_64, then fallback to other architectures
            if (name.contains("x86_64")) {
                exeDownloadUrl = asset["browser_download_url"].toString();
                break;
            } else if (name.contains("arm64") && exeDownloadUrl.isEmpty()) {
                exeDownloadUrl = asset["browser_download_url"].toString();
            } else if (name.contains("loongarch64") && exeDownloadUrl.isEmpty()) {
                exeDownloadUrl = asset["browser_download_url"].toString();
            } else if (name.contains("riscv64") && exeDownloadUrl.isEmpty()) {
                exeDownloadUrl = asset["browser_download_url"].toString();
            }
        }
#endif
    }

    if (exeDownloadUrl.isEmpty()) {
        emitFailed(tr("Could not find YukariConnect download for current platform"));
        return;
    }

    // Save metadata for later reference
    m_metadataObj = obj;

    startExecutableDownload(exeDownloadUrl, m_version);
}

void YukariConnectDownload::startExecutableDownload(const QString& url, const QString& version)
{
    setStatus(tr("Downloading YukariConnect (%1)...").arg(version));

    // Ensure parent directory exists
    QDir().mkpath(QFileInfo(m_exePath).absolutePath());

    QNetworkRequest request((QUrl(url)));
    request.setRawHeader("User-Agent", APPLICATION->getUserAgent().toUtf8());

    m_exeReply = m_network->get(request);
    connect(m_exeReply, &QNetworkReply::finished, this, &YukariConnectDownload::downloadExecutableFinished);
    connect(m_exeReply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        if (total > 0) {
            setProgress(received, total);
        }
        setDetails(tr("%1 / %2 MB").arg(received / (1024 * 1024)).arg(total / (1024 * 1024)));
    });
}

void YukariConnectDownload::downloadExecutableFinished()
{
    if (m_exeReply->error() != QNetworkReply::NoError) {
        emitFailed(tr("Failed to download YukariConnect: %1").arg(m_exeReply->errorString()));
        m_exeReply->deleteLater();
        m_exeReply = nullptr;
        return;
    }

    const QByteArray exeData = m_exeReply->readAll();
    m_exeReply->deleteLater();
    m_exeReply = nullptr;

    // Write to file
    QFile file(m_exePath);
    if (!file.open(QIODevice::WriteOnly)) {
        emitFailed(tr("Failed to open YukariConnect for writing: %1").arg(file.errorString()));
        return;
    }

    file.write(exeData);
    file.close();

    // Make executable on Unix-like systems
#ifndef Q_OS_WIN32
    QFile::setPermissions(m_exePath, QFile::permissions(m_exePath) | QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther);
#endif

    // Save metadata
    if (!m_version.isEmpty()) {
        QJsonObject metadataObj;
        metadataObj["version"] = m_version;
        metadataObj["download_time"] = QDateTime::currentDateTime().toString(Qt::ISODate);

        // Add relevant info from the release metadata
        if (!m_metadataObj.isEmpty()) {
            metadataObj["tag_name"] = m_metadataObj["tag_name"].toString();
            metadataObj["html_url"] = m_metadataObj["html_url"].toString();
        }

        const QJsonDocument metadataDoc(metadataObj);
        QFile metadataFile(m_metadataPath);
        if (metadataFile.open(QIODevice::WriteOnly)) {
            metadataFile.write(metadataDoc.toJson());
            metadataFile.close();
            qDebug() << "Saved YukariConnect metadata: version" << m_version;
        } else {
            qWarning() << "Failed to save YukariConnect metadata";
        }
    }

    qDebug() << "Successfully downloaded YukariConnect to" << m_exePath;
    emitSucceeded();
}
