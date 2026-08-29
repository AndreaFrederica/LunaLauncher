// SPDX-License-Identifier: GPL-3.0-only
/*
 *  LunaLauncher - Minecraft Launcher
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
class AssetsPage;
}

class AssetsPage : public QWidget, public BasePage {
    Q_OBJECT

   public:
    explicit AssetsPage(QWidget* parent = nullptr);
    ~AssetsPage();

    QString displayName() const override { return tr("Assets"); }
    QIcon icon() const override { return QIcon::fromTheme("minecraft"); }
    QString id() const override { return "assets-settings"; }
    QString helpPage() const override { return ""; }
    bool apply() override;
    void retranslate() override;

   private slots:
    void onModeChanged(int index);

   private:
    void loadSettings();
    void applySettings();
    void updateDescription(int mode);

   private:
    Ui::AssetsPage* ui;
};
