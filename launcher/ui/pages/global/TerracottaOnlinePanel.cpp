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

#include "TerracottaOnlinePanel.h"
#include "ui_TerracottaOnlinePanel.h"

#include <QClipboard>
#include <QRegularExpression>
#include <QTimer>
#include <QDateTime>
#include <QGuiApplication>
#include <QMessageBox>
#include <QThread>

#include "minecraft/online/Terracotta.h"

TerracottaOnlinePanel::TerracottaOnlinePanel(QWidget* parent) : QMainWindow(parent), ui(new Ui::TerracottaOnlinePanel)
{
    ui->setupUi(this);

    // Create state polling timer
    m_statePollTimer = new QTimer(this);
    m_statePollTimer->setInterval(1000);  // Poll every 1 second
    connect(m_statePollTimer, &QTimer::timeout, this, [this]() {
        auto state = Terracotta::instance().fetchState();
        if (state) {
            updateStateDisplay(*state);
            updatePortDisplay();

            // Stop polling if we reached host-ok or guest-ok state
            if (state->state == TerracottaTypes::State::HostOk ||
                state->state == TerracottaTypes::State::GuestOk ||
                state->state == TerracottaTypes::State::Exception) {
                stopPollingState();
            }
        }
    });

    // Connect buttons
    connect(ui->pushButton_refresh, &QPushButton::clicked, this, &TerracottaOnlinePanel::onRefreshClicked);
    connect(ui->pushButton_host, &QPushButton::clicked, this, &TerracottaOnlinePanel::onHostClicked);
    connect(ui->pushButton_join, &QPushButton::clicked, this, &TerracottaOnlinePanel::onJoinClicked);
    connect(ui->pushButton_direct, &QPushButton::clicked, this, &TerracottaOnlinePanel::onDirectConnectClicked);
    connect(ui->pushButton_cancel, &QPushButton::clicked, this, &TerracottaOnlinePanel::onCancelClicked);
    connect(ui->pushButton_restart, &QPushButton::clicked, this, &TerracottaOnlinePanel::onRestartClicked);
    connect(ui->pushButton_panic, &QPushButton::clicked, this, &TerracottaOnlinePanel::onPanicClicked);
    connect(ui->pushButton_copy_code, &QPushButton::clicked, this, &TerracottaOnlinePanel::onCopyCodeClicked);
    connect(ui->pushButton_fetch_log, &QPushButton::clicked, this, &TerracottaOnlinePanel::onFetchLogClicked);
    connect(ui->pushButton_clear_log, &QPushButton::clicked, this, &TerracottaOnlinePanel::onClearLogClicked);

    // Connect to Terracotta signals
    connect(&Terracotta::instance(), &Terracotta::stateChanged, this, &TerracottaOnlinePanel::onStateChanged);
    connect(&Terracotta::instance(), &Terracotta::availabilityChanged, this, &TerracottaOnlinePanel::onAvailabilityChanged);

    // Show current connection port
    updatePortDisplay();

    // Auto-start Terracotta process if not running (delayed to avoid blocking UI)
    QTimer::singleShot(100, this, [this]() {
        if (!Terracotta::instance().isProcessRunning()) {
            ui->label_state_value->setText(tr("Starting..."));
            appendLog(tr("Starting Terracotta process..."));

            if (Terracotta::instance().startProcess()) {
                appendLog(tr("Terracotta process started successfully."));
                ui->label_state_value->setText(tr("Connected"));
                updatePortDisplay();
            } else {
                appendLog(tr("Failed to start Terracotta process. Please check if it is installed."));
                ui->label_state_value->setText(tr("Failed"));
            }
        } else {
            ui->label_state_value->setText(tr("Connected"));
        }
        // Initial refresh after process starts
        onRefreshClicked();
    });
}

