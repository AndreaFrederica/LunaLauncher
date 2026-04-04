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
 *  "Your程序通过本作品提供的进程间通信接口（如 HTTP API）与未经修改的
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

#include "TerracottaOnlinePanel.h"
#include "ui_TerracottaOnlinePanel.h"

#include <QClipboard>
#include <QRegularExpression>
#include <QTimer>
#include <QDateTime>
#include <QThread>
#include <QGuiApplication>
#include <QMessageBox>

#include "Application.h"
#include "DesktopServices.h"
#include "minecraft/online/Terracotta.h"
#include "settings/SettingsObject.h"

// Initialize static members
int TerracottaOnlinePanel::s_globalPollingInterval = 1000;
int TerracottaOnlinePanel::s_globalMaxLogLines = 1000;

TerracottaOnlinePanel::TerracottaOnlinePanel(QWidget* parent) : QMainWindow(parent), ui(new Ui::TerracottaOnlinePanel)
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

    // Create state polling timer
    m_statePollTimer = new QTimer(this);
    m_statePollTimer->setInterval(m_pollingInterval);
    connect(m_statePollTimer, &QTimer::timeout, this, [this]() {
        auto state = Terracotta::instance().fetchState();
        if (state) {
            // Track if this is a state transition
            bool stateChanged = (m_lastState != state->state);
            m_lastState = state->state;

            // Handle Exception state with friendly message
            if (state->state == TerracottaTypes::State::Exception && stateChanged) {
                handleExceptionState(*state);
            }

            // Always update state display and port
            updateStateDisplay(*state);
            updatePortDisplay();

            // For HostOk/GuestOk states, continue polling to monitor player changes
            // Note: We keep polling as long as server is running, regardless of state
            if (state->state == TerracottaTypes::State::HostOk ||
                       state->state == TerracottaTypes::State::GuestOk) {
                // In HostOk or GuestOk state, monitor player list changes
                if (stateChanged) {
                    // Just reached this state, log it
                    appendLog(tr("Now monitoring for player changes..."));
                }
                // Check if player list changed (detected by profile_index change)
                if (state->profile_index != m_lastProfileIndex) {
                    if (!stateChanged) {  // Only log if not the initial state change
                        appendLog(tr("Player list updated (index: %1)").arg(state->profile_index));
                    }
                    m_lastProfileIndex = state->profile_index;
                }
            }
        } else {
            // API not responding - server likely stopped
            // Stop polling and update server state
            stopPollingState();
            m_serverRunning = false;
            updateServerButtonState();
            m_lastProfileIndex = -1;
        }
    });

    // Connect buttons
    connect(ui->pushButton_refresh, &QPushButton::clicked, this, &TerracottaOnlinePanel::onRefreshClicked);
    connect(ui->pushButton_open_webui, &QPushButton::clicked, this, &TerracottaOnlinePanel::onOpenWebUIClicked);
    connect(ui->pushButton_host, &QPushButton::clicked, this, &TerracottaOnlinePanel::onHostClicked);
    connect(ui->pushButton_join, &QPushButton::clicked, this, &TerracottaOnlinePanel::onJoinClicked);
    connect(ui->pushButton_close_room, &QPushButton::clicked, this, &TerracottaOnlinePanel::onCloseRoomClicked);
    connect(ui->pushButton_stop_server, &QPushButton::clicked, this, &TerracottaOnlinePanel::onToggleServerClicked);
    connect(ui->pushButton_restart, &QPushButton::clicked, this, &TerracottaOnlinePanel::onRestartClicked);
    connect(ui->pushButton_panic, &QPushButton::clicked, this, &TerracottaOnlinePanel::onPanicClicked);
    connect(ui->pushButton_copy_code, &QPushButton::clicked, this, &TerracottaOnlinePanel::onCopyCodeClicked);
    connect(ui->pushButton_fetch_log, &QPushButton::clicked, this, &TerracottaOnlinePanel::onFetchLogClicked);
    connect(ui->pushButton_clear_log, &QPushButton::clicked, this, &TerracottaOnlinePanel::onClearLogClicked);
    connect(ui->pushButton_about, &QPushButton::clicked, this, &TerracottaOnlinePanel::onAboutClicked);

    // Connect player name text changed to restart save timer
    connect(ui->lineEdit_player_name, &QLineEdit::textChanged, this, [this]() {
        // Restart the timer whenever user types
        m_playerNameSaveTimer->start();
    });

    // Connect to Terracotta signals
    connect(&Terracotta::instance(), &Terracotta::stateChanged, this, &TerracottaOnlinePanel::onStateChanged);
    connect(&Terracotta::instance(), &Terracotta::availabilityChanged, this, &TerracottaOnlinePanel::onAvailabilityChanged);

    // Show current connection port and server button state
    updatePortDisplay();
    updateServerButtonState();

    updateServerButtonState();
}

