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
 *  solution for Minecraft. YukariConnect is developed by burningtnt and licensed
 *  under MPL-2.0.
 *
 *  Project URL: https://github.com/ElicaseTech/YukariConnect
 *
 *  This integration is implemented as a temporary solution. The launcher
 *  communicates with the standalone YukariConnect binary via its HTTP API only.
 *
 *  A future version will replace this with a custom implementation.
 */

#include "YukariConnectOnlinePanel.h"
#include "ui_YukariConnectOnlinePanel.h"

#include <QClipboard>
#include <QFileDialog>
#include <QRegularExpression>
#include <QTimer>
#include <QDateTime>
#include <QThread>
#include <QGuiApplication>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>

#include "Application.h"
#include "DesktopServices.h"
#include "minecraft/online/YukariConnect.h"
#include "settings/SettingsObject.h"

// Initialize static members
int YukariConnectOnlinePanel::s_globalPollingInterval = 1000;
int YukariConnectOnlinePanel::s_globalMaxLogLines = 1000;

YukariConnectOnlinePanel::YukariConnectOnlinePanel(QWidget* parent) : QMainWindow(parent), ui(new Ui::YukariConnectOnlinePanel)
{
    ui->setupUi(this);

    // Set window flags to ensure this behaves as a normal independent window
    // that can be placed behind other windows (not always-on-top)
    setWindowFlags(Qt::Window);

    // Load saved player name
    loadPlayerName();

    // Use global settings
    m_pollingInterval = s_globalPollingInterval;
    m_maxLogLines = s_globalMaxLogLines;

    // Create delayed save timer for player name (saves 1 second after user stops typing)
    m_playerNameSaveTimer = new QTimer(this);
    m_playerNameSaveTimer->setSingleShot(true);
    m_playerNameSaveTimer->setInterval(1000);  // 1 second delay
    connect(m_playerNameSaveTimer, &QTimer::timeout, this, [this]() {
        savePlayerName();
    });

    // Note: In WebSocket mode, state and metadata are pushed by server automatically
    // We don't need to poll for state changes anymore
    // The stateChanged signal will be emitted when server pushes state updates
    // The polling timer is kept for compatibility but does nothing in WebSocket mode
    m_statePollTimer = new QTimer(this);
    m_statePollTimer->setInterval(m_pollingInterval);
    connect(m_statePollTimer, &QTimer::timeout, this, [this]() {
        // In WebSocket mode, we don't poll - server pushes updates automatically
        // This timer is kept for compatibility but does nothing
    });

    // Connect buttons
    connect(ui->pushButton_refresh, &QPushButton::clicked, this, &YukariConnectOnlinePanel::onRefreshClicked);
    connect(ui->pushButton_open_webui, &QPushButton::clicked, this, &YukariConnectOnlinePanel::onOpenWebUIClicked);
    connect(ui->pushButton_host, &QPushButton::clicked, this, &YukariConnectOnlinePanel::onHostClicked);
    connect(ui->pushButton_join, &QPushButton::clicked, this, &YukariConnectOnlinePanel::onJoinClicked);
    connect(ui->pushButton_close_room, &QPushButton::clicked, this, &YukariConnectOnlinePanel::onCloseRoomClicked);
    connect(ui->pushButton_stop_server, &QPushButton::clicked, this, &YukariConnectOnlinePanel::onToggleServerClicked);
    connect(ui->pushButton_restart, &QPushButton::clicked, this, &YukariConnectOnlinePanel::onRestartClicked);
    connect(ui->pushButton_panic, &QPushButton::clicked, this, &YukariConnectOnlinePanel::onPanicClicked);
    connect(ui->pushButton_copy_code, &QPushButton::clicked, this, &YukariConnectOnlinePanel::onCopyCodeClicked);
    connect(ui->pushButton_fetch_log, &QPushButton::clicked, this, &YukariConnectOnlinePanel::onFetchLogClicked);
    connect(ui->pushButton_export_log, &QPushButton::clicked, this, &YukariConnectOnlinePanel::onExportLogClicked);
    connect(ui->pushButton_clear_log, &QPushButton::clicked, this, &YukariConnectOnlinePanel::onClearLogClicked);
    connect(ui->pushButton_about, &QPushButton::clicked, this, &YukariConnectOnlinePanel::onAboutClicked);

    // Connect player name text changed to restart save timer
    connect(ui->lineEdit_player_name, &QLineEdit::textChanged, this, [this]() {
        // Restart timer whenever user types
        m_playerNameSaveTimer->start();
    });

    // Connect to YukariConnect signals
    connect(&YukariConnect::instance(), &YukariConnect::stateChanged, this, &YukariConnectOnlinePanel::onStateChanged);
    connect(&YukariConnect::instance(), &YukariConnect::availabilityChanged, this, &YukariConnectOnlinePanel::onAvailabilityChanged);
    connect(&YukariConnect::instance(), &YukariConnect::metaChanged, this, &YukariConnectOnlinePanel::onMetaChanged);
    connect(&YukariConnect::instance(), &YukariConnect::logOutput, this, &YukariConnectOnlinePanel::onLogOutput);

    // Show current connection port and server button state
    updatePortDisplay();
    updateServerButtonState();

    // Auto-start YukariConnect process if not running (delayed to avoid blocking UI)
    QTimer::singleShot(100, this, [this]() {
        // Check if process is already running
        if (!YukariConnect::instance().isProcessRunning()) {
            ui->label_state_value->setText(tr("Starting..."));
            appendLog(tr("Starting YukariConnect process..."));

            if (YukariConnect::instance().startProcess()) {
                appendLog(tr("YukariConnect process started successfully."));
                ui->label_state_value->setText(tr("Connected"));
                m_serverRunning = true;
                updatePortDisplay();
                // In WebSocket mode, server will push initial state automatically
            } else {
                appendLog(tr("Failed to start YukariConnect process. Please check if it is installed."));
                ui->label_state_value->setText(tr("Failed"));
                m_serverRunning = false;
            }
        } else {
            // Server is already running, wait for WebSocket connection
            ui->label_state_value->setText(tr("Connected"));
        }
        // Update button state after initial check
        updateServerButtonState();
    });
}