TerracottaOnlinePanel::~TerracottaOnlinePanel()
{
    delete ui;
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

    // Start process if not running
    if (!Terracotta::instance().isProcessRunning()) {
        appendLog(tr("Terracotta not running, starting..."));
        if (!Terracotta::instance().startProcess()) {
            appendLog(tr("Failed to start Terracotta process."));
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

    auto state = Terracotta::instance().fetchState();
    if (state) {
        updateStateDisplay(*state);
        updatePortDisplay();  // Update port display after successful refresh
    } else {
        appendLog(tr("Failed to fetch state. Is Terracotta server running?"));
        ui->label_state_value->setText(tr("Error"));
        setUIEnabled(false);
    }

    ui->pushButton_refresh->setEnabled(true);
    m_isRefreshing = false;
}

void TerracottaOnlinePanel::onHostClicked()
{
    QString playerName = ui->lineEdit_player_name->text().trimmed();
    if (playerName.isEmpty()) {
        QMessageBox::warning(this, tr("Missing Player Name"), tr("Please enter a player name."));
        return;
    }

    appendLog(tr("Starting scanning mode for player: %1").arg(playerName));

    if (Terracotta::instance().startScanning(playerName)) {
        appendLog(tr("Successfully entered scanning mode. Waiting for room creation..."));
        // Start polling to wait for room code
        startPollingState();
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

    appendLog(tr("Joining room: %1 as player: %2").arg(roomCode, playerName));

    if (Terracotta::instance().joinRoom(roomCode, playerName)) {
        appendLog(tr("Successfully joined room. Waiting for connection..."));
        // Start polling to wait for connection
        startPollingState();
    } else {
        appendLog(tr("Failed to join room. Check the room code and try again."));
        QMessageBox::warning(this, tr("Join Failed"), tr("Failed to join the room. The room code may be invalid or the room is full."));
    }
}

void TerracottaOnlinePanel::onDirectConnectClicked()
{
    uint16_t port = static_cast<uint16_t>(ui->spinBox_port->value());
    QString playerName = ui->lineEdit_player_name->text().trimmed();
    if (playerName.isEmpty()) {
        QMessageBox::warning(this, tr("Missing Player Name"), tr("Please enter a player name."));
        return;
    }

    appendLog(tr("Starting direct connection to port: %1 as player: %2").arg(port).arg(playerName));

    if (Terracotta::instance().startWithPort(port, playerName)) {
        appendLog(tr("Direct connection started successfully. Waiting for connection..."));
        // Start polling to wait for connection
        startPollingState();
    } else {
        appendLog(tr("Failed to start direct connection."));
    }
}

void TerracottaOnlinePanel::onCancelClicked()
{
    appendLog(tr("Canceling current state and returning to idle..."));

    // Stop any ongoing polling
    stopPollingState();

    if (Terracotta::instance().cancelState()) {
        appendLog(tr("Successfully returned to idle state."));
        onRefreshClicked();
    } else {
        appendLog(tr("Failed to cancel state."));
    }
}

void TerracottaOnlinePanel::onRestartClicked()
{
    auto reply = QMessageBox::question(this, tr("Restart Server"),
                                        tr("Are you sure you want to restart the Terracotta server?\n\nThis will disconnect all active connections."),
                                        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        appendLog(tr("Restarting Terracotta server..."));

        // Stop any ongoing polling
        stopPollingState();

        // Cancel any active state first
        Terracotta::instance().cancelState();

        // Stop the current process
        Terracotta::instance().stopProcess();

        // Wait a moment for it to fully stop, then restart
        QTimer::singleShot(500, this, [this]() {
            if (Terracotta::instance().startProcess()) {
                appendLog(tr("Terracotta server restarted successfully."));
                ui->label_state_value->setText(tr("Connected"));
                updatePortDisplay();
                // Refresh state after restart
                QTimer::singleShot(500, this, [this]() {
                    onRefreshClicked();
                });
            } else {
                appendLog(tr("Failed to restart Terracotta server."));
                ui->label_state_value->setText(tr("Failed"));
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
            onRefreshClicked();
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

void TerracottaOnlinePanel::onStateChanged(const TerracottaTypes::StateResponse& state)
{
    updateStateDisplay(state);
}

void TerracottaOnlinePanel::onAvailabilityChanged(bool available)
{
    updatePortDisplay();  // Update port display when availability changes
    if (available) {
        appendLog(tr("Terracotta server is now available."));
        setUIEnabled(true);
    } else {
        appendLog(tr("Terracotta server is not available."));
        setUIEnabled(false);
    }
}

void TerracottaOnlinePanel::updateStateDisplay(const TerracottaTypes::StateResponse& state)
{
    // Update state label
    ui->label_state_value->setText(getStateString(state.state));

    // Update room info
    if (!state.room.isEmpty()) {
        ui->lineEdit_room_code->setText(state.room);
    } else {
        ui->lineEdit_room_code->setText(tr("-"));
    }

    if (!state.url.isEmpty()) {
        ui->lineEdit_url->setText(state.url);
    } else {
        ui->lineEdit_url->setText(tr("-"));
    }

    // Update player list
    updatePlayerList(state.profiles);

    // Update UI based on state
    switch (state.state) {
        case TerracottaTypes::State::Waiting:
        case TerracottaTypes::State::HostScanning:
            ui->pushButton_host->setEnabled(true);
            ui->pushButton_join->setEnabled(true);
            ui->pushButton_direct->setEnabled(true);
            ui->pushButton_cancel->setEnabled(false);
            break;
        default:
            ui->pushButton_host->setEnabled(false);
            ui->pushButton_join->setEnabled(false);
            ui->pushButton_direct->setEnabled(false);
            ui->pushButton_cancel->setEnabled(true);
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
