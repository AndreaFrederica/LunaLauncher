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
 *  YukariConnect Integration Notice
 *
 *  This file contains integration code with YukariConnect, a P2P multiplayer
 *  solution for Minecraft.
 *
 *  This integration communicates with the standalone YukariConnect binary via
 *  its WebSocket API for real-time operations, and HTTP API for
 *  control operations (such as shutdown/stop room).
 *
 *  A future version may replace this with a custom implementation.
 */

#include "YukariConnect.h"

#include "Application.h"
#include "FileSystem.h"
#include <BuildConfig.h>

#include <QDateTime>
#include <QDir>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QAbstractSocket>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QUrl>
#include <QWebSocket>
#include <QThread>
#include <QTimer>
#include <QtConcurrent>

#ifdef Q_OS_WIN32
#include <windows.h>
#endif

YukariConnect* YukariConnect::s_instance = nullptr;

namespace YukariConnectTypes {

// Profile implementation
Profile Profile::fromJson(const QJsonObject& obj)
{
    Profile profile;
    profile.machine_id = obj["machine_id"].toString();
    if (profile.machine_id.isEmpty()) {
        profile.machine_id = obj["machineId"].toString();
    }
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
    if (obj.contains("profile_index")) {
        response.profile_index = obj["profile_index"].toVariant().toLongLong();
    } else {
        response.profile_index = obj["profileIndex"].toVariant().toLongLong();
    }

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
    int exceptionTypeValue = -1;
    if (obj.contains("type")) {
        exceptionTypeValue = obj["type"].toVariant().toInt();
    } else if (obj.contains("exception_type")) {
        exceptionTypeValue = obj["exception_type"].toVariant().toInt();
    }
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
    info.compile_timestamp = obj["compileTimestamp"].toString();
    if (info.compile_timestamp.isEmpty()) {
        info.compile_timestamp = obj["compile_timestamp"].toString();
    }
    info.core_version = obj["easyTierVersion"].toString();
    if (info.core_version.isEmpty()) {
        info.core_version = obj["core_version"].toString();
    }
    info.target_arch = obj["targetArch"].toString();
    if (info.target_arch.isEmpty()) {
        info.target_arch = obj["target_arch"].toString();
    }
    info.target_env = obj["targetEnv"].toString();
    if (info.target_env.isEmpty()) {
        info.target_env = obj["target_env"].toString();
    }
    info.target_os = obj["targetOS"].toString();
    if (info.target_os.isEmpty()) {
        info.target_os = obj["target_os"].toString();
    }
    info.target_tuple = obj["targetTuple"].toString();
    if (info.target_tuple.isEmpty()) {
        info.target_tuple = obj["target_tuple"].toString();
    }
    info.target_vendor = obj["targetVendor"].toString();
    if (info.target_vendor.isEmpty()) {
        info.target_vendor = obj["target_vendor"].toString();
    }
    info.version = obj["version"].toString();
    const QString yggPortStr = obj["yggdrasilPort"].toString();
    bool ok = false;
    int yggPort = yggPortStr.toInt(&ok);
    info.yggdrasil_port = ok ? yggPort : obj["yggdrasil_port"].toInt();
    return info;
}

}  // namespace YukariConnectTypes

YukariConnect& YukariConnect::instance()
{
    if (!s_instance) {
        s_instance = new YukariConnect();
    }
    return *s_instance;
}

YukariConnect::YukariConnect(QObject* parent) : QObject(parent), m_baseUrl("http://localhost:5062")
{}

YukariConnect::~YukariConnect()
{
    // Clean up WebSocket connection on destruction
    resetWebSocket();
}

QString YukariConnect::getLocalPath() const
{
#ifdef Q_OS_WIN32
    return FS::PathCombine(APPLICATION->root(), "yukari-connect.exe");
#else
    return FS::PathCombine(APPLICATION->root(), "yukari-connect");
#endif
}

QString YukariConnect::getMetadataPath() const
{
    return FS::PathCombine(APPLICATION->root(), "yukari-connect.json");
}

