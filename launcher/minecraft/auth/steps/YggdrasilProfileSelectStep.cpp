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

#include "YggdrasilProfileSelectStep.h"

#include "ui/dialogs/YggdrasilProfileSelectDialog.h"

#include <QJsonArray>
#include <QPointer>

YggdrasilProfileSelectStep::YggdrasilProfileSelectStep(AccountData* data) : AuthStep(data) {}

QString YggdrasilProfileSelectStep::describe()
{
    return tr("Selecting Yggdrasil profile");
}

void YggdrasilProfileSelectStep::perform()
{
    // If the profile was already set by the authentication step (server returned selectedProfile),
    // we can skip this step
    if (!m_data->minecraftProfile.id.isEmpty()) {
        emit finished(AccountTaskState::STATE_WORKING, tr("Profile already selected"));
        return;
    }

    // Determine which token to use based on account type
    QVariantMap* tokenExtra = nullptr;
    if (m_data->type == AccountType::UnifiedPass) {
        tokenExtra = &m_data->unifiedPassToken.extra;
    } else {
        tokenExtra = &m_data->yggdrasilToken.extra;
    }

    // Extract available profiles from token extra data
    auto availableProfilesVar = tokenExtra->value("availableProfiles");
    if (availableProfilesVar.isNull() || !availableProfilesVar.canConvert<QJsonArray>()) {
        // No profiles to select from, profile should have been set by auth step
        if (!m_data->minecraftProfile.id.isEmpty()) {
            emit finished(AccountTaskState::STATE_WORKING, tr("Profile already selected"));
        } else {
            emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("No profiles available for this account"));
        }
        return;
    }

    QJsonArray profilesArray = availableProfilesVar.toJsonArray();
    for (const auto& profileValue : profilesArray) {
        m_availableProfiles.append(profileValue.toObject());
    }

    if (m_availableProfiles.isEmpty()) {
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("No profiles available for this account"));
        return;
    }

    // If only one profile, automatically select it
    if (m_availableProfiles.size() == 1) {
        auto profileObj = m_availableProfiles.first();
        QString profileId = profileObj.value("id").toString();
        QString profileName = profileObj.value("name").toString();
        setSelectedProfile(profileId, profileName);
        return;
    }

    // Multiple profiles - show dialog
    QPointer<YggdrasilProfileSelectDialog> dialog = new YggdrasilProfileSelectDialog(m_availableProfiles);
    int result = dialog->exec();
    if (result == QDialog::Accepted && dialog) {
        SelectedProfile profile = dialog->selectedProfile();
        setSelectedProfile(profile.id, profile.name);
    } else {
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Profile selection cancelled"));
    }
    delete dialog;
}

void YggdrasilProfileSelectStep::setSelectedProfile(const QString& profileId, const QString& profileName)
{
    m_data->minecraftProfile.id = profileId;
    m_data->minecraftProfile.name = profileName;
    m_data->minecraftProfile.validity = Validity::Certain;

    // For UnifiedPass, clear credentials and available profiles
    if (m_data->type == AccountType::UnifiedPass) {
        m_data->unifiedPassToken.extra.remove("credentials");
        m_data->unifiedPassToken.extra.remove("availableProfiles");
    } else {
        // For Yggdrasil
        // For OAuth accounts without refresh_token, keep the credentials for re-authentication
        // For standard Yggdrasil or OAuth with refresh_token, clear the credentials
        bool hasRefreshToken = m_data->yggdrasilToken.extra.contains("refreshToken") &&
                               !m_data->yggdrasilToken.extra["refreshToken"].toString().isEmpty();
        bool isOAuthWithoutRefreshToken = m_data->yggdrasilConfig.tokenType == YggdrasilTokenType::OAuth && !hasRefreshToken;

        if (!isOAuthWithoutRefreshToken) {
            // Clear credentials from memory after profile selection
            m_data->yggdrasilToken.extra.remove("credentials");
        }
        m_data->yggdrasilToken.extra.remove("availableProfiles");
    }

    emit finished(AccountTaskState::STATE_WORKING, tr("Profile selected: %1").arg(profileName));
}
