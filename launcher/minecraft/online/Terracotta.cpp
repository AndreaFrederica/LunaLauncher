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
 *
 *  ----------------------------------------------------------------------
 *
 *  Terracotta Integration Notice
 *
 *  This file contains integration code with Terracotta, a P2P multiplayer
 *  solution for Minecraft. Terracotta is developed by burningtnt and licensed
 *  under AGPL-3.0 with the following exception:
 *
 *  "Your program通过本作品提供的进程间通信接口（如 HTTP API）与未经修改的
 *   本作品应用程序进行交互，不构成衍生作品。"
 *
 *  Translation: "Your program's interaction with an unmodified copy of this
 *  work through the inter-process communication interfaces provided by this
 *  work (such as HTTP APIs) does not constitute a derivative work."
 *
 *  Project URL: https://github.com/burningtnt/Terracotta
 *
 *  This integration is implemented as a temporary solution. The launcher
 *  communicates with the standalone Terracotta binary via its HTTP API only,
 *  which is explicitly permitted under Terracotta's license exception.
 *
 *  A future version will replace this with a custom implementation.
 */

#include "Terracotta.h"

#include "Application.h"
#include "FileSystem.h"
#include "MMCZip.h"
#include "TerracottaReleaseUtils.h"
#include "settings/SettingsObject.h"

#include <QDateTime>
#include <QDir>
#include <QRegularExpression>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>

#ifdef Q_OS_WIN32
#include <windows.h>
#endif

Terracotta* Terracotta::s_instance = nullptr;

namespace {
#ifdef Q_OS_WIN32
constexpr auto kTerracottaOs = TerracottaReleaseUtils::Os::Windows;
#elif defined(Q_OS_MACOS)
constexpr auto kTerracottaOs = TerracottaReleaseUtils::Os::MacOS;
#else
constexpr auto kTerracottaOs = TerracottaReleaseUtils::Os::Linux;
#endif
}

namespace TerracottaTypes {

// Profile implementation
Profile Profile::fromJson(const QJsonObject& obj)
{
    Profile profile;
    profile.machine_id = obj["machine_id"].toString();
    profile.name = obj["name"].toString();
    profile.vendor = obj["vendor"].toString();

    const QString kindStr = obj["kind"].toString();
    if (kindStr == "HOST") {
        profile.kind = ProfileKind::HOST;
    } else if (kindStr == "GUEST") {
        profile.kind = ProfileKind::GUEST;
    } else {
        profile.kind = ProfileKind::LOCAL;
    }

    return profile;
}

QJsonObject Profile::toJson() const
{
    QJsonObject obj;
    obj["machine_id"] = machine_id;
    obj["name"] = name;
    obj["vendor"] = vendor;

    QString kindStr;
    switch (kind) {
        case ProfileKind::HOST:
            [[fallthrough]];
        case ProfileKind::GUEST:
            kindStr = (kind == ProfileKind::HOST) ? "HOST" : "GUEST";
            break;
        default:
            kindStr = "LOCAL";
            break;
    }
    obj["kind"] = kindStr;

    return obj;
}

// StateResponse implementation
StateResponse StateResponse::fromJson(const QJsonObject& obj)
{
    StateResponse response;
    response.index = obj["index"].toVariant().toLongLong();

    const QString stateStr = obj["state"].toString();
    if (stateStr == "waiting") {
        response.state = State::Waiting;
    } else if (stateStr == "host-scanning") {
        response.state = State::HostScanning;
    } else if (stateStr == "host-starting") {
        response.state = State::HostStarting;
    } else if (stateStr == "host-ok") {
        response.state = State::HostOk;
    } else if (stateStr == "guest-connecting") {
        response.state = State::GuestConnecting;
    } else if (stateStr == "guest-starting") {
        response.state = State::GuestStarting;
    } else if (stateStr == "guest-ok") {
        response.state = State::GuestOk;
    } else if (stateStr == "exception") {
        response.state = State::Exception;
    } else {
        response.state = State::Waiting;
    }

    response.room = obj["room"].toString();
    response.url = obj["url"].toString();
    response.profile_index = obj["profile_index"].toVariant().toLongLong();

    const QJsonArray profilesArray = obj["profiles"].toArray();
    for (const QJsonValue& val : profilesArray) {
        response.profiles.append(Profile::fromJson(val.toObject()));
    }

    // Parse difficulty field (for guest-starting state)
    const QString difficultyStr = obj["difficulty"].toString();
    if (difficultyStr == "easiest") {
        response.difficulty = Difficulty::EASIEST;
    } else if (difficultyStr == "simple") {
        response.difficulty = Difficulty::SIMPLE;
    } else if (difficultyStr == "medium") {
        response.difficulty = Difficulty::MEDIUM;
    } else if (difficultyStr == "tough") {
        response.difficulty = Difficulty::TOUGH;
    } else {
        response.difficulty = Difficulty::UNKNOWN;
    }

    // Parse exception_type field (for exception state)
    const int exceptionTypeValue = obj["type"].toVariant().toInt();
    if (exceptionTypeValue >= 0 && exceptionTypeValue <= 5) {
        response.exception_type = static_cast<ExceptionType>(exceptionTypeValue);
    } else {
        response.exception_type = ExceptionType::ScaffoldingInvalidResponse;
    }

    return response;
}

// MetaInfo implementation
MetaInfo MetaInfo::fromJson(const QJsonObject& obj)
{
    MetaInfo info;
    info.compile_timestamp = obj["compile_timestamp"].toString();
    info.easytier_version = obj["easytier_version"].toString();
    info.target_arch = obj["target_arch"].toString();
    info.target_env = obj["target_env"].toString();
    info.target_os = obj["target_os"].toString();
    info.target_tuple = obj["target_tuple"].toString();
    info.target_vendor = obj["target_vendor"].toString();
    info.version = obj["version"].toString();
    info.yggdrasil_port = obj["yggdrasil_port"].toInt();
    return info;
}

}  // namespace TerracottaTypes