QString YukariConnect::getConfigPath() const
{
    // YukariConnect config file (yukari.json) is in the same directory as the executable
    QFileInfo fileInfo(getLocalPath());
    QString configDir = fileInfo.absolutePath();
    return FS::PathCombine(configDir, "yukari.json");
}

QString YukariConnect::getVersion() const
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

bool YukariConnect::checkCache() const
{
    const QString path = getLocalPath();
    if (!QFile::exists(path)) {
        return false;
    }

    // Check file size - YukariConnect executable should be at least 1MB
    const QFileInfo info(path);
    if (info.size() < 1024 * 1024) {
        qDebug() << "YukariConnect file too small, re-downloading";
        return false;
    }

    return true;
}

QString YukariConnect::downloadLatest(bool useMirror)
{
    const QString cachePath = getLocalPath();

    // If cache is valid, return it
    if (checkCache()) {
        qDebug() << "Using cached YukariConnect at" << cachePath;
        return cachePath;
    }

    // Ensure cache directory exists
    QDir().mkpath(QFileInfo(cachePath).absolutePath());

    // Determine download URL based on platform
    const QString downloadUrl = useMirror ? "https://gitee.com/api/v5/repos/YukariC/YukariConnect/releases/latest"
                                          : "https://api.github.com/repos/YukariC/YukariConnect/releases/latest";

    QNetworkAccessManager manager;
    QNetworkRequest request((QUrl(downloadUrl)));
    request.setRawHeader("User-Agent", APPLICATION->getUserAgent().toUtf8());
    request.setRawHeader("Accept", "application/json");

    QNetworkReply* reply = manager.get(request);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "Failed to fetch YukariConnect info:" << reply->errorString();
        reply->deleteLater();
        return QString();
    }

    const QByteArray data = reply->readAll();
    reply->deleteLater();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse YukariConnect info JSON:" << parseError.errorString();
        return QString();
    }

    const QJsonObject obj = doc.object();

    // Get the download URL for the current platform
    QString assetDownloadUrl;
    QString expectedVersion;

    expectedVersion = obj["tag_name"].toString();
    const QJsonArray assets = obj["assets"].toArray();
    for (const QJsonValue& assetVal : assets) {
        const QJsonObject asset = assetVal.toObject();
        const QString name = asset["name"].toString();

        // Match platform-specific files
        // Format: yukari-connect-{version}-{platform}-{arch}
        // Windows: yukari-connect-*-windows-{arch}.exe
        // Linux: yukari-connect-*-linux-{arch}
        // macOS: yukari-connect-*-macos-{arch}
#ifdef Q_OS_WIN32
        if (name.contains("windows") && name.endsWith(".exe")) {
            // Prefer x86_64, fallback to arm64
            if (name.contains("x86_64")) {
                assetDownloadUrl = asset["browser_download_url"].toString();
                break;
            } else if (name.contains("arm64") && assetDownloadUrl.isEmpty()) {
                assetDownloadUrl = asset["browser_download_url"].toString();
            }
        }
#elif defined(Q_OS_MACOS)
        if (name.contains("macos") && !name.endsWith(".pkg") && !name.endsWith(".tar.gz")) {
            // Prefer arm64 (Apple Silicon), fallback to x86_64
            if (name.contains("arm64")) {
                assetDownloadUrl = asset["browser_download_url"].toString();
                break;
            } else if (name.contains("x86_64") && assetDownloadUrl.isEmpty()) {
                assetDownloadUrl = asset["browser_download_url"].toString();
            }
        }
#else
        if (name.contains("linux") && !name.endsWith(".tar.gz")) {
            // Prefer x86_64, then fallback to other architectures
            if (name.contains("x86_64")) {
                assetDownloadUrl = asset["browser_download_url"].toString();
                break;
            } else if (name.contains("arm64") && assetDownloadUrl.isEmpty()) {
                assetDownloadUrl = asset["browser_download_url"].toString();
            } else if (name.contains("loongarch64") && assetDownloadUrl.isEmpty()) {
                assetDownloadUrl = asset["browser_download_url"].toString();
            } else if (name.contains("riscv64") && assetDownloadUrl.isEmpty()) {
                assetDownloadUrl = asset["browser_download_url"].toString();
            }
        }
#endif
    }

    if (assetDownloadUrl.isEmpty()) {
        qWarning() << "Could not find YukariConnect download for current platform";
        return QString();
    }

    // Download the executable
    request.setUrl(QUrl(assetDownloadUrl));
    reply = manager.get(request);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "Failed to download YukariConnect:" << reply->errorString();
        reply->deleteLater();
        return QString();
    }

    const QByteArray exeData = reply->readAll();
    reply->deleteLater();

    // Write to cache
    QFile file(cachePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open YukariConnect for writing:" << file.errorString();
        return QString();
    }

    file.write(exeData);
    file.close();

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
            qDebug() << "Saved YukariConnect metadata: version" << expectedVersion;
        }
    }

    qDebug() << "Successfully downloaded YukariConnect to" << cachePath;
    return cachePath;
}

