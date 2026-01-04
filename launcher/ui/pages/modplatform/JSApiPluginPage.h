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

#include "modplatform/ResourceAPI.h"

#include "ui/pages/modplatform/PluginPage.h"

namespace Ui {
class ResourcePage;
}

namespace ResourceDownload {

class PluginDownloadDialog;

/* This page handles JavaScript API-based plugin downloads. */
class JSApiPluginPage : public PluginPage {
    Q_OBJECT

   public:
    static JSApiPluginPage* create(PluginDownloadDialog* dialog, BaseInstance& instance, ResourceAPI* api)
    {
        return PluginPage::create<JSApiPluginPage>(dialog, instance, api);
    }

    JSApiPluginPage(PluginDownloadDialog* dialog, BaseInstance& instance, ResourceAPI* api);
    ~JSApiPluginPage() override = default;

    QString displayName() const override { return m_apiName; }
    QString debugName() const override { return m_apiName; }
    QIcon icon() const override;
    QString id() const override { return m_apiId; }
    QString helpPage() const override { return m_apiId; }
    QString metaEntryBase() const override { return m_apiId + "Plugins"; }

    bool shouldDisplay() const override;

   private:
    QString m_apiName;
    QString m_apiId;
};

}  // namespace ResourceDownload
