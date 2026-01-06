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

#include <QAbstractListModel>

#include "BaseInstance.h"

#include "modplatform/ModIndex.h"
#include "modplatform/ResourceAPI.h"

#include "ui/pages/modplatform/ResourceModel.h"

class Version;

namespace ResourceDownload {

class PluginPage;

class PluginModel : public ResourceModel {
    Q_OBJECT

   public:
    PluginModel(BaseInstance& base, ResourceAPI* api, QString debugName, QString metaEntryBase, bool api_owned = true);

    /* Ask the API for more information */
    void searchWithTerm(const QString& term, unsigned int sort, bool filter_changed);

    virtual QVariant getInstalledPackVersion(ModPlatform::IndexedPack::Ptr) const override;

    [[nodiscard]] QString debugName() const override { return m_debugName; }
    [[nodiscard]] QString metaEntryBase() const override { return m_metaEntryBase; }

   public slots:
    ResourceAPI::SearchArgs createSearchArguments() override;
    ResourceAPI::VersionSearchArgs createVersionsArguments(const QModelIndex&) override;
    ResourceAPI::ProjectInfoArgs createInfoArguments(const QModelIndex&) override;

   protected:
    virtual bool isPackInstalled(ModPlatform::IndexedPack::Ptr) const override;
    virtual bool checkVersionFilters(const ModPlatform::IndexedVersion& v) override;

   protected:
    BaseInstance& m_base_instance;

   private:
    QString m_debugName;
    QString m_metaEntryBase;
};

}  // namespace ResourceDownload
