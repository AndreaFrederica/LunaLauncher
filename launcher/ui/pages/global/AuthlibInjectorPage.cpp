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

#include "AuthlibInjectorPage.h"
#include "ui_AuthlibInjectorPage.h"

#include <QFileInfo>
#include <QMessageBox>

#include "Application.h"
#include "minecraft/auth/AuthlibInjector.h"
#include "minecraft/auth/AuthlibInjectorDownload.h"
#include "ui/dialogs/ProgressDialog.h"

enum class DownloadSource {
    BMCLAPI,
    Official
};

AuthlibInjectorPage::AuthlibInjectorPage(QWidget* parent) : QWidget(parent), ui(new Ui::AuthlibInjectorPage)
{
    ui->setupUi(this);

    // Setup download source combo box
    ui->comboBox_download_source->addItem(tr("BMCLAPI (Recommended for China)"), static_cast<int>(DownloadSource::BMCLAPI));
    ui->comboBox_download_source->addItem(tr("Official (authlib-injector.yushi.moe)"), static_cast<int>(DownloadSource::Official));

    // Connect buttons
    connect(ui->pushButton_download, &QPushButton::clicked, this, &AuthlibInjectorPage::onDownloadButtonClicked);
    connect(ui->pushButton_delete, &QPushButton::clicked, this, &AuthlibInjectorPage::onDeleteButtonClicked);

    refreshStatus();
}

AuthlibInjectorPage::~AuthlibInjectorPage()
{
    delete ui;
}

bool AuthlibInjectorPage::apply()
{
    // No settings to save
    return true;
}

void AuthlibInjectorPage::retranslate()
{
    ui->retranslateUi(this);
}

void AuthlibInjectorPage::refreshStatus()
{
    updateStatus();
    updateDownloadSource();
}

void AuthlibInjectorPage::updateStatus()
{
    QString path = AuthlibInjector::instance().getLocalPath();
    QFileInfo fileInfo(path);

    if (fileInfo.exists()) {
        ui->label_installed_value->setText(tr("Yes"));

        // Display version
        QString version = AuthlibInjector::instance().getVersion();
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
}

void AuthlibInjectorPage::updateDownloadSource()
{
    // Update the example JVM arguments display
    QString path = AuthlibInjector::instance().getLocalPath();
    QString args = QString("-javaagent:%1={api_url}\n-Dauthlibinjector.yggdrasil.prefetched={base64_metadata}").arg(path);
    ui->plainTextEdit_args->setPlainText(args);
}

void AuthlibInjectorPage::onDownloadButtonClicked()
{
    // Create download task and show progress dialog
    auto task = makeShared<AuthlibInjectorDownload>();
    ProgressDialog progressDialog(this);
    progressDialog.setSkipButton(false);

    if (progressDialog.execWithTask(task.get()) == QDialog::Accepted) {
        updateStatus();
        QMessageBox::information(this, tr("Download Complete"),
                                 tr("authlib-injector has been successfully downloaded."));
    } else {
        QMessageBox::warning(this, tr("Download Failed"),
                             tr("Failed to download authlib-injector. Please check your internet connection and try again."));
    }
}

void AuthlibInjectorPage::onDeleteButtonClicked()
{
    QString path = AuthlibInjector::instance().getLocalPath();
    QString metadataPath = AuthlibInjector::instance().getMetadataPath();
    QFileInfo fileInfo(path);

    if (!fileInfo.exists()) {
        QMessageBox::information(this, tr("File Not Found"), tr("authlib-injector is not installed."));
        return;
    }

    auto reply = QMessageBox::question(this, tr("Delete authlib-injector"),
                                        tr("Are you sure you want to delete authlib-injector?\n\nThis will prevent Yggdrasil authentication from working until you download it again."),
                                        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        bool jarRemoved = QFile::remove(path);
        QFile::remove(metadataPath);  // Also remove metadata

        if (jarRemoved) {
            updateStatus();
            QMessageBox::information(this, tr("Deleted"), tr("authlib-injector has been deleted."));
        } else {
            QMessageBox::warning(this, tr("Delete Failed"), tr("Failed to delete authlib-injector.\n\nYou may need to manually delete the file at:\n%1").arg(path));
        }
    }
}