Terracotta& Terracotta::instance()
{
    if (!s_instance) {
        s_instance = new Terracotta();
    }
    return *s_instance;
}

void Terracotta::setStartupMode(StartupMode mode)
{
    m_startupMode = mode;
}

Terracotta::StartupMode Terracotta::getStartupMode() const
{
    return m_startupMode;
}

Terracotta::Terracotta(QObject* parent) : QObject(parent), m_baseUrl("http://localhost:12580")
{
    auto s = APPLICATION->settings();
    int startupMode = s->contains("TerracottaStartupMode")
                          ? s->get("TerracottaStartupMode").toInt()
                          : static_cast<int>(StartupMode::HMCL);
    m_startupMode = static_cast<StartupMode>(startupMode);
}

QString Terracotta::getLocalPath() const
{
#ifdef Q_OS_WIN32
    return FS::PathCombine(APPLICATION->root(), "terracotta.exe");
#else
    return FS::PathCombine(APPLICATION->root(), "terracotta");
#endif
}

QString Terracotta::getMetadataPath() const
{
    return FS::PathCombine(APPLICATION->root(), "terracotta.json");
}

QString Terracotta::getVersion() const
{
    const QString metadataPath = getMetadataPath();
    QFile file(metadataPath);

    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }

    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return QString();
    }

    const QJsonObject obj = doc.object();
    return obj["version"].toString();
}

bool Terracotta::checkCache() const
{
    const QString path = getLocalPath();
    if (!QFile::exists(path)) {
        return false;
    }

    // Check file size - Terracotta executable should be at least 1MB
    const QFileInfo info(path);
    if (info.size() < 1024 * 1024) {
        qDebug() << "Terracotta file too small, re-downloading";
        return false;
    }

    return true;
}

