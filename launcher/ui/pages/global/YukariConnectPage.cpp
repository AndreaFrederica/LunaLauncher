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

#include "YukariConnectPage.h"
#include "ui_YukariConnectPage.h"

#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>

#include "Application.h"
#include <BuildConfig.h>
#include "DesktopServices.h"
#include "minecraft/online/YukariConnect.h"
#include "minecraft/online/YukariConnectDownload.h"
#include "settings/SettingsObject.h"
#include "ui/dialogs/ProgressDialog.h"
#include "ui/pages/global/YukariConnectOnlinePanel.h"

YukariConnectPage::YukariConnectPage(QWidget* parent) : QWidget(parent), ui(new Ui::YukariConnectPage)
{
    ui->setupUi(this);

    // Setup download source combo box
    ui->comboBox_download_source->addItem(tr("GitHub (Official)"), static_cast<int>(YukariConnectDownloadSource::GitHub));
    ui->comboBox_download_source->addItem(tr("Gitee (Mirror for China)"), static_cast<int>(YukariConnectDownloadSource::Mirror));

    // Load settings
    auto s = APPLICATION->settings();
    QString url = s->get("YukariConnectServerURL").toString();
    int pollInterval = s->get("YukariConnectPollInterval").toInt();
    int maxLogLines = s->get("YukariConnectMaxLogLines").toInt();
    // Default to true if setting doesn't exist (stop YukariConnect when launcher closes)
    bool stopOnClose = s->contains("YukariConnectStopOnClose") ? s->get("YukariConnectStopOnClose").toBool() : true;

    // Set values from settings
    if (!url.isEmpty()) {
        ui->lineEdit_url->setText(url);
    } else {
        ui->lineEdit_url->setText(YukariConnect::instance().getBaseUrl());
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
    YukariConnect::instance().setStopOnClose(stopOnClose);

    // Connect buttons
    connect(ui->pushButton_download, &QPushButton::clicked, this, &YukariConnectPage::onDownloadButtonClicked);
    connect(ui->pushButton_install_file, &QPushButton::clicked, this, &YukariConnectPage::onInstallFromFileButtonClicked);
    connect(ui->pushButton_delete, &QPushButton::clicked, this, &YukariConnectPage::onDeleteButtonClicked);
    connect(ui->pushButton_open_online, &QPushButton::clicked, this, &YukariConnectPage::onOpenOnlineButtonClicked);
    connect(ui->pushButton_license_info, &QPushButton::clicked, this, &YukariConnectPage::onLicenseInfoButtonClicked);
    connect(ui->pushButton_edit_config, &QPushButton::clicked, this, &YukariConnectPage::onEditConfigButtonClicked);
    connect(ui->lineEdit_url, &QLineEdit::editingFinished, this, [this]() {
        YukariConnect::instance().setBaseUrl(ui->lineEdit_url->text());
        updateStatus();
    });

    // Connect polling interval change
    connect(ui->spinBox_poll_interval, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        // Update the Online Panel's polling interval
        YukariConnectOnlinePanel::setPollingInterval(value);
    });

    // Connect max log lines change
    connect(ui->spinBox_max_logs, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        // Update the Online Panel's max log lines
        YukariConnectOnlinePanel::setMaxLogLines(value);
    });

    // Connect to YukariConnect signals
    connect(&YukariConnect::instance(), &YukariConnect::availabilityChanged, this, &YukariConnectPage::onAvailabilityChanged);
    connect(&YukariConnect::instance(), &YukariConnect::metaChanged, this, &YukariConnectPage::onMetaChanged);

    refreshStatus();
}

YukariConnectPage::~YukariConnectPage()
{
    delete ui;
}

bool YukariConnectPage::apply()
{
    // Save settings
    auto s = APPLICATION->settings();

    // Save URL setting
    QString url = ui->lineEdit_url->text();
    s->set("YukariConnectServerURL", url);
    YukariConnect::instance().setBaseUrl(url);

    // Save polling interval
    s->set("YukariConnectPollInterval", ui->spinBox_poll_interval->value());

    // Save max log lines
    s->set("YukariConnectMaxLogLines", ui->spinBox_max_logs->value());

    // Save stop on close setting
    s->set("YukariConnectStopOnClose", ui->checkBox_stop_on_close->isChecked());
    YukariConnect::instance().setStopOnClose(ui->checkBox_stop_on_close->isChecked());

    return true;
}

