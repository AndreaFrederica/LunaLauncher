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
 *  Project URL: https://github.com/burningtnt/YukariConnect
 *
 *  This integration is implemented as a temporary solution. The launcher
 *  communicates with the standalone YukariConnect binary via its HTTP API only.
 *
 *  A future version will replace this with a custom implementation.
 */

#pragma once

#include "minecraft/online/YukariConnect.h"

#include <QMainWindow>
#include "ui/pages/BasePage.h"

namespace Ui {
class YukariConnectOnlinePanel;
}

class YukariConnectOnlinePanel : public QMainWindow, public BasePage {
    Q_OBJECT

   public:
    explicit YukariConnectOnlinePanel(QWidget* parent = nullptr);
    ~YukariConnectOnlinePanel();

    // Static methods to update settings for all instances
    static void setPollingInterval(int intervalMs);
    static void setMaxLogLines(int maxLines);

    int getPollingInterval() const { return m_pollingInterval; }
    int getMaxLogLines() const { return m_maxLogLines; }

    QString displayName() const override { return tr("YukariConnect Online"); }
    QIcon icon() const override { return QIcon::fromTheme("terracotta-online"); }
    QString id() const override { return "terracotta-online"; }
    QString helpPage() const override { return "terracotta-online"; }
    void retranslate() override;

   public slots:
    void onRefreshClicked();
    void onOpenWebUIClicked();
    void onHostClicked();
    void onJoinClicked();
    void onCloseRoomClicked();
    void onToggleServerClicked();  // Handles both Start and Stop Server
    void onRestartClicked();
    void onPanicClicked();
    void onCopyCodeClicked();
    void onFetchLogClicked();
    void onExportLogClicked();
    void onClearLogClicked();
    void onAboutClicked();
    void onStateChanged(const YukariConnectTypes::StateResponse& state);
    void onMetaChanged(const YukariConnectTypes::MetaInfo& meta);
    void onAvailabilityChanged(bool available);
    void onLogOutput(const QString& message);  // Handle process log output

   private:
    void updateStateDisplay(const YukariConnectTypes::StateResponse& state);
    void updatePlayerList(const QList<YukariConnectTypes::Profile>& profiles);
    void updatePortDisplay();
    void updateServerButtonState();  // Update Start/Stop Server button based on process state
    void updateStateHint(YukariConnectTypes::State state);  // Update hint text based on state
    void appendLog(const QString& message);
    QString getStateString(YukariConnectTypes::State state) const;
    QString getExceptionString(YukariConnectTypes::ExceptionType type) const;
    [[nodiscard]] QString getDifficultyString(YukariConnectTypes::Difficulty difficulty) const;
    void handleExceptionState(const YukariConnectTypes::StateResponse& state);
    void setUIEnabled(bool enabled);
    void startPollingState();
    void stopPollingState();
    void loadPlayerName();
    void savePlayerName();

   private:
    Ui::YukariConnectOnlinePanel* ui;
    bool m_isRefreshing = false;
    QTimer* m_statePollTimer = nullptr;
    QTimer* m_playerNameSaveTimer = nullptr;  // Timer for delayed player name saving
    qint64 m_lastProfileIndex = -1;  // Track profile_index for detecting changes
    YukariConnectTypes::State m_lastState = YukariConnectTypes::State::Waiting;
    int m_pollingInterval = 1000;  // Polling interval in milliseconds
    int m_maxLogLines = 1000;  // Maximum log lines to keep

    // Cached last state for diff comparison
    QString m_lastRoomCode;
    QString m_lastUrl;
    QList<YukariConnectTypes::Profile> m_lastProfiles;
    bool m_lastAvailable = false;  // Track last availability state to avoid duplicate logs
    bool m_serverRunning = false;  // Track if server is running (for Start/Stop button)

    static int s_globalPollingInterval;  // Global polling interval shared across all instances
    static int s_globalMaxLogLines;  // Global max log lines shared across all instances
};