QString Terracotta::downloadLatest(bool useMirror)
{
    const QString cachePath = getLocalPath();

    // If cache is valid, return it
    if (checkCache()) {
        qDebug() << "Using cached Terracotta at" << cachePath;
        return cachePath;
    }

    // Ensure cache directory exists
    QDir().mkpath(QFileInfo(cachePath).absolutePath());

    // Determine download URL based on platform
    const QString downloadUrl = useMirror ? "https://gitee.com/api/v5/repos/burningtnt/Terracotta/releases/latest"
                                          : "https://api.github.com/repos/burningtnt/Terracotta/releases/latest";

    QNetworkAccessManager manager;
    QNetworkRequest request((QUrl(downloadUrl)));
    request.setRawHeader("User-Agent", APPLICATION->getUserAgent().toUtf8());
    request.setRawHeader("Accept", "application/json");

    QNetworkReply* reply = manager.get(request);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "Failed to fetch Terracotta info:" << reply->errorString();
        reply->deleteLater();
        return QString();
    }

    const QByteArray data = reply->readAll();
    reply->deleteLater();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse Terracotta info JSON:" << parseError.errorString();
        return QString();
    }

    const QJsonObject obj = doc.object();

    // Get the download URL for the current platform
    QString assetDownloadUrl;
    QString expectedVersion;

    expectedVersion = obj["tag_name"].toString();
    const QJsonArray assets = obj["assets"].toArray();
    assetDownloadUrl = TerracottaReleaseUtils::pickDownloadUrl(assets, kTerracottaOs);

    if (assetDownloadUrl.isEmpty()) {
        qWarning() << "Could not find Terracotta download for current platform";
        return QString();
    }

    // Download the executable
    request.setUrl(QUrl(assetDownloadUrl));
    reply = manager.get(request);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "Failed to download Terracotta:" << reply->errorString();
        reply->deleteLater();
        return QString();
    }

    const QByteArray exeData = reply->readAll();
    reply->deleteLater();

    if (TerracottaReleaseUtils::isArchiveUrl(assetDownloadUrl)) {
        QTemporaryDir tempDir;
        if (!tempDir.isValid()) {
            qWarning() << "Failed to create temporary directory";
            return QString();
        }

        const QString archivePath = FS::PathCombine(tempDir.path(), "download.archive");
        QFile file(archivePath);
        if (!file.open(QIODevice::WriteOnly)) {
            qWarning() << "Failed to open temporary file for writing:" << file.errorString();
            return QString();
        }
        file.write(exeData);
        file.close();

        const QString extractPath = FS::PathCombine(tempDir.path(), "extracted");
        QDir().mkpath(extractPath);

        if (!MMCZip::extractDir(archivePath, extractPath)) {
            qWarning() << "Failed to extract Terracotta archive";
            return QString();
        }

        const QString foundExePath = TerracottaReleaseUtils::findExecutableInDir(extractPath, kTerracottaOs);
        if (foundExePath.isEmpty()) {
            qWarning() << "Could not find Terracotta executable in archive";
            return QString();
        }

        QFile::remove(cachePath);
        if (!QFile::copy(foundExePath, cachePath)) {
            qWarning() << "Failed to move Terracotta executable to cache path";
            return QString();
        }
    } else {
        QFile file(cachePath);
        if (!file.open(QIODevice::WriteOnly)) {
            qWarning() << "Failed to open Terracotta for writing:" << file.errorString();
            return QString();
        }

        file.write(exeData);
        file.close();
    }

    // Make executable on Unix-like systems
#ifndef Q_OS_WIN32
    QFile::setPermissions(cachePath, QFile::permissions(cachePath) | QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther);
#endif

    // Save metadata
    if (!expectedVersion.isEmpty()) {
        QJsonObject metadataObj;
        metadataObj["version"] = expectedVersion;
        metadataObj["download_url"] = assetDownloadUrl;
        metadataObj["download_time"] = QDateTime::currentDateTime().toString(Qt::ISODate);

        const QJsonDocument metadataDoc(metadataObj);
        QFile metadataFile(getMetadataPath());
        if (metadataFile.open(QIODevice::WriteOnly)) {
            metadataFile.write(metadataDoc.toJson());
            metadataFile.close();
            qDebug() << "Saved Terracotta metadata: version" << expectedVersion;
        }
    }

    qDebug() << "Successfully downloaded Terracotta to" << cachePath;
    return cachePath;
}

QString Terracotta::getBaseUrl() const
{
    return m_baseUrl;
}

void Terracotta::setBaseUrl(const QString& url)
{
    m_baseUrl = url;
}

bool Terracotta::isAvailable()
{
    auto meta = fetchMeta();
    return meta != nullptr;
}

std::shared_ptr<TerracottaTypes::MetaInfo> Terracotta::fetchMeta()
{
    return fetchMeta(5000);  // Default 5 second timeout
}

std::shared_ptr<TerracottaTypes::MetaInfo> Terracotta::fetchMeta(int timeoutMs)
{
    const QByteArray data = sendGetRequest("/meta", {}, timeoutMs);
    if (data.isEmpty()) {
        return nullptr;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse Terracotta meta JSON:" << parseError.errorString();
        return nullptr;
    }

    return std::make_shared<TerracottaTypes::MetaInfo>(TerracottaTypes::MetaInfo::fromJson(doc.object()));
}

