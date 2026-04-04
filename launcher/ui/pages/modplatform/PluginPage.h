// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#pragma once

#include <QWidget>

#include "modplatform/ModIndex.h"

#include "ui/pages/modplatform/PluginModel.h"
#include "ui/pages/modplatform/ResourcePage.h"

namespace Ui {
class ResourcePage;
}

namespace ResourceDownload {

class PluginDownloadDialog;

/* This page handles most logic related to browsing and selecting plugins to download. */
class PluginPage : public ResourcePage {
    Q_OBJECT

   public:
    // Create a page without a custom API (uses internal API creation)
    template <typename T>
    static T* create(PluginDownloadDialog* dialog, BaseInstance& instance)
    {
        return create<T>(dialog, instance, nullptr);
    }

    // Create a page with a custom API (useful for JS APIs)
    template <typename T>
    static T* create(PluginDownloadDialog* dialog, BaseInstance& instance, ResourceAPI* customApi)
    {
        auto page = new T(dialog, instance, customApi);
        auto model = static_cast<PluginModel*>(page->getModel());

        connect(model, &ResourceModel::versionListUpdated, page, &ResourcePage::versionListUpdated);
        connect(model, &ResourceModel::projectInfoUpdated, page, &ResourcePage::updateUi);
        connect(model, &QAbstractListModel::modelReset, page, &ResourcePage::modelReset);

        return page;
    }

    //: The plural version of 'plugin'
    inline QString resourcesString() const override { return tr("plugins"); }
    //: The singular version of 'plugins'
    inline QString resourceString() const override { return tr("plugin"); }

    QMap<QString, QString> urlHandlers() const override;

    void addResourceToPage(ModPlatform::IndexedPack::Ptr, ModPlatform::IndexedVersion&, ResourceFolderModel*) override;

    bool supportsFiltering() const override { return false; };

   protected:
    PluginPage(PluginDownloadDialog* dialog, BaseInstance& instance);

   protected slots:
    void triggerSearch() override;
};

}  // namespace ResourceDownload
