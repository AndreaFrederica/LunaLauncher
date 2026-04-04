// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
*/

#include "PluginFolderPage.h"
#include "ui/dialogs/ExportToModListDialog.h"
#include "ui_ExternalResourcesPage.h"

#include <QAbstractItemModel>
#include <QAction>
#include <QEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QSortFilterProxyModel>
#include <algorithm>
#include <memory>

#include "Application.h"

#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/ResourceDownloadDialog.h"

#include "minecraft/mod/Plugin.h"
#include "minecraft/mod/PluginFolderModel.h"

#include "tasks/ConcurrentTask.h"
#include "tasks/Task.h"
#include "ui/dialogs/ProgressDialog.h"

#include "server/ServerInstance.h"

PluginFolderPage::PluginFolderPage(BaseInstance* inst, std::shared_ptr<PluginFolderModel> model, QWidget* parent)
    : ExternalResourcesPage(inst, model.get(), parent), m_model(model)
{
    setFilter(tr("Plugins (*.jar)"));

    ui->actionDownloadItem->setText(tr("Download Plugins"));
    ui->actionDownloadItem->setToolTip(tr("Download plugins from Hangar and other platforms"));
    ui->actionDownloadItem->setEnabled(true);
    ui->actionsToolbar->insertActionBefore(ui->actionAddItem, ui->actionDownloadItem);

    connect(ui->actionDownloadItem, &QAction::triggered, this, &PluginFolderPage::downloadPlugins);

    ui->actionViewHomepage->setToolTip(tr("View the homepages of all selected plugins."));

    ui->actionExportMetadata->setToolTip(tr("Export plugin's metadata to text."));
    connect(ui->actionExportMetadata, &QAction::triggered, this, &PluginFolderPage::exportPluginMetadata);
    ui->actionsToolbar->insertActionAfter(ui->actionViewHomepage, ui->actionExportMetadata);

    ui->actionsToolbar->insertActionAfter(ui->actionViewFolder, ui->actionViewConfigs);

    ui->actionResetItemMetadata->setToolTip(tr("Reset plugin metadata."));
    connect(ui->actionResetItemMetadata, &QAction::triggered, this, &PluginFolderPage::deletePluginMetadata);
}

bool PluginFolderPage::shouldDisplay() const
{
    // Only show for server instances
    return m_instance->traits().contains("server");
}

void PluginFolderPage::updateFrame(const QModelIndex& current, [[maybe_unused]] const QModelIndex& previous)
{
    if (!current.isValid()) {
        return;
    }

    auto sourceCurrent = m_filterModel->mapToSource(current);
    if (!sourceCurrent.isValid()) {
        return;
    }

    int row = sourceCurrent.row();
    auto plugin = m_model->pluginAt(row);
    if (!plugin) {
        return;
    }

    // 更新信息面板
    ui->frame->updateWithPlugin(*plugin);
}

void PluginFolderPage::removeItems(const QItemSelection& selection)
{
    if (m_instance != nullptr && m_instance->isRunning()) {
        auto response = CustomMessageBox::selectable(this, tr("Confirm Delete"),
                                                     tr("If you remove plugins while the server is running it may crash.\n"
                                                        "Are you sure you want to do this?"),
                                                     QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                            ->exec();

        if (response != QMessageBox::Yes)
            return;
    }
    m_model->deleteResources(selection.indexes());
}

void PluginFolderPage::downloadPlugins()
{
    // Check if it's a server instance
    if (!m_instance->traits().contains("server")) {
        return;
    }

    auto serverInst = dynamic_cast<ServerInstance*>(m_instance);
    if (!serverInst) {
        return;
    }

    // Check if plugin loaders are configured
    auto loaders = serverInst->getPluginLoaderTypes();
    if (loaders == 0) {
        auto response = QMessageBox::warning(
            this,
            tr("Plugin Loader Not Configured"),
            tr("You need to configure the plugin loader type before downloading plugins.\n"
               "Would you like to open the Mod Loader configuration page?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes
        );

        if (response == QMessageBox::Yes) {
            // TODO: Switch to ModLoader page
            QMessageBox::information(this, tr("Info"),
                tr("Please go to the 'Mod Loader' page in the settings to configure your server's plugin loader."));
        }
        return;
    }

    // Create download dialog for plugins
    m_downloadDialog = new ResourceDownload::PluginDownloadDialog(this, m_model, m_instance);
    connect(this, &QObject::destroyed, m_downloadDialog, &QDialog::close);
    connect(m_downloadDialog, &QDialog::finished, this, &PluginFolderPage::downloadDialogFinished);

    m_downloadDialog->open();
}

void PluginFolderPage::downloadDialogFinished(int result)
{
    if (result) {
        auto tasks = new ConcurrentTask(tr("Download Plugins"), APPLICATION->settings()->get("NumberOfConcurrentDownloads").toInt());
        connect(tasks, &Task::failed, [this, tasks](QString reason) {
            CustomMessageBox::selectable(this, tr("Error"), reason, QMessageBox::Critical)->show();
            tasks->deleteLater();
        });
        connect(tasks, &Task::aborted, [this, tasks]() {
            CustomMessageBox::selectable(this, tr("Aborted"), tr("Download stopped by user."), QMessageBox::Information)->show();
            tasks->deleteLater();
        });
        connect(tasks, &Task::succeeded, [this, tasks]() {
            QStringList warnings = tasks->warnings();
            if (warnings.count())
                CustomMessageBox::selectable(this, tr("Warnings"), warnings.join('\n'), QMessageBox::Warning)->show();

            tasks->deleteLater();
        });

        if (m_downloadDialog) {
            for (auto& task : m_downloadDialog->getTasks()) {
                tasks->addTask(task);
            }
        } else {
            qWarning() << "PluginDownloadDialog vanished before we could collect tasks!";
        }

        ProgressDialog loadDialog(this);
        loadDialog.setSkipButton(true, tr("Abort"));
        loadDialog.execWithTask(tasks);

        m_model->update();
    }
    if (m_downloadDialog)
        m_downloadDialog->deleteLater();
}

void PluginFolderPage::deletePluginMetadata()
{
    auto selection = m_filterModel->mapSelectionToSource(ui->treeView->selectionModel()->selection()).indexes();
    m_model->deleteMetadata(selection);
}

void PluginFolderPage::exportPluginMetadata()
{
    CustomMessageBox::selectable(this, tr("Not supported"), tr("Exporting plugin metadata is not supported yet."), QMessageBox::Information)
        ->show();
}