std::shared_ptr<TerracottaTypes::StateResponse> Terracotta::fetchState()
{
    const QByteArray data = sendGetRequest("/state", {});
    if (data.isEmpty()) {
        return nullptr;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse Terracotta state JSON:" << parseError.errorString();
        return nullptr;
    }

    auto state = std::make_shared<TerracottaTypes::StateResponse>(TerracottaTypes::StateResponse::fromJson(doc.object()));
    emit stateChanged(*state);
    return state;
}

bool Terracotta::panic(bool peaceful)
{
    QMap<QString, QString> params;
    params["peaceful"] = peaceful ? "true" : "false";
    bool result = sendGetRequestWithStatus("/panic", params);
    // Emergency stop (peaceful=false) terminates the server immediately
    // Reset the success flag so isProcessRunning() returns false
    if (!peaceful && result) {
        m_wasStartedSuccessfully = false;
    }
    return result;
}

void Terracotta::panicAsync(bool peaceful)
{
    QMap<QString, QString> params;
    params["peaceful"] = peaceful ? "true" : "false";
    sendGetRequestAsync("/panic", params);
    // Reset the flag immediately since we're shutting down
    if (!peaceful) {
        m_wasStartedSuccessfully = false;
    }
}

bool Terracotta::startScanning(const QString& player)
{
    QMap<QString, QString> params;
    params["player"] = player;
    return sendGetRequestWithStatus("/state/scanning", params);
}

bool Terracotta::cancelState()
{
    return sendGetRequestWithStatus("/state/ide", {});
}

void Terracotta::cancelStateAsync()
{
    sendGetRequestAsync("/state/ide", {});
}

bool Terracotta::joinRoom(const QString& room, const QString& player)
{
    QMap<QString, QString> params;
    params["room"] = room;
    params["player"] = player;
    return sendGetRequestWithStatus("/state/guesting", params);
}

QByteArray Terracotta::fetchLog(bool fetch)
{
    QMap<QString, QString> params;
    params["fetch"] = fetch ? "true" : "false";
    return sendGetRequest("/log", params);
}

QByteArray Terracotta::sendGetRequest(const QString& endpoint, const QMap<QString, QString>& params, int timeoutMs)
{
    QNetworkAccessManager manager;

    // Build URL with parameters
    QString urlStr = m_baseUrl + endpoint;
    if (!params.isEmpty()) {
        QStringList paramList;
        for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
            paramList.append(it.key() + "=" + it.value());
        }
        urlStr += "?" + paramList.join("&");
    }

    QNetworkRequest request{ QUrl(urlStr) };
    request.setRawHeader("User-Agent", APPLICATION->getUserAgent().toUtf8());

    QNetworkReply* reply = manager.get(request);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    // Set timeout
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeoutTimer.start(timeoutMs);

    loop.exec();

    // Check if timeout occurred
    if (!timeoutTimer.isActive()) {
        qWarning() << "Terracotta request timeout:" << urlStr;
        reply->abort();
        reply->deleteLater();
        emit availabilityChanged(false);
        return QByteArray();
    }
    timeoutTimer.stop();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "Terracotta request failed:" << reply->errorString() << "URL:" << urlStr;
        emit availabilityChanged(false);
        // Reset the start flag when server becomes unavailable
        m_wasStartedSuccessfully = false;
        reply->deleteLater();
        return QByteArray();
    }

    emit availabilityChanged(true);
    const QByteArray data = reply->readAll();
    reply->deleteLater();
    return data;
}

bool Terracotta::sendGetRequestWithStatus(const QString& endpoint, const QMap<QString, QString>& params, int timeoutMs)
{
    QNetworkAccessManager manager;

    // Build URL with parameters
    QString urlStr = m_baseUrl + endpoint;
    if (!params.isEmpty()) {
        QStringList paramList;
        for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
            paramList.append(it.key() + "=" + it.value());
        }
        urlStr += "?" + paramList.join("&");
    }

    QNetworkRequest request{ QUrl(urlStr) };
    request.setRawHeader("User-Agent", APPLICATION->getUserAgent().toUtf8());

    QNetworkReply* reply = manager.get(request);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    // Set timeout
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeoutTimer.start(timeoutMs);

    loop.exec();

    // Check if timeout occurred
    if (!timeoutTimer.isActive()) {
        qWarning() << "Terracotta request timeout:" << urlStr;
        reply->abort();
        reply->deleteLater();
        emit availabilityChanged(false);
        return false;
    }
    timeoutTimer.stop();

    // Check for network errors
    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "Terracotta request failed:" << reply->errorString() << "URL:" << urlStr
                   << "HTTP status:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        emit availabilityChanged(false);
        // Reset the start flag when server becomes unavailable
        m_wasStartedSuccessfully = false;
        reply->deleteLater();
        return false;
    }

    // Check HTTP status code (some endpoints return 200 OK with empty body, others return 400 on error)
    int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (httpStatus >= 200 && httpStatus < 300) {
        emit availabilityChanged(true);
        reply->deleteLater();
        return true;
    } else {
        qWarning() << "Terracotta request returned HTTP status:" << httpStatus << "URL:" << urlStr;
        emit availabilityChanged(false);
        reply->deleteLater();
        return false;
    }
}

