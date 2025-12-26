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

#pragma once

#include "minecraft/online/Terracotta.h"

#include <QMainWindow>
#include "ui/pages/BasePage.h"

namespace Ui {
class TerracottaOnlinePanel;
}

class TerracottaOnlinePanel : public QMainWindow, public BasePage {
    Q_OBJECT

   public:
    explicit TerracottaOnlinePanel(QWidget* parent = nullptr);
    ~TerracottaOnlinePanel();

    // Static methods to update settings for all instances
    static void setPollingInterval(int intervalMs);
    static void setMaxLogLines(int maxLines);

    int getPollingInterval() const { return m_pollingInterval; }
    int getMaxLogLines() const { return m_maxLogLines; }

    QString displayName() const override { return tr("Terracotta Online"); }
    QIcon icon() const override { return QIcon::fromTheme("terracotta-online"); }
    QString id() const override { return "terracotta-online"; }
    QString helpPage() const override { return "terracotta-online"; }
    void retranslate() override;

   private slots:
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
    void onClearLogClicked();
    void onAboutClicked();
    void onStateChanged(const TerracottaTypes::StateResponse& state);
    void onAvailabilityChanged(bool available);

   private:
    void updateStateDisplay(const TerracottaTypes::StateResponse& state);
    void updatePlayerList(const QList<TerracottaTypes::Profile>& profiles);
    void updatePortDisplay();
    void updateServerButtonState();  // Update Start/Stop Server button based on process state
    void updateStateHint(TerracottaTypes::State state);  // Update hint text based on state
    void appendLog(const QString& message);
    QString getStateString(TerracottaTypes::State state) const;
    QString getExceptionString(TerracottaTypes::ExceptionType type) const;
    [[nodiscard]] QString getDifficultyString(TerracottaTypes::Difficulty difficulty) const;
    void handleExceptionState(const TerracottaTypes::StateResponse& state);
    void setUIEnabled(bool enabled);
    void startPollingState();
    void stopPollingState();
    void loadPlayerName();
    void savePlayerName();

   private:
    Ui::TerracottaOnlinePanel* ui;
    bool m_isRefreshing = false;
    QTimer* m_statePollTimer = nullptr;
    QTimer* m_playerNameSaveTimer = nullptr;  // Timer for delayed player name saving
    qint64 m_lastProfileIndex = -1;  // Track profile_index for detecting changes
    TerracottaTypes::State m_lastState = TerracottaTypes::State::Waiting;
    int m_pollingInterval = 1000;  // Polling interval in milliseconds
    int m_maxLogLines = 1000;  // Maximum log lines to keep

    // Cached last state for diff comparison
    QString m_lastRoomCode;
    QString m_lastUrl;
    QList<TerracottaTypes::Profile> m_lastProfiles;
    bool m_lastAvailable = false;  // Track last availability state to avoid duplicate logs
    bool m_serverRunning = false;  // Track if server is running (for Start/Stop button)

    static int s_globalPollingInterval;  // Global polling interval shared across all instances
    static int s_globalMaxLogLines;  // Global max log lines shared across all instances
};
