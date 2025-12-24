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

#include "Nide8AuthPage.h"
#include "ui_Nide8AuthPage.h"

#include <QFileInfo>
#include <QMessageBox>

#include "Application.h"
#include "minecraft/auth/Nide8Auth.h"
#include "minecraft/auth/Nide8AuthDownload.h"
#include "ui/dialogs/ProgressDialog.h"

Nide8AuthPage::Nide8AuthPage(QWidget* parent) : QWidget(parent), ui(new Ui::Nide8AuthPage)
{
    ui->setupUi(this);

    // Connect buttons
    connect(ui->pushButton_download, &QPushButton::clicked, this, &Nide8AuthPage::onDownloadButtonClicked);
    connect(ui->pushButton_delete, &QPushButton::clicked, this, &Nide8AuthPage::onDeleteButtonClicked);

    refreshStatus();
}

Nide8AuthPage::~Nide8AuthPage()
{
    delete ui;
}

bool Nide8AuthPage::apply()
{
    // No settings to save
    return true;
}

void Nide8AuthPage::retranslate()
{
    ui->retranslateUi(this);
}

void Nide8AuthPage::refreshStatus()
{
    updateStatus();
    updateJvmArgs();
}

void Nide8AuthPage::updateStatus()
{
    QString path = Nide8Auth::instance().getLocalPath();
    QFileInfo fileInfo(path);

    if (fileInfo.exists()) {
        ui->label_installed_value->setText(tr("Yes"));

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
        ui->label_size_value->setText(tr("-"));
        ui->pushButton_delete->setEnabled(false);
    }

    ui->lineEdit_path->setText(path);
}

void Nide8AuthPage::updateJvmArgs()
{
    QString path = Nide8Auth::instance().getLocalPath();
    QString args = QString("-javaagent:%1={server_id}\n-Dnide8auth.client=true").arg(path);
    ui->plainTextEdit_args->setPlainText(args);
}

void Nide8AuthPage::onDownloadButtonClicked()
{
    // Create download task and show progress dialog
    auto task = makeShared<Nide8AuthDownload>();
    ProgressDialog progressDialog(this);
    progressDialog.setSkipButton(false);

    if (progressDialog.execWithTask(task.get()) == QDialog::Accepted) {
        updateStatus();
        QMessageBox::information(this, tr("Download Complete"),
                                 tr("nide8auth has been successfully downloaded."));
    } else {
        QMessageBox::warning(this, tr("Download Failed"),
                             tr("Failed to download nide8auth. Please check your internet connection and try again."));
    }
}

void Nide8AuthPage::onDeleteButtonClicked()
{
    QString path = Nide8Auth::instance().getLocalPath();
    QFileInfo fileInfo(path);

    if (!fileInfo.exists()) {
        QMessageBox::information(this, tr("File Not Found"), tr("nide8auth is not installed."));
        return;
    }

    auto reply = QMessageBox::question(this, tr("Delete nide8auth"),
                                        tr("Are you sure you want to delete nide8auth?\n\nThis will prevent UnifiedPass authentication from working until you download it again."),
                                        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        bool jarRemoved = QFile::remove(path);

        if (jarRemoved) {
            updateStatus();
            QMessageBox::information(this, tr("Deleted"), tr("nide8auth has been deleted."));
        } else {
            QMessageBox::warning(this, tr("Delete Failed"), tr("Failed to delete nide8auth.\n\nYou may need to manually delete the file at:\n%1").arg(path));
        }
    }
}
