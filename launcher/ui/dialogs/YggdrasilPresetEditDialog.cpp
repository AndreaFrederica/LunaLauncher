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

#include "YggdrasilPresetEditDialog.h"
#include "ui_YggdrasilPresetEditDialog.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QDialogButtonBox>

YggdrasilPresetEditDialog::YggdrasilPresetEditDialog(QWidget* parent) : QDialog(parent), ui(new Ui::YggdrasilPresetEditDialog)
{
    setupUi();
}

YggdrasilPresetEditDialog::YggdrasilPresetEditDialog(const YggdrasilPreset& preset, QWidget* parent)
    : QDialog(parent), ui(new Ui::YggdrasilPresetEditDialog), m_preset(preset)
{
    setupUi();
    loadPreset(preset);
}

YggdrasilPresetEditDialog::~YggdrasilPresetEditDialog()
{
    delete ui;
}

void YggdrasilPresetEditDialog::setupUi()
{
    ui->setupUi(this);
    setWindowTitle(tr("Edit Yggdrasil Preset"));

    auto* mainLayout = new QVBoxLayout(this);

    // Basic info group
    auto* basicGroup = new QGroupBox(tr("Basic Information"), this);
    auto* basicLayout = new QFormLayout(basicGroup);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(tr("e.g., My Yggdrasil Server"));
    basicLayout->addRow(tr("Name:"), m_nameEdit);

    m_authUrlEdit = new QLineEdit(this);
    m_authUrlEdit->setPlaceholderText(tr("https://example.com/auth"));
    basicLayout->addRow(tr("Auth Server URL:"), m_authUrlEdit);

    m_sessionUrlEdit = new QLineEdit(this);
    m_sessionUrlEdit->setPlaceholderText(tr("https://example.com/session"));
    basicLayout->addRow(tr("Session Server URL:"), m_sessionUrlEdit);

    mainLayout->addWidget(basicGroup);

    // Custom endpoints group
    auto* endpointsGroup = new QGroupBox(tr("Custom Endpoints (Optional)"), this);
    auto* endpointsLayout = new QFormLayout(endpointsGroup);

    auto* helpLabel = new QLabel(tr("Leave empty to use default endpoints. Most servers don't need this."), this);
    helpLabel->setWordWrap(true);
    endpointsLayout->addRow(helpLabel);

    m_authenticateEndpointEdit = new QLineEdit(this);
    m_authenticateEndpointEdit->setPlaceholderText(tr("Default: /authserver/authenticate"));
    endpointsLayout->addRow(tr("Authenticate:"), m_authenticateEndpointEdit);

    m_refreshEndpointEdit = new QLineEdit(this);
    m_refreshEndpointEdit->setPlaceholderText(tr("Default: /sessionserver/refresh"));
    endpointsLayout->addRow(tr("Refresh:"), m_refreshEndpointEdit);

    m_validateEndpointEdit = new QLineEdit(this);
    m_validateEndpointEdit->setPlaceholderText(tr("Default: /sessionserver/validate"));
    endpointsLayout->addRow(tr("Validate:"), m_validateEndpointEdit);

    m_profileEndpointEdit = new QLineEdit(this);
    m_profileEndpointEdit->setPlaceholderText(tr("Default: /sessionserver/session/minecraft/profile/{uuid}"));
    endpointsLayout->addRow(tr("Profile:"), m_profileEndpointEdit);

    mainLayout->addWidget(endpointsGroup);

    // Buttons
    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_okButton = buttonBox->button(QDialogButtonBox::Ok);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &YggdrasilPresetEditDialog::onAccepted);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &YggdrasilPresetEditDialog::reject);
    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);
    resize(500, 400);
}

void YggdrasilPresetEditDialog::loadPreset(const YggdrasilPreset& preset)
{
    m_nameEdit->setText(preset.name);
    m_authUrlEdit->setText(preset.authUrl);
    m_sessionUrlEdit->setText(preset.sessionUrl);
    m_authenticateEndpointEdit->setText(preset.authenticateEndpoint);
    m_refreshEndpointEdit->setText(preset.refreshEndpoint);
    m_validateEndpointEdit->setText(preset.validateEndpoint);
    m_profileEndpointEdit->setText(preset.profileEndpoint);
}

void YggdrasilPresetEditDialog::onAccepted()
{
    QString name = m_nameEdit->text().trimmed();
    QString authUrl = m_authUrlEdit->text().trimmed();
    QString sessionUrl = m_sessionUrlEdit->text().trimmed();

    if (name.isEmpty()) {
        m_nameEdit->setFocus();
        return;
    }

    if (authUrl.isEmpty()) {
        m_authUrlEdit->setFocus();
        return;
    }

    if (sessionUrl.isEmpty()) {
        m_sessionUrlEdit->setFocus();
        return;
    }

    m_preset.name = name;
    m_preset.authUrl = authUrl;
    m_preset.sessionUrl = sessionUrl;
    m_preset.authenticateEndpoint = m_authenticateEndpointEdit->text().trimmed();
    m_preset.refreshEndpoint = m_refreshEndpointEdit->text().trimmed();
    m_preset.validateEndpoint = m_validateEndpointEdit->text().trimmed();
    m_preset.profileEndpoint = m_profileEndpointEdit->text().trimmed();

    accept();
}

void YggdrasilPresetEditDialog::onUseDefaultEndpointsToggled(bool checked)
{
    if (checked) {
        m_authenticateEndpointEdit->clear();
        m_refreshEndpointEdit->clear();
        m_validateEndpointEdit->clear();
        m_profileEndpointEdit->clear();
    }
}
