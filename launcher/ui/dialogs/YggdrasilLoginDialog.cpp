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

#include "YggdrasilLoginDialog.h"
#include "ui_YggdrasilLoginDialog.h"

#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>

#include "minecraft/auth/AuthFlow.h"
#include "minecraft/auth/steps/YggdrasilProfileSelectStep.h"

YggdrasilLoginDialog::YggdrasilLoginDialog(QWidget* parent) : QDialog(parent), ui(new Ui::YggdrasilLoginDialog)
{
    ui->setupUi(this);

    loadPresets();

    // Connect button signals
    connect(ui->loginButton, &QPushButton::clicked, this, &YggdrasilLoginDialog::startLogin);
    connect(ui->cancelButton, &QPushButton::clicked, this, &YggdrasilLoginDialog::reject);

    // Connect preset combo box
    connect(ui->presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (index > 0 && index <= m_presets.size()) {
            // index 0 is "Custom", actual presets start at index 1
            const auto& preset = m_presets[index - 1];
            ui->authServerEdit->setText(preset.authUrl);
            ui->sessionServerEdit->setText(preset.sessionUrl);
            // Set endpoint configuration from preset
            ui->refreshEndpointEdit->setText(preset.refreshEndpoint);
            ui->validateEndpointEdit->setText(preset.validateEndpoint);
            ui->authenticateEndpointEdit->setText(preset.authenticateEndpoint);
            ui->profileEndpointEdit->setText(preset.profileEndpoint);
            ui->oauthTokenEndpointEdit->setText(preset.oauthTokenEndpoint);
            // Store the current preset's endpoint configuration
            m_currentPreset = preset;
        } else {
            // Clear endpoints when "Custom" is selected
            ui->refreshEndpointEdit->clear();
            ui->validateEndpointEdit->clear();
            ui->authenticateEndpointEdit->clear();
            ui->profileEndpointEdit->clear();
            ui->oauthTokenEndpointEdit->clear();
            m_currentPreset = YggdrasilPreset();
        }
    });

    // Connect presets button
    connect(ui->presetsButton, &QPushButton::clicked, this, [this]() {
        // TODO: Open YggdrasilPresetsDialog
        // For now, just reload presets
        loadPresets();
    });

    // Enable login button only when required fields are filled
    auto validateInput = [this]() {
        bool valid = !ui->usernameEdit->text().isEmpty() && !ui->passwordEdit->text().isEmpty() &&
                     !ui->authServerEdit->text().isEmpty() && !ui->sessionServerEdit->text().isEmpty();
        ui->loginButton->setEnabled(valid);
    };

    connect(ui->usernameEdit, &QLineEdit::textChanged, validateInput);
    connect(ui->passwordEdit, &QLineEdit::textChanged, validateInput);
    connect(ui->authServerEdit, &QLineEdit::textChanged, validateInput);
    connect(ui->sessionServerEdit, &QLineEdit::textChanged, validateInput);

    // Initially disable login button
    ui->loginButton->setEnabled(false);
}

YggdrasilLoginDialog::~YggdrasilLoginDialog()
{
    delete ui;
}

void YggdrasilLoginDialog::loadPresets()
{
    m_presets = YggdrasilPresets::getAllPresets();

    ui->presetCombo->clear();
    ui->presetCombo->addItem(tr("Custom"), -1);

    for (int i = 0; i < m_presets.size(); i++) {
        ui->presetCombo->addItem(m_presets[i].name, i);
    }

    ui->presetCombo->setCurrentIndex(0);
}

