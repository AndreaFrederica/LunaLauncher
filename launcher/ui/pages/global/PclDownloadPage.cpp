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

#include "ui/pages/global/PclDownloadPage.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QMessageBox>
#include <QVBoxLayout>

#include "Application.h"
#include "BuildConfig.h"
#include "net/PclDownloadLibrary.h"
#include "minecraft/LibraryDownloadPatches.h"
#include "settings/SettingsObject.h"
#include "ui/pages/global/APIPage.h"

PclDownloadPage::PclDownloadPage(QWidget* parent) : QWidget(parent)
{
    auto root = new QVBoxLayout(this);

    auto general = new QGroupBox(tr("Download Engine"), this);
    auto generalLayout = new QFormLayout(general);
    m_enabled = new QCheckBox(tr("Use PCL.Download for direct file downloads"), general);
    m_fallbackToQt = new QCheckBox(tr("Fall back to the built-in downloader when PCL.Download is unavailable"), general);
    m_status = new QLabel(general);
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_status->setWordWrap(true);

    generalLayout->addRow(m_enabled);
    generalLayout->addRow(m_fallbackToQt);
    generalLayout->addRow(tr("Status:"), m_status);
    root->addWidget(general);

    auto transfers = new QGroupBox(tr("Transfer Options"), this);
    auto transfersLayout = new QFormLayout(transfers);
    m_smartConcurrency = new QCheckBox(tr("智能并发上限"), transfers);
    auto smartHint = new QLabel(tr("仅适用于 PCL 下载：小文件（≤256 KiB）优先，总并发最多 32；大文件及大小未知文件最多 8。关闭后恢复手动上限。"), transfers);
    smartHint->setWordWrap(true);
    transfersLayout->addRow(m_smartConcurrency);
    transfersLayout->addRow(smartHint);
    m_threadLimit = new QSpinBox(transfers);
    m_threadLimit->setRange(1, 64);
    m_threadLimit->setToolTip(tr("所有 PCL 下载任务共享的文件并发上限；仅在关闭智能并发时生效。"));
    connect(m_smartConcurrency, &QCheckBox::toggled, m_threadLimit, &QWidget::setDisabled);
    m_chunkLimit = new QSpinBox(transfers);
    m_chunkLimit->setRange(1, 16);
    m_chunkLimit->setToolTip(tr("每个大文件的分段连接上限。小文件或不支持 Range 的服务器使用单连接；独立于同时下载文件数。"));
    m_speedLimit = new QSpinBox(transfers);
    m_speedLimit->setRange(0, 1000000);
    m_speedLimit->setSuffix(tr(" KB/s"));
    m_speedLimit->setSpecialValueText(tr("Unlimited"));

    transfersLayout->addRow(tr("手动文件并发上限："), m_threadLimit);
    transfersLayout->addRow(tr("单文件分段连接数："), m_chunkLimit);
    transfersLayout->addRow(tr("Speed limit:"), m_speedLimit);
    m_cacheArtifacts = new QCheckBox(tr("保留下载文件副本（模组 JAR / 整合包，默认关闭）"), transfers);
    m_cacheArtifacts->setToolTip(tr("关闭时使用临时下载文件，退出时清理；不影响实例内的模组、游戏依赖和已安装 Java。已有下载副本需手动清理。"));
    transfersLayout->addRow(m_cacheArtifacts);
    root->addWidget(transfers);

    auto patches = new QGroupBox(tr("库下载兼容补丁"), this);
    auto patchesLayout = new QVBoxLayout(patches);
    m_libraryPatches = new QCheckBox(tr("启用库下载补丁（默认关闭；适用于所有下载引擎）"), patches);
    patchesLayout->addWidget(m_libraryPatches);
    auto patchHint = new QLabel(tr("包内有效文件优先。缺少文件时按规则替换地址，或在原地址失败后尝试备选地址；保留原校验值。用户覆盖按规则 ID 替换基础规则，保存后下次任务生效。"), patches);
    patchHint->setWordWrap(true);
    patchesLayout->addWidget(patchHint);
    m_patchStatus = new QPlainTextEdit(patches);
    m_patchStatus->setReadOnly(true);
    m_patchStatus->setMaximumHeight(130);
    patchesLayout->addWidget(m_patchStatus);
    auto patchButtons = new QHBoxLayout();
    for (const auto overlay : {false, true}) {
        auto button = new QPushButton(overlay ? tr("打开用户覆盖文件") : tr("打开基础文件"), patches);
        patchButtons->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, overlay] {
            QString error;
            if (!LibraryDownloadPatches::openFile(overlay, error)) QMessageBox::warning(this, tr("库下载补丁"), error);
            refreshPatches();
        });
    }
    auto reloadPatches = new QPushButton(tr("重新加载"), patches);
    patchButtons->addWidget(reloadPatches);
    connect(reloadPatches, &QPushButton::clicked, this, &PclDownloadPage::refreshPatches);
    patchesLayout->addLayout(patchButtons);
    root->addWidget(patches);
    root->addStretch();
    refreshPatches();

    // Engine status
    if (!BuildConfig.PCL_DOWNLOAD_ENABLED) {
        m_status->setText(tr("Not built in (build_pcl_download=false)."));
    } else if (PclDownloadLibrary::instance().isLoaded() || PclDownloadLibrary::instance().load()) {
        m_status->setText(tr("PCL.Download library loaded."));
    } else {
        m_status->setText(tr("PCL.Download library not found: %1").arg(PclDownloadLibrary::instance().errorString()));
    }

    connect(m_enabled, &QCheckBox::toggled, this, [this](bool enabled) {
        if (!m_container)
            return;
        auto* apiPage = dynamic_cast<APIPage*>(m_container->getPage("apis"));
        if (apiPage) {
            apiPage->setPclDownloadEnabled(enabled);
        }
    });

    loadSettings();
}

