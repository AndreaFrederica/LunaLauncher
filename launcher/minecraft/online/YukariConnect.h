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
 *  its HTTP API.
 *
 *  A future version may replace this with a custom implementation.
 */

#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QProcess>
#include <QString>

#include "QObjectPtr.h"

namespace YukariConnectTypes {

// Profile kind enumeration
enum class ProfileKind { HOST, LOCAL, GUEST };

// Profile structure
struct Profile {
    QString machine_id;
    QString name;
    QString vendor;
    ProfileKind kind;

    static Profile fromJson(const QJsonObject& obj);
    QJsonObject toJson() const;
};

// Difficulty enumeration for guest-starting state
enum class Difficulty {
    UNKNOWN,
    EASIEST,
    SIMPLE,
    MEDIUM,
    TOUGH
};

// Exception type enumeration
enum class ExceptionType {
    PingHostFail = 0,       // Cannot find or connect to host scaffolding
    PingHostRst = 1,        // Connection to host reset/lost
    GuestProcessCrash = 2,  // Guest process crashed
    HostProcessCrash = 3,   // Host process crashed
    PingServerRst = 4,       // Minecraft server connection reset/lost
    ScaffoldingInvalidResponse = 5  // Scaffolding returned invalid response
};

// State enumeration
enum class State {
    Waiting,          // waiting
    HostScanning,     // host-scanning
    HostStarting,     // host-starting
    HostOk,           // host-ok
    GuestConnecting,  // guest-connecting
    GuestStarting,    // guest-starting
    GuestOk,          // guest-ok
    Exception         // exception
};

// State response structure
struct StateResponse {
    qint64 index;
    State state;
    QString room;
    QString url;
    qint64 profile_index;
    QList<Profile> profiles;

    // For guest-starting state
    Difficulty difficulty;
    // For exception state
    ExceptionType exception_type;

    static StateResponse fromJson(const QJsonObject& obj);
};

// Meta information structure
struct MetaInfo {
    QString compile_timestamp;
    QString core_version;
    QString target_arch;
    QString target_env;
    QString target_os;
    QString target_tuple;
    QString target_vendor;
    QString version;
    int yggdrasil_port;

    static MetaInfo fromJson(const QJsonObject& obj);
};

}  // namespace YukariConnectTypes

/**
 * @brief Manages YukariConnect online functionality
 *
 * This class handles communication with the YukariConnect HTTP server,
 * including status queries, room management, and panic operations.
 */
class YukariConnect : public QObject {
    Q_OBJECT

   public:
    static YukariConnect& instance();

    /**
     * @brief Get the local path to YukariConnect executable
     * @return Absolute path to the cached executable
     */
    QString getLocalPath() const;

    /**
     * @brief Get the path to the metadata JSON file
     * @return Absolute path to the metadata file
     */
    QString getMetadataPath() const;

    /**
     * @brief Get the currently installed version
     * @return Version string, or empty string if not installed/version unknown
     */
    QString getVersion() const;

    /**
     * @brief Check if the cached executable file exists and is valid
     * @return true if cache is valid
     */
    bool checkCache() const;

    /**
     * @brief Download the latest version of YukariConnect
     * @param useMirror Whether to use mirror instead of GitHub
     * @return Path to the downloaded executable, or empty string on failure
     */
    QString downloadLatest(bool useMirror = false);

    /**
     * @brief Get the base URL for YukariConnect API
     * @return The base URL (default: http://localhost:12580)
     */
    QString getBaseUrl() const;

    /**
     * @brief Set the base URL for YukariConnect API
     * @param url The new base URL
     */
    void setBaseUrl(const QString& url);

    /**
     * @brief Check if YukariConnect server is available
     * @return true if server is responding
     */
    bool isAvailable();

    /**
     * @brief Fetch meta information from the server
     * @return MetaInfo object, or null on failure
     */
    std::shared_ptr<YukariConnectTypes::MetaInfo> fetchMeta();