void YggdrasilLoginDialog::startLogin()
{
    QString username = ui->usernameEdit->text();
    QString password = ui->passwordEdit->text();
    QString authUrl = ui->authServerEdit->text();
    QString sessionUrl = ui->sessionServerEdit->text();

    // Get endpoint configuration from UI (empty string means use default)
    QString refreshEndpoint = ui->refreshEndpointEdit->text().trimmed();
    QString validateEndpoint = ui->validateEndpointEdit->text().trimmed();
    QString authenticateEndpoint = ui->authenticateEndpointEdit->text().trimmed();
    QString profileEndpoint = ui->profileEndpointEdit->text().trimmed();
    QString oauthTokenEndpoint = ui->oauthTokenEndpointEdit->text().trimmed();

    // Get token type from preset or use default (Standard)
    YggdrasilTokenType tokenType = YggdrasilTokenType::Standard;
    int presetIndex = ui->presetCombo->currentIndex();
    if (presetIndex > 0 && presetIndex <= m_presets.size()) {
        tokenType = m_currentPreset.tokenType;
    }

    // Get source name from preset or use "Custom"
    QString sourceName;
    if (presetIndex > 0 && presetIndex <= m_presets.size()) {
        sourceName = m_presets[presetIndex - 1].name;
    } else {
        sourceName = tr("Custom");
    }

    // Create account
    m_account = MinecraftAccount::createYggdrasil(username, password, authUrl, sessionUrl, sourceName,
                                                   refreshEndpoint, validateEndpoint, authenticateEndpoint, profileEndpoint,
                                                   oauthTokenEndpoint, tokenType);

    // Start auth flow
    m_authflow_task = m_account->login(false);

    connect(m_authflow_task.get(), &Task::failed, this, &YggdrasilLoginDialog::onTaskFailed);
    connect(m_authflow_task.get(), &Task::succeeded, this, &YggdrasilLoginDialog::onAuthFlowSucceeded);
    connect(m_authflow_task.get(), &Task::aborted, this, &YggdrasilLoginDialog::onAuthFlowAborted);
    connect(m_authflow_task.get(), &Task::status, this, &YggdrasilLoginDialog::onAuthFlowStatus);

    m_authflow_task->start();

    ui->loginButton->setEnabled(false);
    ui->loginButton->setText(tr("Logging in..."));
    ui->cancelButton->setText(tr("Cancel"));
}

MinecraftAccountPtr YggdrasilLoginDialog::newAccount(QWidget* parent)
{
    YggdrasilLoginDialog dlg(parent);
    if (dlg.exec() == QDialog::Accepted) {
        return dlg.m_account;
    }
    return nullptr;
}

int YggdrasilLoginDialog::exec()
{
    ui->usernameEdit->setFocus();
    return QDialog::exec();
}

void YggdrasilLoginDialog::onTaskFailed(QString reason)
{
    ui->statusLabel->setText(tr("Login failed: %1").arg(reason));
    ui->statusLabel->setStyleSheet("QLabel { color: red; }");
    ui->loginButton->setEnabled(true);
    ui->loginButton->setText(tr("Login"));
}

void YggdrasilLoginDialog::onAuthFlowStatus(QString status)
{
    ui->statusLabel->setText(status);
    ui->statusLabel->setStyleSheet("");
}

void YggdrasilLoginDialog::onAuthFlowSucceeded()
{
    // Check if we need profile selection
    auto availableProfilesVar = m_account->accountData()->yggdrasilToken.extra.value("availableProfiles");
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
            connect(m_authflow_task.get(), &Task::failed, this, &YggdrasilLoginDialog::onTaskFailed);
            connect(m_authflow_task.get(), &Task::succeeded, this, &YggdrasilLoginDialog::accept);
            connect(m_authflow_task.get(), &Task::aborted, this, &YggdrasilLoginDialog::reject);
            connect(m_authflow_task.get(), &Task::status, this, &YggdrasilLoginDialog::onAuthFlowStatus);

            m_authflow_task->start();
            return;
        }
    }

    accept();
}

void YggdrasilLoginDialog::onAuthFlowAborted()
{
    ui->statusLabel->setText(tr("Login aborted"));
    ui->statusLabel->setStyleSheet("QLabel { color: orange; }");
    ui->loginButton->setEnabled(true);
    ui->loginButton->setText(tr("Login"));
}
