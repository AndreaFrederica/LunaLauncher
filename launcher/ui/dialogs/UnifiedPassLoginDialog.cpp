// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2024 The Luna Launcher Contributors
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

#include "UnifiedPassLoginDialog.h"
#include "ui_UnifiedPassLoginDialog.h"

#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QDesktopServices>

#include "minecraft/auth/AuthFlow.h"
#include "minecraft/auth/steps/YggdrasilProfileSelectStep.h"

UnifiedPassLoginDialog::UnifiedPassLoginDialog(QWidget* parent) : QDialog(parent), ui(new Ui::UnifiedPassLoginDialog)
{
    ui->setupUi(this);

    // Set placeholder text for username
    ui->usernameEdit->setPlaceholderText(tr("UnifiedPass username or QQ email"));

    // Connect button signals
    connect(ui->loginButton, &QPushButton::clicked, this, &UnifiedPassLoginDialog::startLogin);
    connect(ui->cancelButton, &QPushButton::clicked, this, &UnifiedPassLoginDialog::reject);
    connect(ui->registerButton, &QPushButton::clicked, this, &UnifiedPassLoginDialog::onRegisterButtonClicked);

    // Enable login button only when required fields are filled
    auto validateInput = [this]() {
        bool valid = !ui->usernameEdit->text().isEmpty() && !ui->passwordEdit->text().isEmpty() &&
                     !ui->serverIdEdit->text().isEmpty();
        ui->loginButton->setEnabled(valid);
    };

    connect(ui->usernameEdit, &QLineEdit::textChanged, validateInput);
    connect(ui->passwordEdit, &QLineEdit::textChanged, validateInput);
    connect(ui->serverIdEdit, &QLineEdit::textChanged, validateInput);

    // Initially disable login button
    ui->loginButton->setEnabled(false);
}

UnifiedPassLoginDialog::~UnifiedPassLoginDialog()
{
    delete ui;
}

void UnifiedPassLoginDialog::onRegisterButtonClicked()
{
    QString serverId = ui->serverIdEdit->text().trimmed();
    if (serverId.isEmpty()) {
        ui->statusLabel->setText(tr("Please enter server ID first"));
        ui->statusLabel->setStyleSheet("QLabel { color: orange; }");
        return;
    }

    QString registerUrl = QString("https://login.mc-user.com:233/%1/loginreg").arg(serverId);
    QDesktopServices::openUrl(QUrl(registerUrl));
}

void UnifiedPassLoginDialog::startLogin()
{
    QString username = ui->usernameEdit->text().trimmed();
    QString password = ui->passwordEdit->text();
    QString serverId = ui->serverIdEdit->text().trimmed();

    // Create account
    m_account = MinecraftAccount::createUnifiedPass(username, password, serverId);

    // Start auth flow
    m_authflow_task = m_account->login(false);

    connect(m_authflow_task.get(), &Task::failed, this, &UnifiedPassLoginDialog::onTaskFailed);
    connect(m_authflow_task.get(), &Task::succeeded, this, &UnifiedPassLoginDialog::onAuthFlowSucceeded);
    connect(m_authflow_task.get(), &Task::aborted, this, &UnifiedPassLoginDialog::onAuthFlowAborted);
    connect(m_authflow_task.get(), &Task::status, this, &UnifiedPassLoginDialog::onAuthFlowStatus);

    m_authflow_task->start();

    ui->loginButton->setEnabled(false);
    ui->loginButton->setText(tr("Logging in..."));
    ui->cancelButton->setText(tr("Cancel"));
}

MinecraftAccountPtr UnifiedPassLoginDialog::newAccount(QWidget* parent)
{
    UnifiedPassLoginDialog dlg(parent);
    if (dlg.exec() == QDialog::Accepted) {
        return dlg.m_account;
    }
    return nullptr;
}

int UnifiedPassLoginDialog::exec()
{
    ui->usernameEdit->setFocus();
    return QDialog::exec();
}

void UnifiedPassLoginDialog::onTaskFailed(QString reason)
{
    ui->statusLabel->setText(tr("Login failed: %1").arg(reason));
    ui->statusLabel->setStyleSheet("QLabel { color: red; }");
    ui->loginButton->setEnabled(true);
    ui->loginButton->setText(tr("Login"));
}

void UnifiedPassLoginDialog::onAuthFlowStatus(QString status)
{
    ui->statusLabel->setText(status);
    ui->statusLabel->setStyleSheet("");
}

void UnifiedPassLoginDialog::onAuthFlowSucceeded()
{
    // Check if we need profile selection
    auto availableProfilesVar = m_account->accountData()->unifiedPassToken.extra.value("availableProfiles");
    if (availableProfilesVar.isValid() && !availableProfilesVar.isNull()) {
        QJsonArray profilesArray = availableProfilesVar.toJsonArray();

        if (profilesArray.size() > 1) {
            // Show profile selection dialog
            // For now, just select the first one automatically
            auto firstProfile = profilesArray[0].toObject();
            QString profileId = firstProfile.value("id").toString();
            QString profileName = firstProfile.value("name").toString();

            // We need to manually set the selected profile and re-run the profile step
            auto profileStep = makeShared<YggdrasilProfileSelectStep>(m_account->accountData());
            profileStep->setSelectedProfile(profileId, profileName);

            // Now run the profile step to get skin/cape info
            m_authflow_task.reset(new AuthFlow(m_account->accountData(), AuthFlow::Action::Login));
            connect(m_authflow_task.get(), &Task::failed, this, &UnifiedPassLoginDialog::onTaskFailed);
            connect(m_authflow_task.get(), &Task::succeeded, this, &UnifiedPassLoginDialog::accept);
            connect(m_authflow_task.get(), &Task::aborted, this, &UnifiedPassLoginDialog::reject);
            connect(m_authflow_task.get(), &Task::status, this, &UnifiedPassLoginDialog::onAuthFlowStatus);

            m_authflow_task->start();
            return;
        }
    }

    accept();
}

void UnifiedPassLoginDialog::onAuthFlowAborted()
{
    ui->statusLabel->setText(tr("Login aborted"));
    ui->statusLabel->setStyleSheet("QLabel { color: orange; }");
    ui->loginButton->setEnabled(true);
    ui->loginButton->setText(tr("Login"));
}