QString YukariConnect::getBaseUrl() const
{
    return m_baseUrl;
}

void YukariConnect::setBaseUrl(const QString& url)
{
    m_baseUrl = url;
}

// ==================== WebSocket Helper Methods ====================

QString YukariConnect::getWebSocketUrl() const
{
    // Convert HTTP URL to WebSocket URL
    QString wsUrl = m_baseUrl;
    if (wsUrl.startsWith("http://")) {
        wsUrl.replace(0, 7, "ws://");
    } else if (wsUrl.startsWith("https://")) {
        wsUrl.replace(0, 8, "wss://");
    }
    return wsUrl + "/ws";
}

void YukariConnect::ensureWebSocket()
{
    if (!m_webSocket) {
        m_webSocket = new QWebSocket();
        connect(m_webSocket, &QWebSocket::textMessageReceived, this, &YukariConnect::handleWebSocketMessage);
        connect(m_webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::errorOccurred), this, [this](QAbstractSocket::SocketError error) {
            qWarning() << "WebSocket error:" << error << m_webSocket->errorString();
            emit availabilityChanged(false);
            // Reset the start flag when server becomes unavailable
            m_wasStartedSuccessfully = false;
        });
        connect(m_webSocket, &QWebSocket::disconnected, this, [this]() {
            qDebug() << "WebSocket disconnected";
            emit availabilityChanged(false);
            // Reset the start flag when server becomes unavailable
            m_wasStartedSuccessfully = false;
        });
    }
}

bool YukariConnect::ensureWebSocketConnected(int timeoutMs)
{
    ensureWebSocket();

    if (m_webSocket->state() == QAbstractSocket::ConnectedState) {
        return true;
    }

    if (m_webSocket->state() == QAbstractSocket::ConnectingState) {
        // Already connecting, wait for connection
        QEventLoop loop;
        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);
        connect(m_webSocket, &QWebSocket::connected, &loop, &QEventLoop::quit);
        connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
        timeoutTimer.start(timeoutMs);
        loop.exec();

        return m_webSocket->state() == QAbstractSocket::ConnectedState;
    }

    // Need to connect
    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    connect(m_webSocket, &QWebSocket::connected, &loop, &QEventLoop::quit);
    connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeoutTimer.start(timeoutMs);

    QString wsUrl = getWebSocketUrl();
    qDebug() << "Connecting to WebSocket:" << wsUrl;
    m_webSocket->open(QUrl(wsUrl));
    loop.exec();

    if (!timeoutTimer.isActive()) {
        qWarning() << "WebSocket connection timeout";
        return false;
    }

    if (m_webSocket->state() != QAbstractSocket::ConnectedState) {
        qWarning() << "Failed to connect to WebSocket";
        return false;
    }

    qDebug() << "WebSocket connected";
    emit availabilityChanged(true);
    return true;
}

void YukariConnect::resetWebSocket()
{
    if (m_webSocket) {
        if (m_webSocket->state() == QAbstractSocket::ConnectedState) {
            m_webSocket->close();
        }
        m_webSocket->deleteLater();
        m_webSocket = nullptr;
    }
    // Clear all pending responses
    for (auto& list : m_pendingResponses) {
        for (const auto& pending : list) {
            if (pending && pending->loop) {
                pending->loop->quit();
            }
        }
    }
    m_pendingResponses.clear();
}

