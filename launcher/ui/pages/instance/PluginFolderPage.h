// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
*/

#pragma once

#include <QPointer>
#include "ExternalResourcesPage.h"
#include "ui/dialogs/ResourceDownloadDialog.h"

class PluginFolderModel;

class PluginFolderPage : public ExternalResourcesPage {
    Q_OBJECT

   public:
    explicit PluginFolderPage(BaseInstance* inst, std::shared_ptr<PluginFolderModel> model, QWidget* parent = nullptr);
    virtual ~PluginFolderPage() = default;

    void setFilter(const QString& filter) { m_fileSelectionFilter = filter; }

    virtual QString displayName() const override { return tr("Plugins"); }
    virtual QIcon icon() const override { return QIcon::fromTheme("loadermods"); }
    virtual QString id() const override { return "plugins"; }
    virtual QString helpPage() const override { return "Server-plugins"; }

    virtual bool shouldDisplay() const override;

   public slots:
    void updateFrame(const QModelIndex& current, const QModelIndex& previous) override;

   private slots:
    void removeItems(const QItemSelection& selection) override;

    void downloadPlugins();
    void downloadDialogFinished(int result);
    void deletePluginMetadata();
    void exportPluginMetadata();

   protected:
    std::shared_ptr<PluginFolderModel> m_model;
    QPointer<ResourceDownload::PluginDownloadDialog> m_downloadDialog;
};