YukariConnectOnlinePanel::~YukariConnectOnlinePanel()
{
    delete ui;
}

void YukariConnectOnlinePanel::setPollingInterval(int intervalMs)
{
    s_globalPollingInterval = intervalMs;
}

void YukariConnectOnlinePanel::setMaxLogLines(int maxLines)
{
    s_globalMaxLogLines = maxLines;
}

void YukariConnectOnlinePanel::retranslate()
{
    ui->retranslateUi(this);
}

void YukariConnectOnlinePanel::onRefreshClicked()
{
    if (m_isRefreshing) {
        return;
    }
    m_isRefreshing = true;
    ui->pushButton_refresh->setEnabled(false);

    // Start process if not running
    if (!YukariConnect::instance().isProcessRunning()) {
        appendLog(tr("YukariConnect not running, starting..."));
        if (!YukariConnect::instance().startProcess()) {
            appendLog(tr("Failed to start YukariConnect process."));
            ui->label_state_value->setText(tr("Not Running"));
            setUIEnabled(false);
            ui->pushButton_refresh->setEnabled(true);
            m_isRefreshing = false;
            return;
        }
        // Give it a moment to start up
        QThread::msleep(500);
    }

    appendLog(tr("Refreshing state..."));

    // In WebSocket mode, server pushes state automatically
    // Just check if WebSocket is connected
    if (YukariConnect::instance().isAvailable()) {
        appendLog(tr("WebSocket connected. Waiting for server to push state updates..."));
        updatePortDisplay();  // Update port display
    } else {
        appendLog(tr("Failed to connect to YukariConnect server."));
        ui->label_state_value->setText(tr("Error"));
        // Disable most UI, but keep Start/Stop Server button enabled
        ui->groupBox_actions->setEnabled(false);
        ui->pushButton_stop_server->setEnabled(true);
        // Show "Start Server" since connection failed
        ui->pushButton_stop_server->setText(tr("Start Server"));
        ui->pushButton_stop_server->setToolTip(tr("Start YukariConnect server"));
    }

    ui->pushButton_refresh->setEnabled(true);
    m_isRefreshing = false;
}

void YukariConnectOnlinePanel::onOpenWebUIClicked()
{
    QString url = YukariConnect::instance().getBaseUrl();
    if (url.isEmpty()) {
        appendLog(tr("YukariConnect server URL not available. Please start server first."));
        return;
    }
    appendLog(tr("Opening YukariConnect Web UI in browser: %1").arg(url));
    DesktopServices::openUrl(QUrl(url));
}