void YukariConnect::handleWebSocketMessage(const QString& message)
{
    qDebug() << "WebSocket received:" << message;

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse WebSocket message:" << parseError.errorString();
        return;
    }

    const QJsonObject obj = doc.object();
    const QString command = obj["command"].toString();

    // Handle server-pushed metadata immediately.
    // Only emit signal if there are receivers connected to prevent crashes
    if (command == "get_metadata_response") {
        const auto meta = YukariConnectTypes::MetaInfo::fromJson(obj["data"].toObject());
        qDebug() << "Received metadata from server push:" << meta.version;
        // Get the metaChanged signal method to check if it has receivers
        QMetaMethod metaChangedSignal = QMetaMethod::fromSignal(&YukariConnect::metaChanged);
        if (isSignalConnected(metaChangedSignal)) {
            emit metaChanged(meta);
        }
    }

    // First check if there are pending requests waiting for this command
    if (m_pendingResponses.contains(command)) {
        auto& pendingList = m_pendingResponses[command];
        if (!pendingList.isEmpty()) {
            auto pending = pendingList.takeFirst();
            if (pending) {
                pending->response = obj["data"].toObject();
                pending->completed = true;
                if (pending->loop) {
                    pending->loop->quit();
                }
            }
            // Clean up empty lists
            if (pendingList.isEmpty()) {
                m_pendingResponses.remove(command);
            }
        }
    }

    // Then handle auto-pushed messages (emit signals for UI updates)
    if (command == "get_status_response") {
        auto state = std::make_shared<YukariConnectTypes::StateResponse>(
            YukariConnectTypes::StateResponse::fromJson(obj["data"].toObject())
        );
        emit stateChanged(*state);
        return;
    }

    if (command == "get_log_response") {
        // Handle log messages - emit as log output
        const QJsonObject data = obj["data"].toObject();
        QString logMsg = data["logMessage"].toString();
        if (!logMsg.isEmpty()) {
            emit logOutput(logMsg + "\n");
        }
        return;
    }
}