    /**
     * @brief Fetch meta information from the server with custom timeout
     * @param timeoutMs Timeout in milliseconds
     * @return MetaInfo object, or null on failure
     */
    std::shared_ptr<YukariConnectTypes::MetaInfo> fetchMeta(int timeoutMs);

    /**
     * @brief Fetch current state from the server
     * @return StateResponse object, or null on failure
     */
    std::shared_ptr<YukariConnectTypes::StateResponse> fetchState();

    /**
     * @brief Panic - stop YukariConnect online mode
     * @param peaceful Whether to exit peacefully (graceful shutdown)
     * @return true if request was successful
     */
    bool panic(bool peaceful = true);

    /**
     * @brief Panic - stop YukariConnect online mode (fire and forget, does not wait for response)
     * @param peaceful Whether to exit peacefully (graceful shutdown)
     */
    void panicAsync(bool peaceful = true);

    /**
     * @brief Switch to scanning mode (host)
     * @param player Player name
     * @return true if request was successful
     */
    bool startScanning(const QString& player);

    /**
     * @brief Cancel current state and return to idle
     * @return true if request was successful
     */
    bool cancelState();

    /**
     * @brief Cancel current state and return to idle (fire and forget, does not wait for response)
     */
    void cancelStateAsync();

    /**
     * @brief Join a room as guest
     * @param room Room invite code
     * @param player Player name
     * @return true if successfully joined, false otherwise
     */
    bool joinRoom(const QString& room, const QString& player);

    /**
     * @brief Fetch logs from the server
     * @param fetch Whether this is a fetch request
     * @return Log data as byte array, or empty on failure
     */
    QByteArray fetchLog(bool fetch = true);

    /**
     * @brief Start the YukariConnect process
     * @return true if process was started successfully
     */
    bool startProcess();

    /**
     * @brief Stop the YukariConnect process (graceful shutdown only)
     */
    void stopProcess();

    /**
     * @brief Force kill the YukariConnect process immediately
     */
    void forceKillProcess();

    /**
     * @brief Shutdown YukariConnect gracefully and wait for it to stop
     * @param timeoutMs Maximum time to wait for shutdown (default 8000ms)
     * @return true if YukariConnect stopped, false if timeout (but we assume it stopped)
     */
    bool shutdownAndWait(int timeoutMs = 8000);

    /**
     * @brief Check if YukariConnect process is running
     * @return true if process is running
     */
    [[nodiscard]] bool isProcessRunning() const;

    /**
     * @brief Set whether to stop YukariConnect process when launcher closes
     * @param stopOnClose true to stop on launcher close
     */
    void setStopOnClose(bool stopOnClose);

    /**
     * @brief Check if YukariConnect should be stopped when launcher closes
     * @return true if YukariConnect will be stopped on close
     */
    [[nodiscard]] bool getStopOnClose() const;

   signals:
    /**
     * @brief Emitted when the state changes
     */
    void stateChanged(const YukariConnectTypes::StateResponse& state);

    /**
     * @brief Emitted when connection to YukariConnect server changes
     */
    void availabilityChanged(bool available);

   private:
    YukariConnect(QObject* parent = nullptr);
    ~YukariConnect() = default;

    QByteArray sendGetRequest(const QString& endpoint, const QMap<QString, QString>& params = {}, int timeoutMs = 5000);
    // Returns true if HTTP request succeeded (status 200), regardless of response body
    bool sendGetRequestWithStatus(const QString& endpoint, const QMap<QString, QString>& params = {}, int timeoutMs = 5000);
    // Send request without waiting for response (fire and forget)
    void sendGetRequestAsync(const QString& endpoint, const QMap<QString, QString>& params = {});
    QString getPortFilePath() const;
    bool waitForPortFile(quint32 timeoutMs = 15000);
    bool readPortFromFile(quint16& port) const;

   private:
    QString m_baseUrl;
    QProcess* m_process = nullptr;
    mutable QString m_portFilePath;
    bool m_wasStartedSuccessfully = false;  // Track if HMCL mode started successfully
    bool m_stopOnClose = true;  // Whether to stop YukariConnect when launcher closes (default: true)
    static YukariConnect* s_instance;
};