void YukariConnectOnlinePanel::onHostClicked()
{
    QString playerName = ui->lineEdit_player_name->text().trimmed();
    if (playerName.isEmpty()) {
        QMessageBox::warning(this, tr("Missing Player Name"), tr("Please enter a player name."));
        return;
    }

    // Save player name when hosting
    savePlayerName();

    appendLog(tr("Starting scanning mode for player: %1").arg(playerName));

    if (YukariConnect::instance().startScanning(playerName)) {
        appendLog(tr("Successfully entered scanning mode. Waiting for room creation..."));
        // In WebSocket mode, server will push state updates automatically
    } else {
        appendLog(tr("Failed to enter scanning mode."));
    }
}

void YukariConnectOnlinePanel::onJoinClicked()
{
    QString roomCode = ui->lineEdit_join_code->text().trimmed();
    if (roomCode.isEmpty()) {
        QMessageBox::warning(this, tr("Missing Room Code"), tr("Please enter a room code."));
        return;
    }

    QString playerName = ui->lineEdit_player_name->text().trimmed();
    if (playerName.isEmpty()) {
        QMessageBox::warning(this, tr("Missing Player Name"), tr("Please enter a player name."));
        return;
    }

    // Save player name when joining
    savePlayerName();

    appendLog(tr("Joining room: %1 as player: %2").arg(roomCode, playerName));

    if (YukariConnect::instance().joinRoom(roomCode, playerName)) {
        appendLog(tr("Successfully joined room. Waiting for connection..."));
        // In WebSocket mode, server will push state updates automatically
    } else {
        appendLog(tr("Failed to join room. Check room code and try again."));
        QMessageBox::warning(this, tr("Join Failed"), tr("Failed to join room. The room code may be invalid or room is full."));
    }
}

void YukariConnectOnlinePanel::onCloseRoomClicked()
{
    appendLog(tr("Closing room and returning to idle..."));

    if (YukariConnect::instance().cancelState()) {
        appendLog(tr("Room closed successfully. Back to idle state."));
        // In WebSocket mode, server will push state updates automatically
    } else {
        appendLog(tr("Failed to close room."));
    }
}

void YukariConnectOnlinePanel::onToggleServerClicked()
{
    // Check button text to determine action (more reliable than process state)
    bool isStopAction = ui->pushButton_stop_server->text() == tr("Stop Server");

    if (isStopAction) {
        // Server is running - stop it
        auto reply = QMessageBox::question(this, tr("Stop Server"),
                                            tr("Are you sure you want to stop YukariConnect server?\n\nThis will gracefully shut down the server and close all connections."),
                                            QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            appendLog(tr("Stopping YukariConnect server..."));

            // Stop polling before shutdown
            stopPollingState();

            // Use the new shutdown method that handles everything
            bool stopped = YukariConnect::instance().shutdownAndWait();

            if (stopped) {
                appendLog(tr("YukariConnect server stopped gracefully."));
            } else {
                appendLog(tr("YukariConnect server shutdown timeout, assuming stopped."));
            }

            // Update state
            m_serverRunning = false;
            ui->label_state_value->setText(tr("Stopped"));
            updateServerButtonState();
            ui->groupBox_actions->setEnabled(true);
        }
    } else {
        // Server is not running or not available - start it
        appendLog(tr("Starting YukariConnect server..."));

        // First force kill any existing process to ensure clean state
        if (YukariConnect::instance().isProcessRunning()) {
            appendLog(tr("Cleaning up existing process..."));
            YukariConnect::instance().forceKillProcess();
            QThread::msleep(500);  // Brief pause to let cleanup complete
        }

        if (YukariConnect::instance().startProcess()) {
            appendLog(tr("YukariConnect server started successfully."));
            // Update state
            m_serverRunning = true;
            ui->label_state_value->setText(tr("Connected"));
            updatePortDisplay();
            updateServerButtonState();
            // In WebSocket mode, server will push state updates automatically
        } else {
            appendLog(tr("Failed to start YukariConnect server. Please check if it is installed."));
            m_serverRunning = false;
            ui->label_state_value->setText(tr("Failed"));
            updateServerButtonState();
        }
    }
}

