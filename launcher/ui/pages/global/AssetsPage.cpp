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

#include "AssetsPage.h"
#include "ui_AssetsPage.h"

#include "Application.h"
#include "minecraft/update/AssetUpdateTask.h"
#include "settings/SettingsObject.h"

AssetsPage::AssetsPage(QWidget* parent) : QWidget(parent), ui(new Ui::AssetsPage)
{
    ui->setupUi(this);

    ui->verificationModeComboBox->addItem(tr("Always verify"), AssetVerificationMode::AlwaysVerify);
    ui->verificationModeComboBox->addItem(tr("Check existence"), AssetVerificationMode::CheckExistence);
    ui->verificationModeComboBox->addItem(tr("Cache with expiry"), AssetVerificationMode::CacheWithExpiry);
    ui->verificationModeComboBox->addItem(tr("Skip verification"), AssetVerificationMode::SkipVerification);

    void (QComboBox::*currentIndexChangedSignal)(int)(&QComboBox::currentIndexChanged);
    connect(ui->verificationModeComboBox, currentIndexChangedSignal, this, &AssetsPage::onModeChanged);

    loadSettings();
}

AssetsPage::~AssetsPage()
{
    delete ui;
}

void AssetsPage::onModeChanged(int index)
{
    int mode = ui->verificationModeComboBox->itemData(index).toInt();
    ui->expiryGroupBox->setEnabled(mode == AssetVerificationMode::CacheWithExpiry);
    updateDescription(mode);
}

void AssetsPage::updateDescription(int mode)
{
    QString desc;
    switch (mode) {
        case AssetVerificationMode::AlwaysVerify:
            desc = tr("Refresh the asset index and verify the SHA-1 of local asset files on every launch. This requires network access when the index is expired or files need repair.");
            break;
        case AssetVerificationMode::CheckExistence:
            desc = tr("Recommended. Reuse a valid cached index and download assets only when they are missing or have the wrong size. Complete local caches can be used offline.");
            break;
        case AssetVerificationMode::CacheWithExpiry:
            desc = tr("Reuse the index for the configured number of days, then refresh it. This balances startup speed and freshness.");
            break;
        case AssetVerificationMode::SkipVerification:
            desc = tr("Skip the index and asset checks completely when the local index exists. Use only when the local files are known to be complete.");
            break;
        default:
            break;
    }
    ui->modeDescriptionLabel->setText(desc);
}

void AssetsPage::loadSettings()
{
    auto s = APPLICATION->settings();

    int mode = s->get("AssetVerificationMode").toInt();
    int modeIndex = ui->verificationModeComboBox->findData(mode);
    if (modeIndex == -1)
        modeIndex = ui->verificationModeComboBox->findData(AssetVerificationMode::CheckExistence);
    ui->verificationModeComboBox->setCurrentIndex(modeIndex);

    int expiryDays = s->get("AssetCacheExpiryDays").toInt();
    ui->expiryDaysSpinBox->setValue(expiryDays);

    // Trigger description update
    onModeChanged(ui->verificationModeComboBox->currentIndex());
}

void AssetsPage::applySettings()
{
    auto s = APPLICATION->settings();

    int mode = ui->verificationModeComboBox->currentData().toInt();
    s->set("AssetVerificationMode", mode);

    s->set("AssetCacheExpiryDays", ui->expiryDaysSpinBox->value());
}

bool AssetsPage::apply()
{
    applySettings();
    return true;
}

void AssetsPage::retranslate()
{
    ui->retranslateUi(this);
}
