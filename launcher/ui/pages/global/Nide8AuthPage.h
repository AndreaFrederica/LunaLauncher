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
class Nide8AuthPage;
}

class Nide8AuthPage : public QWidget, public BasePage {
    Q_OBJECT

   public:
    explicit Nide8AuthPage(QWidget* parent = 0);
    ~Nide8AuthPage();

    QString displayName() const override { return tr("nide8auth"); }
    QIcon icon() const override { return QIcon::fromTheme("accounts"); }
    QString id() const override { return "nide8auth"; }
    QString helpPage() const override { return "nide8auth"; }
    virtual bool apply() override;
    void retranslate() override;

   private slots:
    void onDownloadButtonClicked();
    void onDeleteButtonClicked();
    void refreshStatus();

   private:
    void updateStatus();
    void updateJvmArgs();

   private:
    Ui::Nide8AuthPage* ui;
};