void YukariConnectOnlinePanel::onRestartClicked()
{
    auto reply = QMessageBox::question(this, tr("Restart Server"),
                                        tr("Are you sure you want to restart YukariConnect server?\n\nThis will disconnect all active connections."),
                                        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        appendLog(tr("Restarting YukariConnect server..."));

        // Stop polling before restart
        stopPollingState();

        // Cancel any active state first
        YukariConnect::instance().cancelState();

        // Stop current process
        YukariConnect::instance().stopProcess();

        // Wait a moment for it to fully stop, then restart
        QTimer::singleShot(500, this, [this]() {
            if (YukariConnect::instance().startProcess()) {
                appendLog(tr("YukariConnect server restarted successfully."));
                m_serverRunning = true;
                ui->label_state_value->setText(tr("Connected"));
                updatePortDisplay();
                updateServerButtonState();
                // In WebSocket mode, server will push state updates automatically
            } else {
                appendLog(tr("Failed to restart YukariConnect server."));
                m_serverRunning = false;
                ui->label_state_value->setText(tr("Failed"));
                updateServerButtonState();
            }
        });
    }
}

void YukariConnectOnlinePanel::onPanicClicked()
{
    auto reply = QMessageBox::question(this, tr("Emergency Stop"),
                                        tr("Are you sure you want to emergency stop YukariConnect?\n\nThis will forcefully stop all P2P connections."),
                                        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        appendLog(tr("Emergency stop initiated..."));

        if (YukariConnect::instance().panic(false)) {
            appendLog(tr("Emergency stop successful."));
            // Update button to show Start Server
            ui->pushButton_stop_server->setText(tr("Start Server"));
            ui->pushButton_stop_server->setToolTip(tr("Start YukariConnect server"));
            // In WebSocket mode, server will push state updates automatically
        } else {
            appendLog(tr("Emergency stop failed."));
        }
    }
}

void YukariConnectOnlinePanel::onCopyCodeClicked()
{
    QString code = ui->lineEdit_room_code->text();
    if (code.isEmpty()) {
        QMessageBox::information(this, tr("No Room Code"), tr("No room code to copy. Create or join a room first."));
        return;
    }

    QClipboard* clipboard = QGuiApplication::clipboard();
    clipboard->setText(code);
    appendLog(tr("Room code copied to clipboard: %1").arg(code));
}

void YukariConnectOnlinePanel::onFetchLogClicked()
{
    appendLog(tr("Fetching logs from YukariConnect server..."));

    QByteArray logData = YukariConnect::instance().fetchLog(true);
    if (!logData.isEmpty()) {
        ui->plainTextEdit_logs->setPlainText(QString::fromUtf8(logData));
        appendLog(tr("Logs fetched successfully."));
    } else {
        appendLog(tr("Failed to fetch logs."));
    }
}

void YukariConnectOnlinePanel::onExportLogClicked()
{
    QString logText = ui->plainTextEdit_logs->toPlainText();
    if (logText.isEmpty()) {
        QMessageBox::information(this, tr("No Logs"), tr("No logs to export."));
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(this, tr("Export Logs"), "yukariconnect_logs.txt", tr("Text Files (*.txt);;All Files (*.*)"));
    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << logText;
        file.close();
        appendLog(tr("Logs exported to: %1").arg(fileName));
    } else {
        QMessageBox::warning(this, tr("Export Failed"), tr("Failed to save logs to file."));
    }
}

void YukariConnectOnlinePanel::onClearLogClicked()
{
    ui->plainTextEdit_logs->clear();
}

void YukariConnectOnlinePanel::onAboutClicked()
{
    QString aboutText = tr(
        "<h3>YukariConnect P2P Multiplayer</h3>"
        "<p><b>P2P Multiplayer Solution</b></p>"
        "<p>YukariConnect is a P2P multiplayer solution for Minecraft that allows players to connect without port forwarding or central servers.</p>"
        "<h4>YukariConnect Project</h4>"
        "<p><b>Developer:</b> AndreaFrederica  AuraElicase<br>"
        "<b>Project URL:</b> <a href=\"https://github.com/ElicaseTech/YukariConnect\">https://github.com/ElicaseTech/YukariConnect</a></p>"
        "<h4>License</h4>"
        "<p>YukariConnect is licensed under <b>Mozilla Public License 2.0 (MPL-2.0)</b>.</p>"
    );

    QMessageBox::about(this, tr("About YukariConnect"), aboutText);
}

