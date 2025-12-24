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

#include "YggdrasilPresetsDialog.h"
#include "ui_YggdrasilPresetsDialog.h"

#include "YggdrasilPresetEditDialog.h"
#include <QMessageBox>

YggdrasilPresetsDialog::YggdrasilPresetsDialog(QWidget* parent) : QDialog(parent), ui(new Ui::YggdrasilPresetsDialog)
{
    ui->setupUi(this);

    loadPresets();

    connect(ui->addButton, &QPushButton::clicked, this, &YggdrasilPresetsDialog::onAddPreset);
    connect(ui->editButton, &QPushButton::clicked, this, &YggdrasilPresetsDialog::onEditPreset);
    connect(ui->deleteButton, &QPushButton::clicked, this, &YggdrasilPresetsDialog::onDeletePreset);
    connect(ui->closeButton, &QPushButton::clicked, this, &YggdrasilPresetsDialog::accept);
    connect(ui->presetList, &QListWidget::itemSelectionChanged, this, &YggdrasilPresetsDialog::onSelectionChanged);

    onSelectionChanged();
}

YggdrasilPresetsDialog::~YggdrasilPresetsDialog()
{
    delete ui;
}

void YggdrasilPresetsDialog::loadPresets()
{
    m_presets = YggdrasilPresets::getCustomPresets();

    ui->presetList->clear();

    for (const auto& preset : m_presets) {
        QListWidgetItem* item = new QListWidgetItem(ui->presetList);
        item->setText(preset.name);
        ui->presetList->addItem(item);
    }

    if (ui->presetList->count() == 0) {
        QListWidgetItem* item = new QListWidgetItem(ui->presetList);
        item->setText(tr("No custom presets"));
        item->setFlags(item->flags() & ~Qt::ItemIsEnabled & ~Qt::ItemIsSelectable);
        ui->presetList->addItem(item);
    }
}

void YggdrasilPresetsDialog::onSelectionChanged()
{
    bool hasSelection = ui->presetList->currentItem() && ui->presetList->currentItem()->isSelected();
    bool isCustom = false;

    if (hasSelection) {
        int row = ui->presetList->currentRow();
        if (row >= 0 && row < m_presets.size()) {
            isCustom = true;
        }
    }

    ui->editButton->setEnabled(isCustom);
    ui->deleteButton->setEnabled(isCustom);
}

void YggdrasilPresetsDialog::onAddPreset()
{
    YggdrasilPresetEditDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        YggdrasilPreset preset = dialog.preset();
        if (YggdrasilPresets::addCustomPreset(preset)) {
            loadPresets();
        } else {
            QMessageBox::warning(this, tr("Error"), tr("A preset with this name already exists."));
        }
    }
}

void YggdrasilPresetsDialog::onEditPreset()
{
    QListWidgetItem* currentItem = ui->presetList->currentItem();
    if (!currentItem) {
        return;
    }

    int row = ui->presetList->currentRow();
    if (row < 0 || row >= m_presets.size()) {
        return;
    }

    const auto& preset = m_presets[row];

    YggdrasilPresetEditDialog dialog(preset, this);
    if (dialog.exec() == QDialog::Accepted) {
        YggdrasilPreset newPreset = dialog.preset();

        // Delete old preset and add new one
        if (newPreset.name != preset.name) {
            YggdrasilPresets::removeCustomPreset(preset.name);
        }

        if (YggdrasilPresets::addCustomPreset(newPreset)) {
            loadPresets();
        } else {
            QMessageBox::warning(this, tr("Error"), tr("A preset with this name already exists."));
        }
    }
}

void YggdrasilPresetsDialog::onDeletePreset()
{
    QListWidgetItem* currentItem = ui->presetList->currentItem();
    if (!currentItem) {
        return;
    }

    int row = ui->presetList->currentRow();
    if (row < 0 || row >= m_presets.size()) {
        return;
    }

    const auto& preset = m_presets[row];

    auto reply = QMessageBox::question(this, tr("Delete Preset"),
                                        tr("Are you sure you want to delete the preset \"%1\"?").arg(preset.name),
                                        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        if (YggdrasilPresets::removeCustomPreset(preset.name)) {
            loadPresets();
        } else {
            QMessageBox::warning(this, tr("Error"), tr("Failed to delete the preset."));
        }
    }
}