TerracottaOnlinePanel::~TerracottaOnlinePanel()
{
    delete ui;
}

void TerracottaOnlinePanel::setPollingInterval(int intervalMs)
{
    s_globalPollingInterval = intervalMs;
}

void TerracottaOnlinePanel::setMaxLogLines(int maxLines)
{
    s_globalMaxLogLines = maxLines;
}

void TerracottaOnlinePanel::retranslate()
{
    ui->retranslateUi(this);
}

void TerracottaOnlinePanel::onRefreshClicked()
{
    if (m_isRefreshing) {
        return;
    }
    m_isRefreshing = true;
    ui->pushButton_refresh->setEnabled(false);

    if (!Terracotta::instance().isProcessRunning()) {
        appendLog(tr("Terracotta is not running."));
        ui->label_state_value->setText(tr("Not Running"));
        setUIEnabled(false);
        ui->pushButton_refresh->setEnabled(true);
        m_isRefreshing = false;
        return;
    }

    appendLog(tr("Refreshing state..."));

    auto state = Terracotta::instance().fetchState();
    if (state) {
        updateStateDisplay(*state);
        updatePortDisplay();  // Update port display after successful refresh
    } else {
        appendLog(tr("Failed to fetch state. Is Terracotta server running?"));
        ui->label_state_value->setText(tr("Error"));
        // Disable most UI, but keep Start/Stop Server button enabled
        ui->groupBox_actions->setEnabled(false);
        ui->pushButton_stop_server->setEnabled(true);
        // Show "Start Server" since fetch failed
        ui->pushButton_stop_server->setText(tr("Start Server"));
        ui->pushButton_stop_server->setToolTip(tr("Start the Terracotta server"));
    }

    ui->pushButton_refresh->setEnabled(true);
    m_isRefreshing = false;
}

void TerracottaOnlinePanel::onOpenWebUIClicked()
{
    QString url = Terracotta::instance().getBaseUrl();
    if (url.isEmpty()) {
        appendLog(tr("Terracotta server URL not available. Please start the server first."));
        return;
    }
    appendLog(tr("Opening Terracotta Web UI in browser: %1").arg(url));
    DesktopServices::openUrl(QUrl(url));
}

void TerracottaOnlinePanel::onHostClicked()
{
    QString playerName = ui->lineEdit_player_name->text().trimmed();
    if (playerName.isEmpty()) {
        QMessageBox::warning(this, tr("Missing Player Name"), tr("Please enter a player name."));
        return;
    }

    // Save player name when hosting
    savePlayerName();

    appendLog(tr("Starting scanning mode for player: %1").arg(playerName));

    if (Terracotta::instance().startScanning(playerName)) {
        appendLog(tr("Successfully entered scanning mode. Waiting for room creation..."));
        // Immediately refresh state to update UI
        onRefreshClicked();
    } else {
        appendLog(tr("Failed to enter scanning mode."));
    }
}

