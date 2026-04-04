// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
 *  Copyright (c) 2022 Jamie Mansfield <jmansfield@cadixdev.org>
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
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *      Copyright 2013-2021 MultiMC Contributors
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#include "AccountListPage.h"
#include "ui/dialogs/skins/SkinManageDialog.h"
#include "ui_AccountListPage.h"

#include <QItemSelectionModel>
#include <QMenu>
#include <QPushButton>

#include <QDebug>

#include "ui/dialogs/ChooseOfflineNameDialog.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/MSALoginDialog.h"
#include "ui/dialogs/YggdrasilLoginDialog.h"
#include "ui/dialogs/UnifiedPassLoginDialog.h"

#include "Application.h"

// Check if developer mode is enabled (ownership check disabled)
#ifdef LAUNCHER_DISABLE_OWNERSHIP_CHECK
#define DEVELOPER_MODE_ENABLED
#endif

AccountListPage::AccountListPage(QWidget* parent) : QMainWindow(parent), ui(new Ui::AccountListPage)
{
    ui->setupUi(this);
    ui->listView->setEmptyString(
        tr("Welcome!\n"
           "If you're new here, you can select the \"Add Microsoft\" button to link your Microsoft account."));
    ui->listView->setEmptyMode(VersionListView::String);
    ui->listView->setContextMenuPolicy(Qt::CustomContextMenu);

    m_accounts = APPLICATION->accounts();

    ui->listView->setModel(m_accounts);
    ui->listView->header()->setSectionResizeMode(AccountList::VListColumns::ProfileNameColumn, QHeaderView::Stretch);
    ui->listView->header()->setSectionResizeMode(AccountList::VListColumns::TypeColumn, QHeaderView::ResizeToContents);
    ui->listView->header()->setSectionResizeMode(AccountList::VListColumns::SourceColumn, QHeaderView::ResizeToContents);
    ui->listView->header()->setSectionResizeMode(AccountList::VListColumns::StatusColumn, QHeaderView::ResizeToContents);
    ui->listView->setSelectionMode(QAbstractItemView::SingleSelection);

    // Expand the account column

    QItemSelectionModel* selectionModel = ui->listView->selectionModel();

    connect(selectionModel, &QItemSelectionModel::selectionChanged,
            [this]([[maybe_unused]] const QItemSelection& sel, [[maybe_unused]] const QItemSelection& dsel) { updateButtonStates(); });
    connect(ui->listView, &VersionListView::customContextMenuRequested, this, &AccountListPage::ShowContextMenu);
    connect(ui->listView, &VersionListView::activated, this,
            [this](const QModelIndex& index) { m_accounts->setDefaultAccount(m_accounts->at(index.row())); });

    connect(m_accounts, &AccountList::listChanged, this, &AccountListPage::listChanged);
    connect(m_accounts, &AccountList::listActivityChanged, this, &AccountListPage::listChanged);
    connect(m_accounts, &AccountList::defaultAccountChanged, this, &AccountListPage::listChanged);

    updateButtonStates();

    // Xbox authentication won't work without a client identifier, so disable the button if it is missing
    if (~APPLICATION->capabilities() & Application::SupportsMSA) {
        ui->actionAddMicrosoft->setVisible(false);
        ui->actionAddMicrosoft->setToolTip(tr("No Microsoft Authentication client ID was set."));
    }

#ifdef DEVELOPER_MODE_ENABLED
    // Show developer mode warning at the top of the list
    ui->listView->setEmptyString(
        tr("<html><head/><body>"
           "<h2 style='color: red;'>⚠️ DEVELOPER BUILD - FOR TESTING ONLY</h2>"
           "<p style='color: red;'><b>You are using a developer version with ownership checks disabled.</b></p>"
           "<p style='color: red;'>Please ensure you are a developer and have already purchased the game.</p>"
           "<p style='color: red;'>If you have not purchased Minecraft, please delete this launcher immediately.</p>"
           "</body></html>"));

    // Show the forced add offline button
    ui->actionAddOfflineForced->setVisible(true);
    ui->actionAddOfflineForced->setToolTip(tr("Developer mode: Add offline account without Microsoft account verification"));
#endif
}

AccountListPage::~AccountListPage()
{
    delete ui;
}

void AccountListPage::retranslate()
{
    ui->retranslateUi(this);
}

void AccountListPage::ShowContextMenu(const QPoint& pos)
{
    auto menu = ui->toolBar->createContextMenu(this, tr("Context menu"));
    menu->exec(ui->listView->mapToGlobal(pos));
    delete menu;
}

void AccountListPage::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
    }
    QMainWindow::changeEvent(event);
}