void YukariConnectPage::retranslate()
{
    ui->retranslateUi(this);
}

void YukariConnectPage::openedImpl()
{
    // Refresh status when page is opened
    refreshStatus();

    // Set YukariConnect launcher custom string for vendor identification
    // This identifies the launcher in player lists
    if (YukariConnect::instance().isAvailable()) {
        QString launcherString = QString("Luna/%1").arg(BuildConfig.versionString());
        YukariConnect::instance().setLauncherCustomString(launcherString);
    }
}

void YukariConnectPage::onDownloadButtonClicked()
{
    int sourceIndex = ui->comboBox_download_source->currentData().toInt();
    bool useMirror = (static_cast<YukariConnectDownloadSource>(sourceIndex) == YukariConnectDownloadSource::Mirror);

    // Create download task and show progress dialog
    auto task = makeShared<YukariConnectDownload>(useMirror);
    ProgressDialog progressDialog(this);
    progressDialog.setSkipButton(false);

    if (progressDialog.execWithTask(task.get()) == QDialog::Accepted) {
        updateStatus();
        QMessageBox::information(this, tr("Download Complete"),
                                 tr("YukariConnect has been successfully downloaded."));
    } else {
        QMessageBox::warning(this, tr("Download Failed"),
                             tr("Failed to download YukariConnect. Please check your internet connection and try again."));
    }
}

void YukariConnectPage::onInstallFromFileButtonClicked()
{
    // Let user select a YukariConnect executable file
    QString filter;
#ifdef Q_OS_WIN32
    filter = tr("Executable Files (*.exe);;All Files (*)");
#else
    filter = tr("All Files (*)");
#endif

    QString sourceFile = QFileDialog::getOpenFileName(
        this,
        tr("Select YukariConnect Executable"),
        QString(),
        filter
    );

    if (sourceFile.isEmpty()) {
        return;  // User cancelled
    }

    // Verify the file exists and is valid
    QFileInfo sourceInfo(sourceFile);
    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
        QMessageBox::warning(this, tr("Invalid File"),
                             tr("The selected file is not valid."));
        return;
    }

    // Check file size - YukariConnect executable should be at least 1MB
    if (sourceInfo.size() < 1024 * 1024) {
        auto reply = QMessageBox::warning(
            this,
            tr("File Too Small"),
            tr("The selected file seems too small to be a valid YukariConnect executable (%1 MB).\n\nAre you sure you want to continue?")
                .arg(sourceInfo.size() / 1024.0 / 1024.0, 0, 'f', 2),
            QMessageBox::Yes | QMessageBox::No
        );
        if (reply != QMessageBox::Yes) {
            return;
        }
    }

    // Get target path
    QString targetPath = YukariConnect::instance().getLocalPath();
    QString targetDir = QFileInfo(targetPath).absolutePath();

    // Ensure target directory exists
    QDir().mkpath(targetDir);

    // Remove existing file if it exists
    if (QFile::exists(targetPath)) {
        QFile::remove(targetPath);
    }

    // Copy the file
    if (QFile::copy(sourceFile, targetPath)) {
        // Make executable on Unix-like systems
#ifndef Q_OS_WIN32
        QFile::setPermissions(targetPath, QFile::permissions(targetPath) | QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther);
#endif

        updateStatus();
        QMessageBox::information(this, tr("Installation Complete"),
                                 tr("YukariConnect has been successfully installed from:\n%1").arg(sourceFile));
    } else {
        QMessageBox::warning(this, tr("Installation Failed"),
                             tr("Failed to copy YukariConnect to the target location.\n\nTarget: %1").arg(targetPath));
    }
}

void YukariConnectPage::onDeleteButtonClicked()
{
    QString path = YukariConnect::instance().getLocalPath();
    QString metadataPath = YukariConnect::instance().getMetadataPath();
    QFileInfo fileInfo(path);

    if (!fileInfo.exists()) {
        QMessageBox::information(this, tr("File Not Found"), tr("YukariConnect is not installed."));
        return;
    }

    auto reply = QMessageBox::question(this, tr("Delete YukariConnect"),
                                        tr("Are you sure you want to delete YukariConnect?\n\nThis will prevent P2P multiplayer from working until you download it again."),
                                        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        bool exeRemoved = QFile::remove(path);
        QFile::remove(metadataPath);  // Also remove metadata

        if (exeRemoved) {
            updateStatus();
            QMessageBox::information(this, tr("Deleted"), tr("YukariConnect has been deleted."));
        } else {
            QMessageBox::warning(this, tr("Delete Failed"), tr("Failed to delete YukariConnect.\n\nYou may need to manually delete the file at:\n%1").arg(path));
        }
    }
}

