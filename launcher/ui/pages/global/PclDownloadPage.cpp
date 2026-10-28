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
#include <QVBoxLayout>

#include "Application.h"
#include "BuildConfig.h"
#include "net/PclDownloadLibrary.h"
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
    m_threadLimit = new QSpinBox(transfers);
    m_threadLimit->setRange(1, 64);
    m_threadLimit->setToolTip(tr("Maximum number of download threads (connections) per task."));
    m_speedLimit = new QSpinBox(transfers);
    m_speedLimit->setRange(0, 1000000);
    m_speedLimit->setSuffix(tr(" KB/s"));
    m_speedLimit->setSpecialValueText(tr("Unlimited"));

    transfersLayout->addRow(tr("Threads per download:"), m_threadLimit);
    transfersLayout->addRow(tr("Speed limit:"), m_speedLimit);
    root->addWidget(transfers);

    root->addStretch();

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
    s->set("PclDownloadSpeedLimitKBps", m_speedLimit->value());
}
