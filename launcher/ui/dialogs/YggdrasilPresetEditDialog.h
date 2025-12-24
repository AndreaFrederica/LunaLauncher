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

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>

#include "minecraft/auth/YggdrasilPresets.h"

namespace Ui {
class YggdrasilPresetEditDialog;
}

class YggdrasilPresetEditDialog : public QDialog {
    Q_OBJECT

   public:
    explicit YggdrasilPresetEditDialog(QWidget* parent = nullptr);
    explicit YggdrasilPresetEditDialog(const YggdrasilPreset& preset, QWidget* parent = nullptr);
    ~YggdrasilPresetEditDialog();

    YggdrasilPreset preset() const { return m_preset; }

   private slots:
    void onAccepted();
    void onUseDefaultEndpointsToggled(bool checked);

   private:
    void setupUi();
    void loadPreset(const YggdrasilPreset& preset);

    Ui::YggdrasilPresetEditDialog* ui;
    YggdrasilPreset m_preset;
    QLineEdit* m_nameEdit;
    QLineEdit* m_authUrlEdit;
    QLineEdit* m_sessionUrlEdit;
    QLineEdit* m_authenticateEndpointEdit;
    QLineEdit* m_refreshEndpointEdit;
    QLineEdit* m_validateEndpointEdit;
    QLineEdit* m_profileEndpointEdit;
    QPushButton* m_okButton;
};
