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

    QString displayName() const override { return tr("Terracotta Online"); }
    QIcon icon() const override { return QIcon::fromTheme("terracotta-online"); }
    QString id() const override { return "terracotta-online"; }
    QString helpPage() const override { return "terracotta-online"; }
    void retranslate() override;

   private slots:
    void onRefreshClicked();
    void onHostClicked();
    void onJoinClicked();
    void onDirectConnectClicked();
    void onCancelClicked();
    void onRestartClicked();
    void onPanicClicked();
    void onCopyCodeClicked();
    void onFetchLogClicked();
    void onClearLogClicked();
    void onStateChanged(const TerracottaTypes::StateResponse& state);
    void onAvailabilityChanged(bool available);

   private:
    void updateStateDisplay(const TerracottaTypes::StateResponse& state);
    void updatePlayerList(const QList<TerracottaTypes::Profile>& profiles);
    void updatePortDisplay();
    void appendLog(const QString& message);
    QString getStateString(TerracottaTypes::State state) const;
    void setUIEnabled(bool enabled);
    void startPollingState();
    void stopPollingState();

   private:
    Ui::TerracottaOnlinePanel* ui;
    bool m_isRefreshing = false;
    QTimer* m_statePollTimer = nullptr;
};