QMenu* AccountListPage::createPopupMenu()
{
    QMenu* filteredMenu = QMainWindow::createPopupMenu();
    filteredMenu->removeAction(ui->toolBar->toggleViewAction());
    return filteredMenu;
}

void AccountListPage::listChanged()
{
    updateButtonStates();
}

void AccountListPage::on_actionAddMicrosoft_triggered()
{
    auto account = MSALoginDialog::newAccount(this);
    if (account) {
        m_accounts->addAccount(account);
        if (m_accounts->count() == 1) {
            m_accounts->setDefaultAccount(account);
        }
    }
}

void AccountListPage::on_actionAddOffline_triggered()
{
    if (!m_accounts->anyAccountIsValid()) {
#ifdef DEVELOPER_MODE_ENABLED
        // Developer mode: Show warning with option to force add
        QMessageBox developerMsg(this);
        developerMsg.setIcon(QMessageBox::Warning);
        developerMsg.setWindowTitle(tr("Developer Mode Warning"));
        developerMsg.setTextFormat(Qt::RichText);
        developerMsg.setText(
            tr("<html><head/><body>"
               "<h2 style='color: red;'>⚠️ DEVELOPER BUILD - FOR TESTING ONLY</h2>"
               "<p style='color: red;'><b>You are using a developer version with ownership checks disabled.</b></p>"
               "<p>Normally, you must add a Microsoft account that owns Minecraft before you can add an offline account.</p>"
               "<p>By proceeding, you confirm that:</p>"
               "<ul>"
               "<li>You are a developer testing this launcher</li>"
               "<li>You have already purchased a legitimate copy of Minecraft</li>"
               "<li>This build is for testing purposes only</li>"
               "</ul>"
               "<p style='color: red;'>If you have not purchased Minecraft, please cancel and delete this launcher immediately.</p>"
               "</body></html>")
        );

        developerMsg.addButton(tr("Force Add (Developer)"), QMessageBox::ActionRole);
        QPushButton* cancelButton = developerMsg.addButton(QMessageBox::Cancel);
        developerMsg.setDefaultButton(cancelButton);
        developerMsg.exec();

        if (developerMsg.clickedButton() == cancelButton) {
            return;
        }
        // User clicked Force Add - proceed to add offline account
#else
        // Normal mode: Show error requiring Microsoft account
        QMessageBox::warning(this, tr("Error"),
                             tr("You must add a Microsoft account that owns Minecraft before you can add an offline account."
                                "<br><br>"
                                "If you have lost your account you can contact Microsoft for support."));
        return;
#endif
    }

    ChooseOfflineNameDialog dialog(tr("Please enter your desired username to add your offline account."), this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    if (const MinecraftAccountPtr account = MinecraftAccount::createOffline(dialog.getUsername())) {
        account->login()->start();  // The task will complete here.
        m_accounts->addAccount(account);
        if (m_accounts->count() == 1) {
            m_accounts->setDefaultAccount(account);
        }
    }
}

// Developer mode function - always defined for moc, but only callable when developer mode is enabled
void AccountListPage::on_actionAddOfflineForced_triggered()
{
#ifdef DEVELOPER_MODE_ENABLED
    // Show developer warning dialog before allowing to proceed
    auto result = QMessageBox::warning(
        this,
        tr("Developer Mode Warning"),
        tr("<html><head/><body>"
           "<h2 style='color: red;'>⚠️ DEVELOPER MODE</h2>"
           "<p>Normally, you must add a Microsoft account that owns Minecraft before you can add an offline account.</p>"
           "<p><b>You are about to add an offline account without verification.</b></p>"
           "<p>By proceeding, you confirm that:</p>"
           "<ul>"
           "<li>You are a developer testing this launcher</li>"
           "<li>You have already purchased a legitimate copy of Minecraft</li>"
           "<li>This build is for testing purposes only</li>"
           "</ul>"
           "<p style='color: red;'>If you have not purchased Minecraft, please cancel and delete this launcher.</p>"
           "</body></html>"
           "<br><br>"
           "Do you wish to proceed?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (result != QMessageBox::Yes) {
        return;
    }

    ChooseOfflineNameDialog dialog(tr("Please enter your desired username to add your offline account."), this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    if (const MinecraftAccountPtr account = MinecraftAccount::createOffline(dialog.getUsername())) {
        account->login()->start();  // The task will complete here.
        m_accounts->addAccount(account);
        if (m_accounts->count() == 1) {
            m_accounts->setDefaultAccount(account);
        }
    }
#else
    // This should never be called in non-developer mode since the button is hidden
    qWarning("on_actionAddOfflineForced_triggered called in non-developer mode!");
#endif
}

void AccountListPage::on_actionAddYggdrasil_triggered()
{
    auto account = YggdrasilLoginDialog::newAccount(this);
    if (account) {
        m_accounts->addAccount(account);
        if (m_accounts->count() == 1) {
            m_accounts->setDefaultAccount(account);
        }
    }
}

void AccountListPage::on_actionAddUnifiedPass_triggered()
{
    auto account = UnifiedPassLoginDialog::newAccount(this);
    if (account) {
        m_accounts->addAccount(account);
        if (m_accounts->count() == 1) {
            m_accounts->setDefaultAccount(account);
        }
    }
}

void AccountListPage::on_actionRemove_triggered()
{
    auto response = CustomMessageBox::selectable(this, tr("Remove account?"), tr("Do you really want to delete this account?"),
                                                 QMessageBox::Question, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                        ->exec();
    if (response != QMessageBox::Yes) {
        return;
    }
    QModelIndexList selection = ui->listView->selectionModel()->selectedIndexes();
    if (selection.size() > 0) {
        QModelIndex selected = selection.first();
        m_accounts->removeAccount(selected);
    }
}

void AccountListPage::on_actionRefresh_triggered()
{
    QModelIndexList selection = ui->listView->selectionModel()->selectedIndexes();
    if (selection.size() > 0) {
        QModelIndex selected = selection.first();
        MinecraftAccountPtr account = selected.data(AccountList::PointerRole).value<MinecraftAccountPtr>();
        m_accounts->requestRefresh(account->internalId());
    }
}

void AccountListPage::on_actionSetDefault_triggered()
{
    QModelIndexList selection = ui->listView->selectionModel()->selectedIndexes();
    if (selection.size() > 0) {
        QModelIndex selected = selection.first();
        MinecraftAccountPtr account = selected.data(AccountList::PointerRole).value<MinecraftAccountPtr>();
        m_accounts->setDefaultAccount(account);
    }
}

void AccountListPage::on_actionNoDefault_triggered()
{
    m_accounts->setDefaultAccount(nullptr);
}

void AccountListPage::updateButtonStates()
{
    // If there is no selection, disable buttons that require something selected.
    QModelIndexList selection = ui->listView->selectionModel()->selectedIndexes();
    bool hasSelection = !selection.empty();
    bool accountIsReady = false;
    bool accountIsOnline = false;
    bool accountCanMoveUp = false;
    bool accountCanMoveDown = false;
    if (hasSelection) {
        QModelIndex selected = selection.first();
        MinecraftAccountPtr account = selected.data(AccountList::PointerRole).value<MinecraftAccountPtr>();
        accountIsReady = !account->isActive();
        accountIsOnline = account->accountType() != AccountType::Offline;

        accountCanMoveUp = selected.row() > 0;
        int indexOfLast = m_accounts->count() - 1;
        accountCanMoveDown = selected.row() < indexOfLast;
    }
    ui->actionRemove->setEnabled(accountIsReady);
    ui->actionSetDefault->setEnabled(accountIsReady);
    ui->actionManageSkins->setEnabled(accountIsReady && accountIsOnline);
    ui->actionRefresh->setEnabled(accountIsReady && accountIsOnline);

    if (m_accounts->defaultAccount().get() == nullptr) {
        ui->actionNoDefault->setEnabled(false);
        ui->actionNoDefault->setChecked(true);
    } else {
        ui->actionNoDefault->setEnabled(true);
        ui->actionNoDefault->setChecked(false);
    }
    ui->actionMoveUp->setEnabled(accountCanMoveUp);
    ui->actionMoveDown->setEnabled(accountCanMoveDown);
    ui->listView->resizeColumnToContents(3);
}

void AccountListPage::on_actionManageSkins_triggered()
{
    QModelIndexList selection = ui->listView->selectionModel()->selectedIndexes();
    if (selection.size() > 0) {
        QModelIndex selected = selection.first();
        MinecraftAccountPtr account = selected.data(AccountList::PointerRole).value<MinecraftAccountPtr>();
        SkinManageDialog dialog(this, account);
        dialog.exec();
    }
}

void AccountListPage::on_actionMoveUp_triggered()
{
    QModelIndexList selection = ui->listView->selectionModel()->selectedIndexes();
    if (selection.size() > 0) {
        QModelIndex selected = selection.first();
        m_accounts->moveAccount(selected, -1);
    }
}

void AccountListPage::on_actionMoveDown_triggered()
{
    QModelIndexList selection = ui->listView->selectionModel()->selectedIndexes();
    if (selection.size() > 0) {
        QModelIndex selected = selection.first();
        m_accounts->moveAccount(selected, 1);
    }
}