void YukariConnectPage::onOpenOnlineButtonClicked()
{
    // Use the same singleton panel instance as MainWindow
    static YukariConnectOnlinePanel* panel = nullptr;
    if (!panel) {
        // Don't set parent to make it a truly independent window that won't stay on top
        panel = new YukariConnectOnlinePanel(nullptr);
    }
    panel->show();
    panel->raise();
    panel->activateWindow();
}

void YukariConnectPage::onLicenseInfoButtonClicked()
{
    QString aboutText = tr(
        "<h3>YukariConnect P2P Multiplayer</h3>"
        "<p><b>P2P Multiplayer Solution</b></p>"
        "<p>YukariConnect is a P2P multiplayer solution for Minecraft that allows players to connect without port forwarding or central servers.</p>"
        "<h4>YukariConnect Project</h4>"
        "<p><b>Developer:</b> AndreaFrederica  AuraElicase<br>"
        "<b>Project URL:</b> <a href=\"https://github.com/ElicaseTech/YukariConnect\">https://github.com/ElicaseTech/YukariConnect</a></p>"
        "<h4>License</h4>"
        "<p>YukariConnect is licensed under the <b>Mozilla Public License 2.0 (MPL-2.0)</b>.</p>"
    );

    QMessageBox::about(this, tr("About YukariConnect"), aboutText);
}

void YukariConnectPage::onAvailabilityChanged(bool available)
{
    if (isVisible()) {
        refreshStatus();
    }
}

void YukariConnectPage::refreshStatus()
{
    updateStatus();
}

void YukariConnectPage::onMetaChanged(const YukariConnectTypes::MetaInfo& meta)
{
    // Only update UI if page is visible to prevent crashes
    if (!isVisible()) {
        qDebug() << "Meta changed but page not visible, skipping UI update";
        return;
    }
    qDebug() << "Meta changed, updating version display:" << meta.version;
    ui->label_version_value->setText(meta.version);
}

void YukariConnectPage::updateStatus()
{
    QString path = YukariConnect::instance().getLocalPath();
    QFileInfo fileInfo(path);

    if (fileInfo.exists()) {
        ui->label_installed_value->setText(tr("Yes"));

        // Display version
        QString version = YukariConnect::instance().getVersion();
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

void YukariConnectPage::onEditConfigButtonClicked()
{
    QString configPath = YukariConnect::instance().getConfigPath();
    QFileInfo fileInfo(configPath);

    // Check if config file exists
    if (!fileInfo.exists()) {
        auto reply = QMessageBox::question(
            this,
            tr("Config File Not Found"),
            tr("The configuration file (yukari.json) does not exist yet.\n\nWould you like to create a default configuration file?"),
            QMessageBox::Yes | QMessageBox::No
        );

        if (reply == QMessageBox::Yes) {
            // Create default config
            QString defaultConfig = QString("{\n"
                "  \"HttpPort\": 5062,\n"
                "  \"DefaultScaffoldingPort\": 13448,\n"
                "  \"LauncherCustomString\": null,\n"
                "  \"TerracottaCompatibilityMode\": true,\n"
                "  \"McServerOfflineThreshold\": 6,\n"
                "  \"EasyTierStartupTimeoutSeconds\": 12,\n"
                "  \"CenterDiscoveryTimeoutSeconds\": 25\n"
                "}\n");

            QFile configFile(configPath);
            if (configFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                configFile.write(defaultConfig.toUtf8());
                configFile.close();
                QMessageBox::information(this, tr("Config Created"),
                    tr("Default configuration file created at:\n%1").arg(configPath));
            } else {
                QMessageBox::warning(this, tr("Failed to Create Config"),
                    tr("Could not create configuration file at:\n%1").arg(configPath));
                return;
            }
        } else {
            return;
        }
    }

    // Open config file in system text editor
    if (!DesktopServices::openPath(configPath)) {
        QMessageBox::warning(this, tr("Failed to Open Config"),
            tr("Could not open configuration file.\n\nLocation: %1\n\nYou can open it manually in a text editor.").arg(configPath));
    }
}