void TerracottaOnlinePanel::onJoinClicked()
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

    if (Terracotta::instance().joinRoom(roomCode, playerName)) {
        appendLog(tr("Successfully joined room. Waiting for connection..."));
        // Immediately refresh state to update UI
        onRefreshClicked();
    } else {
        appendLog(tr("Failed to join room. Check the room code and try again."));
        QMessageBox::warning(this, tr("Join Failed"), tr("Failed to join the room. The room code may be invalid or the room is full."));
    }
}

void TerracottaOnlinePanel::onCloseRoomClicked()
{
    appendLog(tr("Closing room and returning to idle..."));

    if (Terracotta::instance().cancelState()) {
        appendLog(tr("Room closed successfully. Back to idle state."));
        // Immediately refresh state to update UI
        onRefreshClicked();
    } else {
        appendLog(tr("Failed to close room."));
    }
}

void TerracottaOnlinePanel::onToggleServerClicked()
{
    // Check button text to determine action (more reliable than process state)
    bool isStopAction = ui->pushButton_stop_server->text() == tr("Stop Server");

    if (isStopAction) {
        // Server is running - stop it
        auto reply = QMessageBox::question(this, tr("Stop Server"),
                                            tr("Are you sure you want to stop the Terracotta server?\n\nThis will gracefully shut down the server and close all connections."),
                                            QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            appendLog(tr("Stopping Terracotta server..."));

            // Stop polling before shutdown
            stopPollingState();

            // Use the new shutdown method that handles everything
            bool stopped = Terracotta::instance().shutdownAndWait();

            if (stopped) {
                appendLog(tr("Terracotta server stopped gracefully."));
            } else {
                appendLog(tr("Terracotta server shutdown timeout, assuming stopped."));
            }

            // Update state
            m_serverRunning = false;
            ui->label_state_value->setText(tr("Stopped"));
            updateServerButtonState();
            ui->groupBox_actions->setEnabled(true);
        }
    } else {
        // Server is not running or not available - start it
        appendLog(tr("Starting Terracotta server..."));

        // First force kill any existing process to ensure clean state
        if (Terracotta::instance().isProcessRunning()) {
            appendLog(tr("Cleaning up existing process..."));
            Terracotta::instance().forceKillProcess();
            QThread::msleep(500);  // Brief pause to let cleanup complete
        }

        if (Terracotta::instance().startProcess()) {
            appendLog(tr("Terracotta server started successfully."));
            // Update state
            m_serverRunning = true;
            ui->label_state_value->setText(tr("Connected"));
            updatePortDisplay();
            updateServerButtonState();
            // Start polling for state changes
            startPollingState();
            // Refresh state after a short delay to let server initialize
            QTimer::singleShot(500, this, [this]() {
                onRefreshClicked();
            });
        } else {
            appendLog(tr("Failed to start Terracotta server. Please check if it is installed."));
            m_serverRunning = false;
            ui->label_state_value->setText(tr("Failed"));
            updateServerButtonState();
        }
    }
}

void TerracottaOnlinePanel::onRestartClicked()
{
    auto reply = QMessageBox::question(this, tr("Restart Server"),
                                        tr("Are you sure you want to restart the Terracotta server?\n\nThis will disconnect all active connections."),
                                        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        appendLog(tr("Restarting Terracotta server..."));

        // Stop polling before restart
        stopPollingState();

        // Cancel any active state first
        Terracotta::instance().cancelState();

        // Stop the current process
        Terracotta::instance().stopProcess();

        // Wait a moment for it to fully stop, then restart
        QTimer::singleShot(500, this, [this]() {
            if (Terracotta::instance().startProcess()) {
                appendLog(tr("Terracotta server restarted successfully."));
                m_serverRunning = true;
                ui->label_state_value->setText(tr("Connected"));
                updatePortDisplay();
                updateServerButtonState();
                // Start polling after restart
                startPollingState();
                // Refresh state after restart
                QTimer::singleShot(500, this, [this]() {
                    onRefreshClicked();
                });
            } else {
                appendLog(tr("Failed to restart Terracotta server."));
                m_serverRunning = false;
                ui->label_state_value->setText(tr("Failed"));
                updateServerButtonState();
            }
        });
    }
}

