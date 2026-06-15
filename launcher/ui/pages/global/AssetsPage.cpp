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

    ui->verificationModeComboBox->addItem(tr("总是校验（AlwaysVerify）"), AssetVerificationMode::AlwaysVerify);
    ui->verificationModeComboBox->addItem(tr("仅检查文件存在与大小（CheckExistence）"), AssetVerificationMode::CheckExistence);
    ui->verificationModeComboBox->addItem(tr("按过期时间缓存（CacheWithExpiry）"), AssetVerificationMode::CacheWithExpiry);
    ui->verificationModeComboBox->addItem(tr("完全跳过（SkipVerification）"), AssetVerificationMode::SkipVerification);

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
            desc = tr("每次启动都重新从服务器下载资源索引并联网检查所有文件。最安全，但每次都需要联网，在网络不通时将无法启动。");
            break;
        case AssetVerificationMode::CheckExistence:
            desc = tr("（推荐）不强制重新下载索引，只在文件不存在或大小不匹配时才联网下载缺失的资源。网络不通时只要文件完整即可正常启动。");
            break;
        case AssetVerificationMode::CacheWithExpiry:
            desc = tr("使用缓存有效期机制：索引文件在有效期内不会重新下载，超期后才重新检查。平衡了启动速度与文件新鲜度。");
            break;
        case AssetVerificationMode::SkipVerification:
            desc = tr("完全跳过所有资源检查，启动最快。仅在确认本地文件完整时使用，否则可能因文件损坏导致游戏崩溃。");
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