bool YukariConnect::sendWsRequest(const QString& command, const QJsonObject& data)
{
    if (!ensureWebSocketConnected(5000)) {
        return false;
    }

    QJsonObject request;
    request["command"] = command;
    request["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    // Always include data field, even if empty (server expects it)
    request["data"] = data.isEmpty() ? QJsonObject() : data;

    const QJsonDocument doc(request);
    const QString message = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

    qDebug() << "WebSocket sending:" << message;
    m_webSocket->sendTextMessage(message);
    return true;
}

QJsonObject YukariConnect::sendWsRequestAndWait(const QString& command, const QString& responseCommand, const QJsonObject& data, int timeoutMs)
{
    if (!ensureWebSocketConnected(5000)) {
        return QJsonObject();
    }

    // Create pending response tracker
    QEventLoop loop;
    auto pending = QSharedPointer<PendingWsResponse>::create();
    pending->loop = &loop;
    pending->completed = false;

    if (!m_pendingResponses.contains(responseCommand)) {
        m_pendingResponses[responseCommand] = QList<QSharedPointer<PendingWsResponse>>();
    }
    m_pendingResponses[responseCommand].append(pending);

    // Send request
    QJsonObject request;
    request["command"] = command;
    request["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    // Always include data field, even if empty (server expects it)
    request["data"] = data.isEmpty() ? QJsonObject() : data;

    const QJsonDocument doc(request);
    const QString message = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

    qDebug() << "WebSocket sending:" << message;
    m_webSocket->sendTextMessage(message);

    // Wait for response
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeoutTimer.start(timeoutMs);
    loop.exec();

    // Remove from pending list if still there (timeout case)
    if (m_pendingResponses.contains(responseCommand)) {
        auto& list = m_pendingResponses[responseCommand];
        list.removeAll(pending);
        if (list.isEmpty()) {
            m_pendingResponses.remove(responseCommand);
        }
    }

    if (!pending->completed) {
        qWarning() << "WebSocket request timeout for command:" << command;
        return QJsonObject();
    }

    return pending->response;
}

void YukariConnect::sendWsRequestAsync(const QString& command, const QJsonObject& data)
{
    if (!ensureWebSocketConnected(5000)) {
        return;
    }

    QJsonObject request;
    request["command"] = command;
    request["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    // Always include data field, even if empty (server expects it)
    request["data"] = data.isEmpty() ? QJsonObject() : data;

    const QJsonDocument doc(request);
    const QString message = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

    qDebug() << "WebSocket sending (async):" << message;
    m_webSocket->sendTextMessage(message);
}

// ==================== Public API Methods (WebSocket) ====================

bool YukariConnect::isAvailable()
{
    // In WebSocket mode, just check if WebSocket is connected
    // Don't wait for metadata response since server will push it automatically
    return m_webSocket && m_webSocket->state() == QAbstractSocket::ConnectedState;
}

std::shared_ptr<YukariConnectTypes::MetaInfo> YukariConnect::fetchMeta()
{
    // In WebSocket mode, metadata is pushed by server automatically
    // This method is kept for compatibility but does nothing
    // The actual metadata comes via metaChanged signal
    return nullptr;
}

std::shared_ptr<YukariConnectTypes::MetaInfo> YukariConnect::fetchMeta(int timeoutMs)
{
    // In WebSocket mode, metadata is pushed by server automatically
    // This method is kept for compatibility but does nothing
    // The actual metadata comes via metaChanged signal
    (void)timeoutMs;
    return nullptr;
}

std::shared_ptr<YukariConnectTypes::StateResponse> YukariConnect::fetchState()
{
    // In WebSocket mode, state is pushed by server automatically
    // This method is kept for compatibility but does nothing
    // The actual state comes via stateChanged signal
    return nullptr;
}

bool YukariConnect::panic(bool peaceful)
{
    // Use HTTP API for shutdown - more reliable for stopping the server
    // /panic is a GET request with 'peaceful' query parameter (from source code)
    QString endpoint = QString("/panic?peaceful=%1").arg(peaceful ? "true" : "false");
    const QJsonObject response = sendHttpRequest(endpoint, {}, "GET");
    bool result = !response.isEmpty();

    // Reset the success flag so isProcessRunning() returns false
    if (result) {
        m_wasStartedSuccessfully = false;
    }
    return result;
}

void YukariConnect::panicAsync(bool peaceful)
{
    // Use HTTP API for shutdown - more reliable for stopping the server
    // /panic is a GET request with 'peaceful' query parameter (from source code)
    QString endpoint = QString("/panic?peaceful=%1").arg(peaceful ? "true" : "false");
    // Send HTTP request in a separate thread to avoid blocking
    (void)QtConcurrent::run([this, endpoint]() {
        sendHttpRequest(endpoint, {}, "GET");
    });
    // Reset the flag immediately since we're shutting down
    m_wasStartedSuccessfully = false;
}

bool YukariConnect::startScanning(const QString& player)
{
    QJsonObject data;
    data["player"] = player;
    const QJsonObject response = sendWsRequestAndWait("start_host", "start_host_response", data);
    return !response.isEmpty() && response["status"].toString() == "ok";
}

bool YukariConnect::cancelState()
{
    // Use HTTP API for stopping room - more reliable
    const QJsonObject response = sendHttpRequest("/room/stop", {}, "POST");
    return !response.isEmpty();
}

void YukariConnect::cancelStateAsync()
{
    // Use HTTP API for stopping room - more reliable
    // Send HTTP request in a separate thread to avoid blocking
    (void)QtConcurrent::run([this]() {
        sendHttpRequest("/room/stop", {}, "POST");
    });
}

bool YukariConnect::joinRoom(const QString& room, const QString& player)
{
    QJsonObject data;
    data["room"] = room;
    data["player"] = player;
    const QJsonObject response = sendWsRequestAndWait("join_room", "join_room_response", data);
    return !response.isEmpty() && response["status"].toString() == "ok";
}

// ==================== HTTP API Methods ====================

QJsonObject YukariConnect::sendHttpRequest(const QString& endpoint, const QJsonObject& data, const QString& method)
{
    QNetworkAccessManager manager;
    QNetworkRequest request(QUrl(m_baseUrl + endpoint));
    request.setRawHeader("User-Agent", APPLICATION->getUserAgent().toUtf8());

    QNetworkReply* reply = nullptr;

    if (method == "GET") {
        reply = manager.get(request);
    } else if (method == "POST") {
        request.setRawHeader("Content-Type", "application/json");
        const QJsonDocument doc(data);
        const QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
        reply = manager.post(request, jsonData);
    }

    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    QJsonObject result;
    if (reply->error() == QNetworkReply::NoError) {
        const QByteArray responseData = reply->readAll();
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(responseData, &parseError);
        if (parseError.error == QJsonParseError::NoError) {
            result = doc.object();
        }
    }
    reply->deleteLater();

    return result;
}

bool YukariConnect::httpStopRoom()
{
    const QJsonObject response = sendHttpRequest("/room/stop", {}, "POST");
    return !response.isEmpty();
}

bool YukariConnect::httpRetryRoom()
{
    const QJsonObject response = sendHttpRequest("/room/retry", {}, "POST");
    return !response.isEmpty();
}

bool YukariConnect::httpStartHost(const QString& player, int scaffoldingPort)
{
    QJsonObject data;
    if (!player.isEmpty()) {
        data["playerName"] = "Host";
    } else {
        data["playerName"] = player;
    }
    if (scaffoldingPort > 0) {
        data["scaffoldingPort"] = scaffoldingPort;
    }
    // Add launcher custom string if set
    QString launcherString = QString("Luna/%1").arg(BuildConfig.versionString());
    data["launcherCustomString"] = launcherString;

    const QJsonObject response = sendHttpRequest("/room/host/start", data, "POST");
    return !response.isEmpty();
}

bool YukariConnect::httpStartGuest(const QString& room, const QString& player)
{
    QJsonObject data;
    data["roomCode"] = room;
    if (!player.isEmpty()) {
        data["playerName"] = "Guest";
    } else {
        data["playerName"] = player;
    }
    // Add launcher custom string if set
    QString launcherString = QString("Luna/%1").arg(BuildConfig.versionString());
    data["launcherCustomString"] = launcherString;

    const QJsonObject response = sendHttpRequest("/room/guest/start", data, "POST");
    return !response.isEmpty();
}

QByteArray YukariConnect::fetchLog(bool fetch)
{
    // Logs are pushed automatically via WebSocket, so we just return accumulated log lines
    QMutexLocker locker(&const_cast<QMutex&>(m_logMutex));
    return m_logLines.join("\n").toUtf8();
}

QString YukariConnect::getPortFilePath() const
{
    if (m_portFilePath.isEmpty()) {
        // Create a temporary file path for the port JSON
        // Use temp directory + yukari-connect_port.json
        QString tempDir = QDir::tempPath();
        m_portFilePath = FS::PathCombine(tempDir, "yukari-connect_port.json");
    }
    return m_portFilePath;
}

bool YukariConnect::waitForPortFile(quint32 timeoutMs)
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

bool YukariConnect::readPortFromFile(quint16& port) const
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

bool YukariConnect::startProcess()
{
    // Check if already running
    if (m_wasStartedSuccessfully) {
        // Verify the server is actually responding
        if (isAvailable()) {
            qDebug() << "YukariConnect already running and available";
            return true;
        } else {
            // Server was started but no longer responding, reset state
            qDebug() << "YukariConnect was started but no longer responding, resetting";
            m_wasStartedSuccessfully = false;
        }
    }

    if (m_process && m_process->state() == QProcess::Running) {
        qDebug() << "YukariConnect process already running";
        return true;
    }

    const QString exePath = getLocalPath();
    if (!QFile::exists(exePath)) {
        qWarning() << "YukariConnect executable not found at:" << exePath;
        return false;
    }

    // Create process
    m_process = new QProcess(this);

    // Set working directory to APPLICATION->root() so YukariConnect can find yukari.json
    m_process->setWorkingDirectory(APPLICATION->root());

    // Setup process to run in background
    m_process->setProcessChannelMode(QProcess::MergedChannels);

    // Track port detection from output
    quint16 detectedPort = 0;
    QByteArray capturedOutput;
    QEventLoop loop;
    bool portDetected = false;

    // Function to parse and detect port from output
    auto detectPortFromOutput = [&capturedOutput, &detectedPort, &portDetected, &loop]() -> bool {
        const QString output = QString::fromUtf8(capturedOutput);

        // Pattern 1: YukariConnect port info output - "YUKARI_PORT_INFO:port=5062"
        QRegularExpression yukariPortRegex(R"(YUKARI_PORT_INFO:port=(\d+))");
        QRegularExpressionMatch match = yukariPortRegex.match(output);
        if (match.hasMatch()) {
            bool ok = false;
            int port = match.captured(1).toInt(&ok);
            if (ok && port > 0 && port <= 65535) {
                detectedPort = static_cast<quint16>(port);
                qDebug() << "YukariConnect detected on port:" << detectedPort;
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
                qDebug() << "YukariConnect server mode detected on port:" << detectedPort;
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
                qWarning() << "YukariConnect process finished with non-zero exit code:" << exitCode;
            } else {
                qWarning() << "YukariConnect process exited before port was detected";
            }
            emit availabilityChanged(false);
        } else {
            qDebug() << "YukariConnect HMCL mode: process exited normally after successful start";
        }
        loop.quit();
    });

    connect(m_process, &QProcess::errorOccurred, this, [this, &loop](QProcess::ProcessError error) {
        qWarning() << "YukariConnect process error:" << error;
        m_wasStartedSuccessfully = false;
        emit availabilityChanged(false);
        loop.quit();
    });

    // Read output when available and parse for port immediately
    connect(m_process, &QProcess::readyReadStandardOutput, this, [this, &capturedOutput, &detectPortFromOutput]() {
        const QByteArray data = m_process->readAllStandardOutput();
        qDebug() << "YukariConnect:" << data;
        capturedOutput.append(data);
        detectPortFromOutput();  // Try to detect port immediately when output arrives
    });
    connect(m_process, &QProcess::readyReadStandardError, this, [this]() {
        const QByteArray data = m_process->readAllStandardError();
        qWarning() << "YukariConnect:" << data;
    });

    // Start YukariConnect without any parameters
    // YukariConnect will keep running and output logs to stdout/stderr
    qDebug() << "Starting YukariConnect:" << exePath;

    m_process->start(exePath);

    if (!m_process->waitForStarted(5000)) {
        qWarning() << "Failed to start YukariConnect process:" << m_process->errorString();
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

        // Reset any existing WebSocket connection
        resetWebSocket();

        emit availabilityChanged(true);
        qDebug() << "YukariConnect started on port:" << detectedPort;

        // Prepare launcher string before connecting
        QString launcherString = QString("Luna/%1").arg(BuildConfig.versionString());

        // Connect to WebSocket
        if (ensureWebSocketConnected(5000)) {
            qDebug() << "WebSocket connected to YukariConnect";

            // Wait a bit for the server to send initial messages
            // The server sends: get_status_response, get_room_status_response,
            // list_servers_response, list_public_servers_response, get_metadata_response
            QTimer::singleShot(500, this, [this, launcherString]() {
                // Set launcher vendor identification automatically on startup
                setLauncherCustomString(launcherString);
            });
        } else {
            qWarning() << "Failed to connect WebSocket to YukariConnect";
        }

        // Connect process output for continuous log streaming
        // YukariConnect keeps running and outputs logs continuously
        connect(m_process, &QProcess::readyReadStandardOutput, this, [this]() {
            const QByteArray data = m_process->readAllStandardOutput();
            QString output = QString::fromUtf8(data);
            emit logOutput(output);
        });
        connect(m_process, &QProcess::readyReadStandardError, this, [this]() {
            const QByteArray data = m_process->readAllStandardError();
            QString output = QString::fromUtf8(data);
            emit logOutput(output);
        });

        // Process stays running, don't delete it
        return true;
    }

    // Port not detected - check if process is still running
    if (m_process && m_process->state() == QProcess::Running) {
        qWarning() << "YukariConnect process started but port not detected";
        // Still keep the log streaming connection
        connect(m_process, &QProcess::readyReadStandardOutput, this, [this]() {
            const QByteArray data = m_process->readAllStandardOutput();
            QString output = QString::fromUtf8(data);
            emit logOutput(output);
        });
        connect(m_process, &QProcess::readyReadStandardError, this, [this]() {
            const QByteArray data = m_process->readAllStandardError();
            QString output = QString::fromUtf8(data);
            emit logOutput(output);
        });
        return false;
    }

    // Process exited or failed
    qWarning() << "YukariConnect failed to start or exited immediately";
    delete m_process;
    m_process = nullptr;
    return false;
}

void YukariConnect::stopProcess()
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

void YukariConnect::forceKillProcess()
{
    // Clean up WebSocket connection first
    resetWebSocket();

    if (m_process) {
        qDebug() << "Force killing YukariConnect process...";
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

bool YukariConnect::shutdownAndWait(int timeoutMs)
{
    // First check if process was ever started successfully
    // If we never started it or it was already marked as stopped, don't try to shut down
    if (!m_wasStartedSuccessfully) {
        qDebug() << "YukariConnect was not started, skipping shutdown";
        return true;
    }

    // Check if YukariConnect WebSocket is connected
    if (!isAvailable()) {
        // Already not connected
        qDebug() << "YukariConnect not available, assuming already stopped";
        m_wasStartedSuccessfully = false;
        return true;
    }

    // Send shutdown commands asynchronously (don't wait for response)
    cancelStateAsync();
    panicAsync(true);

    // Wait for WebSocket to disconnect
    const int sleepMs = 100;
    const int maxIterations = timeoutMs / sleepMs;
    for (int i = 0; i < maxIterations; i++) {
        QThread::msleep(sleepMs);
        if (!isAvailable()) {
            // Stopped successfully
            qDebug() << "YukariConnect stopped successfully";
            m_wasStartedSuccessfully = false;
            return true;
        }
    }

    // Timeout - force kill the process
    qDebug() << "YukariConnect shutdown timeout, force killing process...";
    forceKillProcess();
    m_wasStartedSuccessfully = false;
    return false;
}

bool YukariConnect::isProcessRunning() const
{
    // In HMCL mode, the process exits after writing the port file
    // but the web server continues running in the background
    // Check if we successfully started it and the base URL is set to something other than default
    if (m_wasStartedSuccessfully) {
        return true;
    }
    return m_process && m_process->state() == QProcess::Running;
}

void YukariConnect::setStopOnClose(bool stopOnClose)
{
    m_stopOnClose = stopOnClose;
}

bool YukariConnect::getStopOnClose() const
{
    return m_stopOnClose;
}

// ==================== YukariConnect Extension APIs (WebSocket) ====================

bool YukariConnect::setLauncherCustomString(const QString& customString)
{
    QJsonObject data;
    data["launcherCustomString"] = customString;

    const QJsonObject response = sendWsRequestAndWait("set_launcher_custom_string", "set_launcher_custom_string_response", data);
    if (response.isEmpty()) {
        qWarning() << "Failed to set launcher custom string";
        return false;
    }

    qDebug() << "YukariConnect: Launcher custom string set to:" << customString;
    return true;
}

QJsonObject YukariConnect::getRoomStatus()
{
    const QJsonObject response = sendWsRequestAndWait("get_room_status", "get_room_status_response", {});
    return response;
}

bool YukariConnect::retryRoom()
{
    // room_retry - clears Exception state and returns to Idle
    const QJsonObject response = sendWsRequestAndWait("room_retry", "room_retry_response", {});
    if (response.isEmpty()) {
        qWarning() << "Failed to retry from Exception state";
        return false;
    }

    qDebug() << "YukariConnect: Retried from Exception state, returning to Idle";
    return true;
}
