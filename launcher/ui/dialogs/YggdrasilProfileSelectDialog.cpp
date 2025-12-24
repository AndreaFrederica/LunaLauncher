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

#include "YggdrasilProfileSelectDialog.h"
#include "ui_YggdrasilProfileSelectDialog.h"

#include <QListWidgetItem>

YggdrasilProfileSelectDialog::YggdrasilProfileSelectDialog(const QVector<QJsonObject>& profiles, QWidget* parent)
    : QDialog(parent), ui(new Ui::YggdrasilProfileSelectDialog), m_profiles(profiles)
{
    ui->setupUi(this);

    // Populate the list with profiles
    for (const auto& profile : m_profiles) {
        QString name = profile.value("name").toString();
        QString id = profile.value("id").toString();

        QListWidgetItem* item = new QListWidgetItem(ui->profileList);
        item->setText(QString("%1 (%2)").arg(name, id));
        item->setData(Qt::UserRole, id);
        ui->profileList->addItem(item);
    }

    // Select the first profile by default
    if (ui->profileList->count() > 0) {
        ui->profileList->setCurrentRow(0);
    }

    // Connect signals
    connect(ui->profileList, &QListWidget::itemDoubleClicked, this, &YggdrasilProfileSelectDialog::onProfileDoubleClicked);
    connect(ui->selectButton, &QPushButton::clicked, this, &YggdrasilProfileSelectDialog::onProfileSelected);
    connect(ui->cancelButton, &QPushButton::clicked, this, &YggdrasilProfileSelectDialog::reject);
}

YggdrasilProfileSelectDialog::~YggdrasilProfileSelectDialog()
{
    delete ui;
}

void YggdrasilProfileSelectDialog::onProfileSelected()
{
    QListWidgetItem* currentItem = ui->profileList->currentItem();
    if (!currentItem) {
        return;
    }

    QString profileId = currentItem->data(Qt::UserRole).toString();

    // Find the profile in our list
    for (const auto& profile : m_profiles) {
        if (profile.value("id").toString() == profileId) {
            m_selectedProfile.id = profile.value("id").toString();
            m_selectedProfile.name = profile.value("name").toString();
            break;
        }
    }

    accept();
}

void YggdrasilProfileSelectDialog::onProfileDoubleClicked(QListWidgetItem* item)
{
    Q_UNUSED(item);
    onProfileSelected();
}
