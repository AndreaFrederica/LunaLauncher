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

#include <QWidget>

#include "ui/pages/BasePage.h"

namespace Ui {
class TerracottaPage;
}

enum class TerracottaDownloadSource {
    GitHub,
    Mirror
};

class TerracottaPage : public QWidget, public BasePage {
    Q_OBJECT

   public:
    explicit TerracottaPage(QWidget* parent = nullptr);
    ~TerracottaPage();

    QString displayName() const override { return tr("Terracotta"); }
    QIcon icon() const override { return QIcon::fromTheme("proxy"); }
    QString id() const override { return "terracotta"; }
    QString helpPage() const override { return "terracotta"; }
    virtual bool apply() override;
    void retranslate() override;
    void openedImpl() override;

   private slots:
    void onDownloadButtonClicked();
    void onDeleteButtonClicked();
    void onOpenOnlineButtonClicked();
    void onLicenseInfoButtonClicked();
    void onAvailabilityChanged(bool available);

   private:
    void refreshStatus();
    void updateStatus();

   private:
    Ui::TerracottaPage* ui;
};