void YukariConnectOnlinePanel::onStateChanged(const YukariConnectTypes::StateResponse& state)
{
    // Track if this is a state transition
    bool stateChanged = (m_lastState != state.state);
    m_lastState = state.state;

    // Handle Exception state with friendly message (only on transition)
    if (state.state == YukariConnectTypes::State::Exception && stateChanged) {
        handleExceptionState(state);
    }

    // For HostOk/GuestOk states, monitor player changes
    if (state.state == YukariConnectTypes::State::HostOk ||
        state.state == YukariConnectTypes::State::GuestOk) {
        // In HostOk or GuestOk state, monitor player list changes
        if (stateChanged) {
            // Just reached this state, log it
            appendLog(tr("Now monitoring for player changes..."));
        }
        // Check if player list changed (detected by profile_index change)
        if (state.profile_index != m_lastProfileIndex) {
            if (!stateChanged) {  // Only log if not initial state change
                appendLog(tr("Player list updated (index: %1)").arg(state.profile_index));
            }
            m_lastProfileIndex = state.profile_index;
        }
    }

    updateStateDisplay(state);
}

void YukariConnectOnlinePanel::onAvailabilityChanged(bool available)
{
    updatePortDisplay();  // Update port display when availability changes

    // Only log when availability actually changes
    if (available != m_lastAvailable) {
        m_lastAvailable = available;
        if (available) {
            appendLog(tr("YukariConnect server is now available."));
        } else {
            appendLog(tr("YukariConnect server is not available."));
        }
    }

    // Update server running state based on availability
    m_serverRunning = available;

    if (available) {
        setUIEnabled(true);
        updateServerButtonState();  // Update button based on actual process state
    } else {
        // Disable most UI, but keep Start/Stop Server button enabled
        // so users can restart the server
        ui->pushButton_host->setEnabled(false);
        ui->pushButton_join->setEnabled(false);
        ui->pushButton_close_room->setEnabled(false);
        ui->pushButton_stop_server->setEnabled(true);  // Keep Start/Stop Server enabled

        // When server is not available, show "Start Server" button
        updateServerButtonState();
    }
}

void YukariConnectOnlinePanel::onMetaChanged(const YukariConnectTypes::MetaInfo& meta)
{
    // Handle metadata changes - update version display if panel is visible
    if (isVisible()) {
        // Only update UI if panel is visible to prevent crashes
        qDebug() << "Meta changed, updating version display:" << meta.version;
        // Version display is handled in the main window, not in this panel
    }
}