void PclDownloadPage::setEnabledFromBackend(bool enabled)
{
    const QSignalBlocker blocker(m_enabled);
    m_enabled->setChecked(enabled);
}

bool PclDownloadPage::apply()
{
    applySettings();

    // Apply transfer limits to the engine immediately.
    if (PclDownloadLibrary::instance().isLoaded()) {
        PclDownloadLibrary::instance().setThreadLimit(APPLICATION->settings()->get("PclDownloadThreadLimit").toInt());
        PclDownloadLibrary::instance().setChunkLimit(APPLICATION->settings()->get("PclDownloadChunkLimit").toInt());
        PclDownloadLibrary::instance().setSmartConcurrency(APPLICATION->settings()->get("PclDownloadSmartConcurrency").toBool());
        PclDownloadLibrary::instance().setSpeedLimit(
            static_cast<qint64>(APPLICATION->settings()->get("PclDownloadSpeedLimitKBps").toInt()) * 1024);
    }
    return true;
}

void PclDownloadPage::loadSettings()
{
    auto s = APPLICATION->settings();
    m_enabled->setChecked(s->get("DownloadBackend").toInt() == 2);
    m_fallbackToQt->setChecked(s->get("PclDownloadFallbackToQt").toBool());
    m_threadLimit->setValue(s->get("PclDownloadThreadLimit").toInt());
    m_chunkLimit->setValue(s->get("PclDownloadChunkLimit").toInt());
    m_cacheArtifacts->setChecked(s->get("CacheDownloadArtifacts").toBool());
    m_smartConcurrency->setChecked(s->get("PclDownloadSmartConcurrency").toBool());
    m_libraryPatches->setChecked(s->get("LibraryDownloadPatchesEnabled").toBool());
    m_threadLimit->setDisabled(m_smartConcurrency->isChecked());
    m_speedLimit->setValue(s->get("PclDownloadSpeedLimitKBps").toInt());
}

void PclDownloadPage::applySettings()
{
    auto s = APPLICATION->settings();
    const int backend = s->get("DownloadBackend").toInt();
    if (m_enabled->isChecked()) {
        s->set("DownloadBackend", 2);
    } else if (backend == 2) {
        s->set("DownloadBackend", 0);
    }
    s->set("PclDownloadFallbackToQt", m_fallbackToQt->isChecked());
    s->set("PclDownloadThreadLimit", m_threadLimit->value());
    s->set("PclDownloadChunkLimit", m_chunkLimit->value());
    s->set("CacheDownloadArtifacts", m_cacheArtifacts->isChecked());
    s->set("PclDownloadSmartConcurrency", m_smartConcurrency->isChecked());
    s->set("LibraryDownloadPatchesEnabled", m_libraryPatches->isChecked());
    s->set("PclDownloadSpeedLimitKBps", m_speedLimit->value());
}

void PclDownloadPage::refreshPatches()
{
    const auto config = LibraryDownloadPatches::current();
    auto text = tr("基础文件：%1\n用户覆盖：%2\n合并后的规则：%3 条").arg(LibraryDownloadPatches::basePath(), LibraryDownloadPatches::overlayPath()).arg(config.rules.size());
    if (!config.errors.isEmpty()) text += '\n' + config.errors.join('\n');
    m_patchStatus->setPlainText(text);
}