void Terracotta::sendGetRequestAsync(const QString& endpoint, const QMap<QString, QString>& params)
{
    // Build URL with parameters
    QString urlStr = m_baseUrl + endpoint;
    if (!params.isEmpty()) {
        QStringList paramList;
        for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
            paramList.append(it.key() + "=" + it.value());
        }
        urlStr += "?" + paramList.join("&");
    }

    QNetworkRequest request{ QUrl(urlStr) };
    request.setRawHeader("User-Agent", APPLICATION->getUserAgent().toUtf8());

    // Create a manager that will be deleted when the reply is finished
    QNetworkAccessManager* manager = new QNetworkAccessManager(this);
    QNetworkReply* reply = manager->get(request);

    // Connect to finished signal to clean up
    connect(reply, &QNetworkReply::finished, this, [manager, reply]() {
        reply->deleteLater();
        manager->deleteLater();
    });

    // Also clean up on error (in case finished never fires)
    connect(reply, &QNetworkReply::errorOccurred, this, [manager, reply]() {
        reply->deleteLater();
        manager->deleteLater();
    });
}

QString Terracotta::getPortFilePath() const
{
    if (m_portFilePath.isEmpty()) {
        // Create a temporary file path for the port JSON
        // Use temp directory + terracotta_port.json
        QString tempDir = QDir::tempPath();
        m_portFilePath = FS::PathCombine(tempDir, "terracotta_port.json");
    }
    return m_portFilePath;
}

bool Terracotta::waitForPortFile(quint32 timeoutMs)
{
    const QString portFile = getPortFilePath();
    const quint32 startTime = QDateTime::currentMSecsSinceEpoch();

    while (QDateTime::currentMSecsSinceEpoch() - startTime < timeoutMs) {
        if (QFile::exists(portFile)) {
            return true;
        }
        QCoreApplication::processEvents();
        QThread::msleep(100);
    }
    return false;
}

bool Terracotta::readPortFromFile(quint16& port) const
{
    const QString portFile = getPortFilePath();
    QFile file(portFile);

    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse port file JSON:" << parseError.errorString();
        return false;
    }

    const QJsonObject obj = doc.object();
    if (!obj.contains("port")) {
        qWarning() << "Port file missing 'port' field";
        return false;
    }

    const QJsonValue portValue = obj["port"];
    if (!portValue.isDouble()) {
        qWarning() << "Port value is not a number";
        return false;
    }

    int portInt = portValue.toInt(-1);
    if (portInt <= 0 || portInt > 65535) {
        qWarning() << "Invalid port value:" << portInt;
        return false;
    }

    port = static_cast<quint16>(portInt);
    return true;
}

