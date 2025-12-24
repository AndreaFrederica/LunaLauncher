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
#include <QVector>

#include "minecraft/auth/YggdrasilPresets.h"

namespace Ui {
class YggdrasilPresetsDialog;
}

class YggdrasilPresetsDialog : public QDialog {
    Q_OBJECT

   public:
    explicit YggdrasilPresetsDialog(QWidget* parent = nullptr);
    ~YggdrasilPresetsDialog();

   private slots:
    void onAddPreset();
    void onEditPreset();
    void onDeletePreset();
    void onSelectionChanged();

   private:
    void loadPresets();
    void savePresetFromDialog(const YggdrasilPreset& preset);

    Ui::YggdrasilPresetsDialog* ui;
    QVector<YggdrasilPreset> m_presets;
};