void YukariConnectOnlinePanel::updateStateDisplay(const YukariConnectTypes::StateResponse& state)
{
    // Update state label with color
    ui->label_state_value->setText(getStateString(state.state));

    // Set color based on state (same scheme as main window status bar)
    QString color;
    switch (state.state) {
        case YukariConnectTypes::State::Waiting:
        case YukariConnectTypes::State::HostScanning:
            color = "gray";
            break;
        case YukariConnectTypes::State::HostStarting:
            color = "orange";
            break;
        case YukariConnectTypes::State::HostOk:
            color = "green";
            break;
        case YukariConnectTypes::State::GuestConnecting:
        case YukariConnectTypes::State::GuestStarting:
            color = "orange";
            break;
        case YukariConnectTypes::State::GuestOk:
            color = "green";
            break;
        case YukariConnectTypes::State::Exception:
            color = "red";
            break;
        default:
            color = "green";
            break;
    }
    ui->label_state_value->setStyleSheet(QString("color: %1; font-weight: bold;").arg(color));

    // Update hint text based on state
    updateStateHint(state.state);

    // Update server button state based on process running status
    updateServerButtonState();

    // Update room info only if changed (diff check to avoid flicker)
    QString newRoomCode = !state.room.isEmpty() ? state.room : tr("-");
    if (m_lastRoomCode != newRoomCode) {
        ui->lineEdit_room_code->setText(newRoomCode);
        m_lastRoomCode = newRoomCode;
    }

    QString newUrl = !state.url.isEmpty() ? state.url : tr("-");
    if (m_lastUrl != newUrl) {
        ui->lineEdit_url->setText(newUrl);
        m_lastUrl = newUrl;
    }

    // Update connection difficulty
    QString difficultyText = getDifficultyString(state.difficulty);
    ui->lineEdit_difficulty->setText(difficultyText);

    // Update player list only if profiles changed (check count and contents)
    bool profilesChanged = (m_lastProfiles.size() != state.profiles.size());
    if (!profilesChanged) {
        // Check if any profile is different
        const auto profileCount = static_cast<int>(state.profiles.size());
        for (int i = 0; i < profileCount; ++i) {
            const auto& p1 = m_lastProfiles[i];
            const auto& p2 = state.profiles[i];
            if (p1.name != p2.name || p1.machine_id != p2.machine_id ||
                p1.vendor != p2.vendor || p1.kind != p2.kind) {
                profilesChanged = true;
                break;
            }
        }
    }

    if (profilesChanged) {
        updatePlayerList(state.profiles);
        m_lastProfiles = state.profiles;
    }

    // Update UI based on state
    switch (state.state) {
        case YukariConnectTypes::State::Waiting:
        case YukariConnectTypes::State::HostScanning:
            // In idle or scanning mode - can start hosting or joining
            ui->pushButton_host->setEnabled(true);
            ui->pushButton_join->setEnabled(true);
            ui->pushButton_close_room->setEnabled(false);
            break;
        case YukariConnectTypes::State::HostOk:
            // Hosting a room - can close room
            ui->pushButton_host->setEnabled(false);
            ui->pushButton_join->setEnabled(false);
            ui->pushButton_close_room->setEnabled(true);
            break;
        case YukariConnectTypes::State::GuestOk:
            // Joined as guest - can leave room
            ui->pushButton_host->setEnabled(false);
            ui->pushButton_join->setEnabled(false);
            ui->pushButton_close_room->setEnabled(true);
            break;
        case YukariConnectTypes::State::Exception:
            // Exception state - room is closed, can start a new room
            ui->pushButton_host->setEnabled(true);
            ui->pushButton_join->setEnabled(true);
            ui->pushButton_close_room->setEnabled(false);
            break;
        default:
            // In transition states (connecting, starting, etc.)
            ui->pushButton_host->setEnabled(false);
            ui->pushButton_join->setEnabled(false);
            ui->pushButton_close_room->setEnabled(true);
            break;
    }

    setUIEnabled(true);
}

void YukariConnectOnlinePanel::updatePlayerList(const QList<YukariConnectTypes::Profile>& profiles)
{
    ui->tableWidget_players->setRowCount(profiles.size());

    for (int i = 0; i < profiles.size(); ++i) {
        const auto& profile = profiles[i];

        ui->tableWidget_players->setItem(i, 0, new QTableWidgetItem(profile.name));
        ui->tableWidget_players->setItem(i, 1, new QTableWidgetItem(profile.machine_id));
        ui->tableWidget_players->setItem(i, 2, new QTableWidgetItem(profile.vendor));

        QString kindStr;
        switch (profile.kind) {
            case YukariConnectTypes::ProfileKind::HOST:
                kindStr = tr("Host");
                break;
            case YukariConnectTypes::ProfileKind::GUEST:
                kindStr = tr("Guest");
                break;
            default:
                kindStr = tr("Local");
                break;
        }
        ui->tableWidget_players->setItem(i, 3, new QTableWidgetItem(kindStr));
    }
}

void YukariConnectOnlinePanel::appendLog(const QString& message)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    ui->plainTextEdit_logs->appendPlainText(QString("[%1] %2").arg(timestamp, message));

    // Trim log to max lines if set (0 means unlimited)
    if (m_maxLogLines > 0) {
        QTextDocument* doc = ui->plainTextEdit_logs->document();
        int lineCount = doc->blockCount();
        if (lineCount > m_maxLogLines) {
            // Remove excess lines from beginning
            QTextCursor cursor(doc);
            cursor.movePosition(QTextCursor::Start);
            while (doc->blockCount() > m_maxLogLines) {
                cursor.select(QTextCursor::LineUnderCursor);
                cursor.removeSelectedText();
                cursor.deleteChar();  // Remove newline
            }
        }
    }
}

