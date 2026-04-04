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

#include "TerracottaPage.h"
#include "ui_TerracottaPage.h"

#include <QFileInfo>
#include <QMessageBox>

#include "Application.h"
#include "minecraft/online/Terracotta.h"
#include "minecraft/online/TerracottaDownload.h"
#include "settings/SettingsObject.h"
#include "ui/dialogs/ProgressDialog.h"
#include "ui/pages/global/TerracottaOnlinePanel.h"

TerracottaPage::TerracottaPage(QWidget* parent) : QWidget(parent), ui(new Ui::TerracottaPage)
{
    ui->setupUi(this);

    // Setup download source combo box
    ui->comboBox_download_source->addItem(tr("GitHub (Official)"), static_cast<int>(TerracottaDownloadSource::GitHub));
    ui->comboBox_download_source->addItem(tr("Gitee (Mirror for China)"), static_cast<int>(TerracottaDownloadSource::Mirror));

    ui->comboBox_startup_mode->addItem(tr("Foreground (Controllable)"), static_cast<int>(Terracotta::StartupMode::Foreground));
    ui->comboBox_startup_mode->addItem(tr("HMCL-compatible (Simulate HMCL)"), static_cast<int>(Terracotta::StartupMode::HMCL));

    // Load settings
    auto s = APPLICATION->settings();
    QString url = s->get("TerracottaServerURL").toString();
    int pollInterval = s->get("TerracottaPollInterval").toInt();
    int maxLogLines = s->get("TerracottaMaxLogLines").toInt();
    // Default to true if setting doesn't exist (stop Terracotta when launcher closes)
    bool stopOnClose = s->contains("TerracottaStopOnClose") ? s->get("TerracottaStopOnClose").toBool() : true;
    int startupMode = s->contains("TerracottaStartupMode") ? s->get("TerracottaStartupMode").toInt() : static_cast<int>(Terracotta::StartupMode::HMCL);

    // Set values from settings
    if (!url.isEmpty()) {
        ui->lineEdit_url->setText(url);
    } else {
        ui->lineEdit_url->setText(Terracotta::instance().getBaseUrl());
    }

    // Set polling interval (default 1000ms if not set)
    if (pollInterval < 100 || pollInterval > 10000) {
        pollInterval = 1000;
    }
    ui->spinBox_poll_interval->setValue(pollInterval);

    // Set max log lines (default 1000 if not set)
    if (maxLogLines < 0 || maxLogLines > 10000) {
        maxLogLines = 1000;
    }
    ui->spinBox_max_logs->setValue(maxLogLines);

    // Set stop on close checkbox (default true if not set)
    ui->checkBox_stop_on_close->setChecked(stopOnClose);
    Terracotta::instance().setStopOnClose(stopOnClose);
    int idx = ui->comboBox_startup_mode->findData(startupMode);
    int fallbackIdx = ui->comboBox_startup_mode->findData(static_cast<int>(Terracotta::StartupMode::HMCL));
    ui->comboBox_startup_mode->setCurrentIndex(idx < 0 ? (fallbackIdx < 0 ? 0 : fallbackIdx) : idx);
    Terracotta::instance().setStartupMode(static_cast<Terracotta::StartupMode>(ui->comboBox_startup_mode->currentData().toInt()));

    // Connect buttons
    connect(ui->pushButton_download, &QPushButton::clicked, this, &TerracottaPage::onDownloadButtonClicked);
    connect(ui->pushButton_delete, &QPushButton::clicked, this, &TerracottaPage::onDeleteButtonClicked);
    connect(ui->pushButton_open_online, &QPushButton::clicked, this, &TerracottaPage::onOpenOnlineButtonClicked);
    connect(ui->pushButton_license_info, &QPushButton::clicked, this, &TerracottaPage::onLicenseInfoButtonClicked);
    connect(ui->lineEdit_url, &QLineEdit::editingFinished, this, [this]() {
        Terracotta::instance().setBaseUrl(ui->lineEdit_url->text());
        updateStatus();
    });
    connect(ui->comboBox_startup_mode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        Terracotta::instance().setStartupMode(static_cast<Terracotta::StartupMode>(ui->comboBox_startup_mode->currentData().toInt()));
    });

    // Connect polling interval change
    connect(ui->spinBox_poll_interval, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        // Update the Online Panel's polling interval
        TerracottaOnlinePanel::setPollingInterval(value);
    });

    // Connect max log lines change
    connect(ui->spinBox_max_logs, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        // Update the Online Panel's max log lines
        TerracottaOnlinePanel::setMaxLogLines(value);
    });

    // Connect to Terracotta signals
    connect(&Terracotta::instance(), &Terracotta::availabilityChanged, this, &TerracottaPage::onAvailabilityChanged);

    refreshStatus();
}

TerracottaPage::~TerracottaPage()
{
    delete ui;
}

bool TerracottaPage::apply()
{
    // Save settings
    auto s = APPLICATION->settings();

    // Save URL setting
    QString url = ui->lineEdit_url->text();
    s->set("TerracottaServerURL", url);
    Terracotta::instance().setBaseUrl(url);

    // Save polling interval
    s->set("TerracottaPollInterval", ui->spinBox_poll_interval->value());

    // Save max log lines
    s->set("TerracottaMaxLogLines", ui->spinBox_max_logs->value());

    // Save stop on close setting
    s->set("TerracottaStopOnClose", ui->checkBox_stop_on_close->isChecked());
    Terracotta::instance().setStopOnClose(ui->checkBox_stop_on_close->isChecked());
    s->set("TerracottaStartupMode", ui->comboBox_startup_mode->currentData().toInt());
    Terracotta::instance().setStartupMode(static_cast<Terracotta::StartupMode>(ui->comboBox_startup_mode->currentData().toInt()));

    return true;
}