void TerracottaOnlinePanel::onPanicClicked()
{
    auto reply = QMessageBox::question(this, tr("Emergency Stop"),
                                        tr("Are you sure you want to emergency stop Terracotta?\n\nThis will forcefully stop all P2P connections."),
                                        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        appendLog(tr("Emergency stop initiated..."));

        if (Terracotta::instance().panic(false)) {
            appendLog(tr("Emergency stop successful."));
            // Update button to show Start Server
            ui->pushButton_stop_server->setText(tr("Start Server"));
            ui->pushButton_stop_server->setToolTip(tr("Start the Terracotta server"));
            // Refresh state after a short delay
            QTimer::singleShot(500, this, [this]() {
                onRefreshClicked();
            });
        } else {
            appendLog(tr("Emergency stop failed."));
        }
    }
}

void TerracottaOnlinePanel::onCopyCodeClicked()
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

void TerracottaOnlinePanel::onFetchLogClicked()
{
    appendLog(tr("Fetching logs from Terracotta server..."));

    QByteArray logData = Terracotta::instance().fetchLog(true);
    if (!logData.isEmpty()) {
        ui->plainTextEdit_logs->setPlainText(QString::fromUtf8(logData));
        appendLog(tr("Logs fetched successfully."));
    } else {
        appendLog(tr("Failed to fetch logs."));
    }
}

void TerracottaOnlinePanel::onClearLogClicked()
{
    ui->plainTextEdit_logs->clear();
}

void TerracottaOnlinePanel::onAboutClicked()
{
    QString aboutText = tr(
        "<h3>Terracotta P2P Multiplayer</h3>"
        "<p><b>P2P Multiplayer Integration for Luna Launcher</b></p>"
        "<p>This feature integrates with Terracotta, a P2P multiplayer solution for Minecraft.</p>"
        "<h4>Terracotta Project</h4>"
        "<p><b>Developer:</b> burningtnt<br>"
        "<b>Project URL:</b> <a href=\"https://github.com/burningtnt/Terracotta\">https://github.com/burningtnt/Terracotta</a></p>"
        "<h4>License</h4>"
        "<p>Terracotta is licensed under <b>AGPL-3.0</b> with the following exception:</p>"
        "<p><i>\"Your program通过本作品提供的进程间通信接口（如 HTTP API）与未经修改的"
        "本作品应用程序进行交互，不构成衍生作品。\"</i></p>"
        "<p><i>Translation: \"Your program's interaction with an unmodified copy of this work"
        "through the inter-process communication interfaces provided by this work (such as HTTP APIs)"
        "does not constitute a derivative work.\"</i></p>"
        "<h4>Integration Notice</h4>"
        "<p>This integration communicates with the standalone Terracotta binary via its HTTP API only,"
        "which is explicitly permitted under Terracotta's license exception.</p>"
        "<p><b>Note:</b> This is a temporary solution. A future version will replace this with a custom implementation.</p>"
    );

    QMessageBox::about(this, tr("About Terracotta"), aboutText);
}

void TerracottaOnlinePanel::onStateChanged(const TerracottaTypes::StateResponse& state)
{
    updateStateDisplay(state);
}

