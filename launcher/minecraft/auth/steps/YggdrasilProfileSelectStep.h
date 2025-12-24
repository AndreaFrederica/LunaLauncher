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

#pragma once

#include <QJsonObject>
#include <QObject>

#include "minecraft/auth/AuthStep.h"

/**
 * Step that handles profile selection when multiple profiles are available.
 * This step waits for the user to select a profile through a dialog.
 */
class YggdrasilProfileSelectStep : public AuthStep {
    Q_OBJECT

   public:
    explicit YggdrasilProfileSelectStep(AccountData* data);
    virtual ~YggdrasilProfileSelectStep() noexcept = default;

    void perform() override;
    QString describe() override;

    void setSelectedProfile(const QString& profileId, const QString& profileName);

   private:
    QVector<QJsonObject> m_availableProfiles;
};
