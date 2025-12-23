// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2024 The Prism Launcher Contributors
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

#include <QDialog>
#include <QJsonObject>
#include <QVector>
#include <QListWidgetItem>

namespace Ui {
class YggdrasilProfileSelectDialog;
}

struct SelectedProfile {
    QString id;
    QString name;
};

class YggdrasilProfileSelectDialog : public QDialog {
    Q_OBJECT

   public:
    explicit YggdrasilProfileSelectDialog(const QVector<QJsonObject>& profiles, QWidget* parent = nullptr);
    ~YggdrasilProfileSelectDialog();

    SelectedProfile selectedProfile() const { return m_selectedProfile; }

   private slots:
    void onProfileSelected();
    void onProfileDoubleClicked(QListWidgetItem* item);

   private:
    Ui::YggdrasilProfileSelectDialog* ui;
    SelectedProfile m_selectedProfile;
    QVector<QJsonObject> m_profiles;
};