void TerracottaOnlinePanel::onAvailabilityChanged(bool available)
{
    updatePortDisplay();  // Update port display when availability changes

    // Only log when availability actually changes
    if (available != m_lastAvailable) {
        m_lastAvailable = available;
        if (available) {
            appendLog(tr("Terracotta server is now available."));
        } else {
            appendLog(tr("Terracotta server is not available."));
        }
    }

    // Update server running state based on availability
    m_serverRunning = available;

    if (available) {
        setUIEnabled(true);
        updateServerButtonState();  // Update button based on actual process state
    } else {
        // Disable most UI, but keep the Start/Stop Server button enabled
        // so users can restart the server
        ui->pushButton_host->setEnabled(false);
        ui->pushButton_join->setEnabled(false);
        ui->pushButton_close_room->setEnabled(false);
        ui->pushButton_stop_server->setEnabled(true);  // Keep Start/Stop Server enabled

        // When server is not available, show "Start Server" button
        updateServerButtonState();
    }
}

void TerracottaOnlinePanel::updateStateDisplay(const TerracottaTypes::StateResponse& state)
{
    // Update state label with color
    ui->label_state_value->setText(getStateString(state.state));

    // Set color based on state (same scheme as main window status bar)
    QString color;
    switch (state.state) {
        case TerracottaTypes::State::Waiting:
        case TerracottaTypes::State::HostScanning:
            color = "gray";
            break;
        case TerracottaTypes::State::HostStarting:
            color = "orange";
            break;
        case TerracottaTypes::State::HostOk:
            color = "green";
            break;
        case TerracottaTypes::State::GuestConnecting:
        case TerracottaTypes::State::GuestStarting:
            color = "orange";
            break;
        case TerracottaTypes::State::GuestOk:
            color = "green";
            break;
        case TerracottaTypes::State::Exception:
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
        case TerracottaTypes::State::Waiting:
        case TerracottaTypes::State::HostScanning:
            // In idle or scanning mode - can start hosting or joining
            ui->pushButton_host->setEnabled(true);
            ui->pushButton_join->setEnabled(true);
            ui->pushButton_close_room->setEnabled(false);
            break;
        case TerracottaTypes::State::HostOk:
            // Hosting a room - can close room
            ui->pushButton_host->setEnabled(false);
            ui->pushButton_join->setEnabled(false);
            ui->pushButton_close_room->setEnabled(true);
            break;
        case TerracottaTypes::State::GuestOk:
            // Joined as guest - can leave room
            ui->pushButton_host->setEnabled(false);
            ui->pushButton_join->setEnabled(false);
            ui->pushButton_close_room->setEnabled(true);
            break;
        case TerracottaTypes::State::Exception:
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

void TerracottaOnlinePanel::updatePlayerList(const QList<TerracottaTypes::Profile>& profiles)
{
    ui->tableWidget_players->setRowCount(profiles.size());

    for (int i = 0; i < profiles.size(); ++i) {
        const auto& profile = profiles[i];

        ui->tableWidget_players->setItem(i, 0, new QTableWidgetItem(profile.name));
        ui->tableWidget_players->setItem(i, 1, new QTableWidgetItem(profile.machine_id));
        ui->tableWidget_players->setItem(i, 2, new QTableWidgetItem(profile.vendor));

        QString kindStr;
        switch (profile.kind) {
            case TerracottaTypes::ProfileKind::HOST:
                kindStr = tr("Host");
                break;
            case TerracottaTypes::ProfileKind::GUEST:
                kindStr = tr("Guest");
                break;
            default:
                kindStr = tr("Local");
                break;
        }
        ui->tableWidget_players->setItem(i, 3, new QTableWidgetItem(kindStr));
    }
}

void TerracottaOnlinePanel::appendLog(const QString& message)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    ui->plainTextEdit_logs->appendPlainText(QString("[%1] %2").arg(timestamp, message));

    // Trim log to max lines if set (0 means unlimited)
    if (m_maxLogLines > 0) {
        QTextDocument* doc = ui->plainTextEdit_logs->document();
        int lineCount = doc->blockCount();
        if (lineCount > m_maxLogLines) {
            // Remove excess lines from the beginning
            QTextCursor cursor(doc);
            cursor.movePosition(QTextCursor::Start);
            while (doc->blockCount() > m_maxLogLines) {
                cursor.select(QTextCursor::LineUnderCursor);
                cursor.removeSelectedText();
                cursor.deleteChar();  // Remove the newline
            }
        }
    }
}

QString TerracottaOnlinePanel::getStateString(TerracottaTypes::State state) const
{
    switch (state) {
        case TerracottaTypes::State::Waiting:
            return tr("Waiting");
        case TerracottaTypes::State::HostScanning:
            return tr("Host Scanning");
        case TerracottaTypes::State::HostStarting:
            return tr("Host Starting");
        case TerracottaTypes::State::HostOk:
            return tr("Host Ready");
        case TerracottaTypes::State::GuestConnecting:
            return tr("Guest Connecting");
        case TerracottaTypes::State::GuestStarting:
            return tr("Guest Starting");
        case TerracottaTypes::State::GuestOk:
            return tr("Guest Connected");
        case TerracottaTypes::State::Exception:
            return tr("Exception");
        default:
            return tr("Unknown");
    }
}

void TerracottaOnlinePanel::updateStateHint(TerracottaTypes::State state)
{
    QString hint;
    switch (state) {
        case TerracottaTypes::State::Waiting:
            hint = tr("Enter your player name and click Create Room to host, or enter a room code to join.");
            break;
        case TerracottaTypes::State::HostScanning:
            hint = tr("Open your Minecraft world, press Esc, and click 'Open to LAN' to start the LAN server.");
            break;
        case TerracottaTypes::State::HostStarting:
            hint = tr("Starting your room... The server will be ready shortly.");
            break;
        case TerracottaTypes::State::HostOk:
            hint = tr("Your room is ready! Share the room code with friends so they can join.");
            break;
        case TerracottaTypes::State::GuestConnecting:
            hint = tr("Connecting to the host room... This may take a moment.");
            break;
        case TerracottaTypes::State::GuestStarting:
            hint = tr("Setting up connection to the Minecraft server...");
            break;
        case TerracottaTypes::State::GuestOk:
            hint = tr("Connected! You can now join the game using the connection URL above.");
            break;
        case TerracottaTypes::State::Exception:
            hint = tr("An error occurred. Check the logs for details and try creating or joining a room again.");
            break;
        default:
            hint = tr("Please wait...");
            break;
    }
    ui->label_state_hint->setText(hint);
}

void TerracottaOnlinePanel::setUIEnabled(bool enabled)
{
    ui->groupBox_actions->setEnabled(enabled);
    ui->pushButton_refresh->setEnabled(enabled);
}

void TerracottaOnlinePanel::startPollingState()
{
    if (m_statePollTimer && !m_statePollTimer->isActive()) {
        m_statePollTimer->start();
        appendLog(tr("Started polling for state changes..."));
    }
}

QString TerracottaOnlinePanel::getExceptionString(TerracottaTypes::ExceptionType type) const
{
    switch (type) {
        case TerracottaTypes::ExceptionType::PingHostFail:
            return tr("Cannot connect to host");
        case TerracottaTypes::ExceptionType::PingHostRst:
            return tr("Connection to host lost");
        case TerracottaTypes::ExceptionType::GuestEasytierCrash:
            return tr("EasyTier crashed (guest)");
        case TerracottaTypes::ExceptionType::HostEasytierCrash:
            return tr("EasyTier crashed (host)");
        case TerracottaTypes::ExceptionType::PingServerRst:
            return tr("Minecraft server closed");
        case TerracottaTypes::ExceptionType::ScaffoldingInvalidResponse:
            return tr("Invalid server response");
        default:
            return tr("Unknown error");
    }
}

QString TerracottaOnlinePanel::getDifficultyString(TerracottaTypes::Difficulty difficulty) const
{
    switch (difficulty) {
        case TerracottaTypes::Difficulty::EASIEST:
            return tr("Easiest");
        case TerracottaTypes::Difficulty::SIMPLE:
            return tr("Simple");
        case TerracottaTypes::Difficulty::MEDIUM:
            return tr("Medium");
        case TerracottaTypes::Difficulty::TOUGH:
            return tr("Tough");
        case TerracottaTypes::Difficulty::UNKNOWN:
        default:
            return tr("Unknown");
    }
}

void TerracottaOnlinePanel::handleExceptionState(const TerracottaTypes::StateResponse& state)
{
    QString exceptionMsg = getExceptionString(state.exception_type);

    // Provide friendly messages for common exception types
    switch (state.exception_type) {
        case TerracottaTypes::ExceptionType::PingServerRst:
            // Minecraft game exited - this is normal behavior
            appendLog(tr("Room closed: Minecraft game has exited."));
            appendLog(tr("You can start a new room whenever you're ready to play again."));
            break;
        case TerracottaTypes::ExceptionType::PingHostRst:
            appendLog(tr("Connection lost: The host closed the room or disconnected."));
            break;
        case TerracottaTypes::ExceptionType::PingHostFail:
            appendLog(tr("Connection failed: Unable to reach the host room."));
            break;
        case TerracottaTypes::ExceptionType::GuestEasytierCrash:
        case TerracottaTypes::ExceptionType::HostEasytierCrash:
            appendLog(tr("Error: EasyTier process crashed. Reason: %1").arg(exceptionMsg));
            appendLog(tr("Try restarting the Terracotta server."));
            break;
        case TerracottaTypes::ExceptionType::ScaffoldingInvalidResponse:
            appendLog(tr("Protocol error: Received invalid response from server."));
            break;
        default:
            appendLog(tr("Exception occurred: %1").arg(exceptionMsg));
            break;
    }

    // Automatically reset to Waiting state after exception
    appendLog(tr("Resetting to idle state..."));
    if (Terracotta::instance().cancelState()) {
        appendLog(tr("Successfully reset to idle state."));
        // Refresh state after a short delay to confirm the reset
        QTimer::singleShot(200, this, [this]() {
            onRefreshClicked();
        });
    } else {
        appendLog(tr("Failed to reset to idle state. You may need to restart the server."));
    }
}

void TerracottaOnlinePanel::stopPollingState()
{
    if (m_statePollTimer && m_statePollTimer->isActive()) {
        m_statePollTimer->stop();
        appendLog(tr("Stopped polling state."));
    }
}

void TerracottaOnlinePanel::updatePortDisplay()
{
    // Extract port from current base URL
    QString baseUrl = Terracotta::instance().getBaseUrl();
    QRegularExpression portRegex(R"(http://[^/]+:(\d+))");
    QRegularExpressionMatch match = portRegex.match(baseUrl);

    if (match.hasMatch()) {
        QString port = match.captured(1);
        statusBar()->showMessage(tr("Connected to Terracotta server at: %1 (Port: %2)").arg(baseUrl).arg(port));
    } else {
        statusBar()->showMessage(tr("Not connected to Terracotta server"));
    }
}

void TerracottaOnlinePanel::updateServerButtonState()
{
    // Use tracked server state instead of checking API
    if (m_serverRunning) {
        ui->pushButton_stop_server->setText(tr("Stop Server"));
        ui->pushButton_stop_server->setToolTip(tr("Gracefully stop the Terracotta server"));
    } else {
        ui->pushButton_stop_server->setText(tr("Start Server"));
        ui->pushButton_stop_server->setToolTip(tr("Start the Terracotta server"));
    }
}

void TerracottaOnlinePanel::loadPlayerName()
{
    auto s = APPLICATION->settings();
    QString savedName = s->get("TerracottaPlayerName").toString();
    if (!savedName.isEmpty()) {
        ui->lineEdit_player_name->setText(savedName);
    }
}

void TerracottaOnlinePanel::savePlayerName()
{
    auto s = APPLICATION->settings();
    QString playerName = ui->lineEdit_player_name->text().trimmed();
    s->set("TerracottaPlayerName", playerName);
}