QString YukariConnectOnlinePanel::getStateString(YukariConnectTypes::State state) const
{
    switch (state) {
        case YukariConnectTypes::State::Waiting:
            return tr("Waiting");
        case YukariConnectTypes::State::HostScanning:
            return tr("Host Scanning");
        case YukariConnectTypes::State::HostStarting:
            return tr("Host Starting");
        case YukariConnectTypes::State::HostOk:
            return tr("Host Ready");
        case YukariConnectTypes::State::GuestConnecting:
            return tr("Guest Connecting");
        case YukariConnectTypes::State::GuestStarting:
            return tr("Guest Starting");
        case YukariConnectTypes::State::GuestOk:
            return tr("Guest Connected");
        case YukariConnectTypes::State::Exception:
            return tr("Exception");
        default:
            return tr("Unknown");
    }
}

void YukariConnectOnlinePanel::updateStateHint(YukariConnectTypes::State state)
{
    QString hint;
    switch (state) {
        case YukariConnectTypes::State::Waiting:
            hint = tr("Enter your player name and click Create Room to host, or enter a room code to join.");
            break;
        case YukariConnectTypes::State::HostScanning:
            hint = tr("Open your Minecraft world, press Esc, and click 'Open to LAN' to start LAN server.");
            break;
        case YukariConnectTypes::State::HostStarting:
            hint = tr("Starting your room... The server will be ready shortly.");
            break;
        case YukariConnectTypes::State::HostOk:
            hint = tr("Your room is ready! Share the room code with friends so they can join.");
            break;
        case YukariConnectTypes::State::GuestConnecting:
            hint = tr("Connecting to the host room... This may take a moment.");
            break;
        case YukariConnectTypes::State::GuestStarting:
            hint = tr("Setting up connection to Minecraft server...");
            break;
        case YukariConnectTypes::State::GuestOk:
            hint = tr("Connected! You can now join the game using the connection URL above.");
            break;
        case YukariConnectTypes::State::Exception:
            hint = tr("An error occurred. Check the logs for details and try creating or joining a room again.");
            break;
        default:
            hint = tr("Please wait...");
            break;
    }
    ui->label_state_hint->setText(hint);
}

void YukariConnectOnlinePanel::setUIEnabled(bool enabled)
{
    ui->groupBox_actions->setEnabled(enabled);
    ui->pushButton_refresh->setEnabled(enabled);
}

void YukariConnectOnlinePanel::startPollingState()
{
    // In WebSocket mode, we don't need to poll - server pushes updates automatically
    // This method is kept for compatibility but does nothing in WebSocket mode
    appendLog(tr("WebSocket mode: Server will push state updates automatically."));
}

QString YukariConnectOnlinePanel::getExceptionString(YukariConnectTypes::ExceptionType type) const
{
    switch (type) {
        case YukariConnectTypes::ExceptionType::PingHostFail:
            return tr("Cannot connect to host");
        case YukariConnectTypes::ExceptionType::PingHostRst:
            return tr("Connection to host lost");
        case YukariConnectTypes::ExceptionType::GuestProcessCrash:
            return tr("Process crashed (guest)");
        case YukariConnectTypes::ExceptionType::HostProcessCrash:
            return tr("Process crashed (host)");
        case YukariConnectTypes::ExceptionType::PingServerRst:
            return tr("Minecraft server closed");
        case YukariConnectTypes::ExceptionType::ScaffoldingInvalidResponse:
            return tr("Invalid server response");
        default:
            return tr("Unknown error");
    }
}

QString YukariConnectOnlinePanel::getDifficultyString(YukariConnectTypes::Difficulty difficulty) const
{
    switch (difficulty) {
        case YukariConnectTypes::Difficulty::EASIEST:
            return tr("Easiest");
        case YukariConnectTypes::Difficulty::SIMPLE:
            return tr("Simple");
        case YukariConnectTypes::Difficulty::MEDIUM:
            return tr("Medium");
        case YukariConnectTypes::Difficulty::TOUGH:
            return tr("Tough");
        case YukariConnectTypes::Difficulty::UNKNOWN:
        default:
            return tr("Unknown");
    }
}