bool Terracotta::startProcess()
{
    // Check if already started successfully via HMCL mode
    if (m_wasStartedSuccessfully) {
        // Verify the server is actually responding
        if (isAvailable()) {
            qDebug() << "Terracotta already running and available";
            return true;
        } else {
            // Server was started but no longer responding, reset state
            qDebug() << "Terracotta was started but no longer responding, resetting";
            m_wasStartedSuccessfully = false;
        }
    }

    if (m_process && m_process->state() == QProcess::Running) {
        qDebug() << "Terracotta process already running";
        return true;
    }

    const QString exePath = getLocalPath();
    if (!QFile::exists(exePath)) {
        qWarning() << "Terracotta executable not found at:" << exePath;
        return false;
    }

    // Clean up old port file if exists
    const QString portFile = getPortFilePath();
    if (QFile::exists(portFile)) {
        QFile::remove(portFile);
    }

    // Create process
    m_process = new QProcess(this);

    // Setup process to run in background
    m_process->setProcessChannelMode(QProcess::MergedChannels);

    // In HMCL mode, the process exits after writing the port file - this is NORMAL
    // The web server continues running in the background
    bool portFileRead = false;
    quint16 detectedPort = 0;
    QByteArray capturedOutput;
    QEventLoop loop;
    bool portDetected = false;

    // Function to parse and detect port from output
    auto detectPortFromOutput = [&capturedOutput, &detectedPort, &portDetected, &loop]() -> bool {
        const QString output = QString::fromUtf8(capturedOutput);

        // Pattern 1: Secondary mode - "port = 62664"
        QRegularExpression portRegex(R"(port\s*=\s*(\d+))");
        QRegularExpressionMatch match = portRegex.match(output);
        if (match.hasMatch()) {
            bool ok = false;
            int port = match.captured(1).toInt(&ok);
            if (ok && port > 0 && port <= 65535) {
                detectedPort = static_cast<quint16>(port);
                qDebug() << "Terracotta detected existing instance (secondary mode) on port:" << detectedPort;
                portDetected = true;
                loop.quit();
                return true;
            }
        }

        // Pattern 2: Server mode - "Rocket has launched from http://127.0.0.1:52749"
        QRegularExpression rocketRegex(R"(Rocket has launched from http://[^/]+:(\d+))");
        match = rocketRegex.match(output);
        if (match.hasMatch()) {
            bool ok = false;
            int port = match.captured(1).toInt(&ok);
            if (ok && port > 0 && port <= 65535) {
                detectedPort = static_cast<quint16>(port);
                qDebug() << "Terracotta server mode detected on port:" << detectedPort;
                portDetected = true;
                loop.quit();
                return true;
            }
        }

        return false;
    };

    // Connect to process finished signal - for async exit handling
    connect(m_process, &QProcess::finished, this, [this, &loop](int exitCode, QProcess::ExitStatus) -> void {
        // Only handle exit if we haven't successfully started yet
        // If m_wasStartedSuccessfully is true, port was already detected and this is normal HMCL exit
        if (!m_wasStartedSuccessfully) {
            if (exitCode != 0) {
                qWarning() << "Terracotta process finished with non-zero exit code:" << exitCode;
            } else {
                qWarning() << "Terracotta process exited before port was detected";
            }
            emit availabilityChanged(false);
        } else {
            qDebug() << "Terracotta HMCL mode: process exited normally after successful start";
        }
        loop.quit();
    });

    connect(m_process, &QProcess::errorOccurred, this, [this, &loop](QProcess::ProcessError error) {
        qWarning() << "Terracotta process error:" << error;
        m_wasStartedSuccessfully = false;
        emit availabilityChanged(false);
        loop.quit();
    });

    // Read output when available and parse for port immediately
    connect(m_process, &QProcess::readyReadStandardOutput, this, [this, &capturedOutput, &detectPortFromOutput]() {
        const QByteArray data = m_process->readAllStandardOutput();
        qDebug() << "Terracotta:" << data;
        capturedOutput.append(data);
        detectPortFromOutput();  // Try to detect port immediately when output arrives
    });
    connect(m_process, &QProcess::readyReadStandardError, this, [this]() {
        qWarning() << "Terracotta:" << m_process->readAllStandardError();
    });

    // Start with --hmcl parameter to get port via file
    QStringList args;
    if (m_startupMode == StartupMode::HMCL) {
#ifdef Q_OS_WIN32
        args = { "--hmcl2", portFile };
#else
        args = { "--hmcl", portFile };
#endif
    } else {
        args = {};
    }
    qDebug() << "Starting Terracotta:" << exePath << args;

    m_process->start(exePath, args);

    if (!m_process->waitForStarted(5000)) {
        qWarning() << "Failed to start Terracotta process:" << m_process->errorString();
        delete m_process;
        m_process = nullptr;
        return false;
    }

    // Wait for port to be detected from output (max 5 seconds)
    // The readyReadStandardOutput signal will call detectPortFromOutput() which will quit the loop
    // when a port is detected
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(5000);
    loop.exec();

    // Disconnect all signals to prevent handlers from executing after loop exits
    // This is critical because the handlers capture references to local variables
    disconnect(m_process, &QProcess::finished, nullptr, nullptr);
    disconnect(m_process, &QProcess::errorOccurred, nullptr, nullptr);
    disconnect(m_process, &QProcess::readyReadStandardOutput, nullptr, nullptr);
    disconnect(m_process, &QProcess::readyReadStandardError, nullptr, nullptr);

    // Check if port was detected from output
    if (portDetected && detectedPort > 0) {
        m_baseUrl = QString("http://127.0.0.1:%1").arg(detectedPort);
        m_wasStartedSuccessfully = true;
        emit availabilityChanged(true);
        qDebug() << "Terracotta started on port:" << detectedPort;

        // Clean up the process if it exited
        if (m_process->state() != QProcess::Running) {
            m_process->deleteLater();
            m_process = nullptr;
        }
        return true;
    }

    // Process is still running - wait for port file (primary mode)
    if (m_startupMode == StartupMode::HMCL) {
        if (!waitForPortFile(8000)) {
            qWarning() << "Terracotta port file did not appear within timeout";
            if (m_process && m_process->state() == QProcess::Running) {
                qWarning() << "Process still running but no port file - terminating";
                stopProcess();
            }
            return false;
        }
        if (!readPortFromFile(detectedPort)) {
            qWarning() << "Failed to read port from file";
            stopProcess();
            return false;
        }
        portFileRead = true;
        m_baseUrl = QString("http://127.0.0.1:%1").arg(detectedPort);
        qDebug() << "Terracotta started on port (hmcl mode):" << detectedPort;
        m_wasStartedSuccessfully = true;
        emit availabilityChanged(true);
        return true;
    }
    return false;
}

