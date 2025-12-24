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

#pragma once

#include <QDialog>

#include "minecraft/auth/MinecraftAccount.h"

namespace Ui {
class UnifiedPassLoginDialog;
}

class UnifiedPassLoginDialog : public QDialog {
    Q_OBJECT

   public:
    ~UnifiedPassLoginDialog();

    static MinecraftAccountPtr newAccount(QWidget* parent = nullptr);
    int exec() override;

   protected slots:
    void onTaskFailed(QString reason);
    void onAuthFlowStatus(QString status);
    void onAuthFlowSucceeded();
    void onAuthFlowAborted();
    void onRegisterButtonClicked();

   private:
    explicit UnifiedPassLoginDialog(QWidget* parent = nullptr);

    void startLogin();

    Ui::UnifiedPassLoginDialog* ui;
    MinecraftAccountPtr m_account;
    shared_qobject_ptr<AuthFlow> m_authflow_task;
};