void YukariConnectOnlinePanel::handleExceptionState(const YukariConnectTypes::StateResponse& state)
{
    QString exceptionMsg = getExceptionString(state.exception_type);

    // Provide friendly messages for common exception types
    switch (state.exception_type) {
        case YukariConnectTypes::ExceptionType::PingServerRst:
            // Minecraft game exited - this is normal behavior
            appendLog(tr("Room closed: Minecraft game has exited."));
            appendLog(tr("You can start a new room whenever you're ready to play again."));
            break;
        case YukariConnectTypes::ExceptionType::PingHostRst:
            appendLog(tr("Connection lost: The host closed the room or disconnected."));
            break;
        case YukariConnectTypes::ExceptionType::PingHostFail:
            appendLog(tr("Connection failed: Unable to reach the host room."));
            break;
        case YukariConnectTypes::ExceptionType::GuestProcessCrash:
        case YukariConnectTypes::ExceptionType::HostProcessCrash:
            appendLog(tr("Error: Process crashed. Reason: %1").arg(exceptionMsg));
            appendLog(tr("Try restarting YukariConnect server."));
            break;
        case YukariConnectTypes::ExceptionType::ScaffoldingInvalidResponse:
            appendLog(tr("Protocol error: Received invalid response from server."));
            break;
        default:
            appendLog(tr("Exception occurred: %1").arg(exceptionMsg));
            break;
    }

    // Automatically retry from Exception state to return to Idle
    appendLog(tr("Auto-recovering from exception state..."));
    if (YukariConnect::instance().retryRoom()) {
        appendLog(tr("Successfully recovered to idle state."));
        // In WebSocket mode, server will push state updates automatically
    } else {
        appendLog(tr("Failed to recover from exception state. You may need to restart the server."));
    }
}

void YukariConnectOnlinePanel::stopPollingState()
{
    // In WebSocket mode, we don't need to poll - server pushes updates automatically
    // This method is kept for compatibility but does nothing in WebSocket mode
    appendLog(tr("Stopped listening for state updates."));
}

void YukariConnectOnlinePanel::updatePortDisplay()
{
    // Extract port from current base URL
    QString baseUrl = YukariConnect::instance().getBaseUrl();
    QRegularExpression portRegex(R"(http://[^/]+:(\d+))");
    QRegularExpressionMatch match = portRegex.match(baseUrl);

    if (match.hasMatch()) {
        QString port = match.captured(1);
        statusBar()->showMessage(tr("Connected to YukariConnect server at: %1 (Port: %2)").arg(baseUrl).arg(port));
    } else {
        statusBar()->showMessage(tr("Not connected to YukariConnect server"));
    }
}

void YukariConnectOnlinePanel::updateServerButtonState()
{
    // Use tracked server state instead of checking API
    if (m_serverRunning) {
        ui->pushButton_stop_server->setText(tr("Stop Server"));
        ui->pushButton_stop_server->setToolTip(tr("Gracefully stop YukariConnect server"));
    } else {
        ui->pushButton_stop_server->setText(tr("Start Server"));
        ui->pushButton_stop_server->setToolTip(tr("Start YukariConnect server"));
    }
}

void YukariConnectOnlinePanel::loadPlayerName()
{
    auto s = APPLICATION->settings();
    QString savedName = s->get("YukariConnectPlayerName").toString();
    if (!savedName.isEmpty()) {
        ui->lineEdit_player_name->setText(savedName);
    }
}

void YukariConnectOnlinePanel::savePlayerName()
{
    auto s = APPLICATION->settings();
    QString playerName = ui->lineEdit_player_name->text().trimmed();
    s->set("YukariConnectPlayerName", playerName);
}

void YukariConnectOnlinePanel::onLogOutput(const QString& message)
{
    // Remove trailing newlines to avoid extra blank lines in log view
    QString logMessage = message;
    while (logMessage.endsWith('\n') || logMessage.endsWith('\r')) {
        logMessage.chop(1);
    }

    if (!logMessage.isEmpty()) {
        // Append raw process output directly to log view without timestamp
        // The process logs already have their own formatting
        ui->plainTextEdit_logs->appendPlainText(logMessage);

        // Trim log to max lines if set (0 means unlimited)
        if (m_maxLogLines > 0) {
            QTextDocument* doc = ui->plainTextEdit_logs->document();
            int lineCount = doc->blockCount();
            if (lineCount > m_maxLogLines) {
                // Remove excess lines from beginning
                QTextCursor cursor(doc);
                cursor.movePosition(QTextCursor::Start);
                int linesToRemove = lineCount - m_maxLogLines;
                for (int i = 0; i < linesToRemove; i++) {
                    cursor.select(QTextCursor::BlockUnderCursor);
                    cursor.removeSelectedText();
                    cursor.deleteChar();  // Remove newline
                }
            }
        }
    }
}
