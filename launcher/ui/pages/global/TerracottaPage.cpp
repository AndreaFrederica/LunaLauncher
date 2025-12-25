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
#include "ui/dialogs/ProgressDialog.h"
#include "ui/pages/global/TerracottaOnlinePanel.h"

TerracottaPage::TerracottaPage(QWidget* parent) : QWidget(parent), ui(new Ui::TerracottaPage)
{
    ui->setupUi(this);

    // Setup download source combo box
    ui->comboBox_download_source->addItem(tr("GitHub (Official)"), static_cast<int>(TerracottaDownloadSource::GitHub));
    ui->comboBox_download_source->addItem(tr("Gitee (Mirror for China)"), static_cast<int>(TerracottaDownloadSource::Mirror));

    // Set default URL
    ui->lineEdit_url->setText(Terracotta::instance().getBaseUrl());

    // Connect buttons
    connect(ui->pushButton_download, &QPushButton::clicked, this, &TerracottaPage::onDownloadButtonClicked);
    connect(ui->pushButton_delete, &QPushButton::clicked, this, &TerracottaPage::onDeleteButtonClicked);
    connect(ui->pushButton_open_online, &QPushButton::clicked, this, &TerracottaPage::onOpenOnlineButtonClicked);
    connect(ui->lineEdit_url, &QLineEdit::editingFinished, this, [this]() {
        Terracotta::instance().setBaseUrl(ui->lineEdit_url->text());
        updateStatus();
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
    // Save URL setting
    QString url = ui->lineEdit_url->text();
    Terracotta::instance().setBaseUrl(url);
    // TODO: Save to settings
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
        panel = new TerracottaOnlinePanel(this);
    }
    panel->show();
    panel->raise();
    panel->activateWindow();
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