void Terracotta::stopProcess()
{
    // Only attempt graceful shutdown via API
    // Don't force kill - let the user decide if they want to force terminate
    if (m_process && m_process->state() == QProcess::Running) {
        qDebug() << "Attempting graceful shutdown via panic API...";
        panic(true);
        // After sending shutdown signal, reset the success flag
        // The server may have exited before we could get a response
        m_wasStartedSuccessfully = false;
    }
}

void Terracotta::forceKillProcess()
{
    if (m_process) {
        qDebug() << "Force killing Terracotta process...";
        if (m_process->state() == QProcess::Running) {
            m_process->terminate();
            if (!m_process->waitForFinished(2000)) {
                qWarning() << "Terminate failed, killing process...";
                m_process->kill();
                m_process->waitForFinished(1000);
            }
        }
        m_process->deleteLater();
        m_process = nullptr;
    }

    // Clean up port file
    const QString portFile = getPortFilePath();
    if (QFile::exists(portFile)) {
        QFile::remove(portFile);
    }

    // Reset HMCL mode flag
    m_wasStartedSuccessfully = false;

    emit availabilityChanged(false);
}

bool Terracotta::shutdownAndWait(int timeoutMs)
{
    // First check if the process was ever started successfully
    // If we never started it or it was already marked as stopped, don't try to shut down
    if (!m_wasStartedSuccessfully) {
        qDebug() << "Terracotta was not started, skipping shutdown";
        return true;
    }

    // Check if Terracotta API is responding (with very short timeout)
    // Use an even shorter timeout to avoid blocking
    if (fetchMeta(50) == nullptr) {
        // Already not running
        qDebug() << "Terracotta not responding, assuming already stopped";
        m_wasStartedSuccessfully = false;
        return true;
    }

    // Send shutdown commands asynchronously (don't wait for response)
    cancelStateAsync();
    panicAsync(true);

    // Wait for API to stop responding
    const int sleepMs = 100;
    const int maxIterations = timeoutMs / sleepMs;
    for (int i = 0; i < maxIterations; i++) {
        QThread::msleep(sleepMs);
        // Use very short timeout to avoid blocking
        if (fetchMeta(50) == nullptr) {
            // Stopped successfully
            qDebug() << "Terracotta stopped successfully";
            m_wasStartedSuccessfully = false;
            return true;
        }
    }

    // Timeout - but we assume it has stopped
    qDebug() << "Terracotta shutdown timeout, assuming stopped";
    m_wasStartedSuccessfully = false;
    return false;
}

bool Terracotta::isProcessRunning() const
{
    if (m_startupMode == StartupMode::HMCL) {
        if (m_wasStartedSuccessfully) {
            return true;
        }
        return m_process && m_process->state() == QProcess::Running;
    } else {
        return m_process && m_process->state() == QProcess::Running;
    }
}

void Terracotta::setStopOnClose(bool stopOnClose)
{
    m_stopOnClose = stopOnClose;
}

bool Terracotta::getStopOnClose() const
{
    return m_stopOnClose;
}