void TerracottaPage::retranslate()
{
    ui->retranslateUi(this);
}

void TerracottaPage::openedImpl()
{
    // Refresh status when page is opened
    refreshStatus();
}

void TerracottaPage::onDownloadButtonClicked()
{
    int sourceIndex = ui->comboBox_download_source->currentData().toInt();
    bool useMirror = (static_cast<TerracottaDownloadSource>(sourceIndex) == TerracottaDownloadSource::Mirror);

    // Create download task and show progress dialog
    auto task = makeShared<TerracottaDownload>(useMirror);
    ProgressDialog progressDialog(this);
    progressDialog.setSkipButton(false);

    if (progressDialog.execWithTask(task.get()) == QDialog::Accepted) {
        updateStatus();
        QMessageBox::information(this, tr("Download Complete"),
                                 tr("Terracotta has been successfully downloaded."));
    } else {
        QMessageBox::warning(this, tr("Download Failed"),
                             tr("Failed to download Terracotta. Please check your internet connection and try again."));
    }
}

void TerracottaPage::onDeleteButtonClicked()
{
    QString path = Terracotta::instance().getLocalPath();
    QString metadataPath = Terracotta::instance().getMetadataPath();
    QFileInfo fileInfo(path);

    if (!fileInfo.exists()) {
        QMessageBox::information(this, tr("File Not Found"), tr("Terracotta is not installed."));
        return;
    }

    auto reply = QMessageBox::question(this, tr("Delete Terracotta"),
                                        tr("Are you sure you want to delete Terracotta?\n\nThis will prevent P2P multiplayer from working until you download it again."),
                                        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        bool exeRemoved = QFile::remove(path);
        QFile::remove(metadataPath);  // Also remove metadata

        if (exeRemoved) {
            updateStatus();
            QMessageBox::information(this, tr("Deleted"), tr("Terracotta has been deleted."));
        } else {
            QMessageBox::warning(this, tr("Delete Failed"), tr("Failed to delete Terracotta.\n\nYou may need to manually delete the file at:\n%1").arg(path));
        }
    }
}

void TerracottaPage::onOpenOnlineButtonClicked()
{
    // Use the same singleton panel instance as MainWindow
    static TerracottaOnlinePanel* panel = nullptr;
    if (!panel) {
        // Don't set parent to make it a truly independent window that won't stay on top
        panel = new TerracottaOnlinePanel(nullptr);
    }
    panel->show();
    panel->raise();
    panel->activateWindow();
}

void TerracottaPage::onLicenseInfoButtonClicked()
{
    QString aboutText = tr(
        "<h3>Terracotta P2P Multiplayer</h3>"
        "<p><b>P2P Multiplayer Integration for Luna Launcher</b></p>"
        "<p>This feature integrates with Terracotta, a P2P multiplayer solution for Minecraft.</p>"
        "<h4>Terracotta Project</h4>"
        "<p><b>Developer:</b> burningtnt<br>"
        "<b>Project URL:</b> <a href=\"https://github.com/burningtnt/Terracotta\">https://github.com/burningtnt/Terracotta</a></p>"
        "<h4>License</h4>"
        "<p>Terracotta is licensed under <b>AGPL-3.0</b> with the following exception:</p>"
        "<p><i>\"Your program通过本作品提供的进程间通信接口（如 HTTP API）与未经修改的"
        "本作品应用程序进行交互，不构成衍生作品。\"</i></p>"
        "<p><i>Translation: \"Your program's interaction with an unmodified copy of this work"
        "through the inter-process communication interfaces provided by this work (such as HTTP APIs)"
        "does not constitute a derivative work.\"</i></p>"
        "<h4>Integration Notice</h4>"
        "<p>This integration communicates with the standalone Terracotta binary via its HTTP API only,"
        "which is explicitly permitted under Terracotta's license exception.</p>"
        "<p><b>Note:</b> This is a temporary solution. A future version will replace this with a custom implementation.</p>"
    );

    QMessageBox::about(this, tr("About Terracotta"), aboutText);
}

void TerracottaPage::onAvailabilityChanged(bool available)
{
    if (isVisible()) {
        refreshStatus();
    }
}

void TerracottaPage::refreshStatus()
{
    updateStatus();
}

void TerracottaPage::updateStatus()
{
    QString path = Terracotta::instance().getLocalPath();
    QFileInfo fileInfo(path);

    if (fileInfo.exists()) {
        ui->label_installed_value->setText(tr("Yes"));

        // Display version
        QString version = Terracotta::instance().getVersion();
        ui->label_version_value->setText(version.isEmpty() ? tr("Unknown") : version);

        qint64 fileSizeKB = fileInfo.size() / 1024;
        qint64 fileSizeMB = fileSizeKB / 1024;
        QString sizeStr;
        if (fileSizeMB > 0) {
            sizeStr = tr("%1 MB (%2 bytes)").arg(fileSizeMB).arg(fileInfo.size());
        } else {
            sizeStr = tr("%1 KB (%2 bytes)").arg(fileSizeKB).arg(fileInfo.size());
        }
        ui->label_size_value->setText(sizeStr);
        ui->pushButton_delete->setEnabled(true);
    } else {
        ui->label_installed_value->setText(tr("No"));
        ui->label_version_value->setText(tr("-"));
        ui->label_size_value->setText(tr("-"));
        ui->pushButton_delete->setEnabled(false);
    }

    ui->lineEdit_path->setText(path);

    // Don't fetch server status here to avoid unnecessary connection attempts on startup
    // Server status is shown in the Online Panel when user opens it
}
