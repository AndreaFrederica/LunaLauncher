// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "ui/pages/modplatform/ModPage.h"

class JSResourceAPI;

namespace ResourceDownload {

class JSApiModPage : public ModPage {
    Q_OBJECT

   public:
    static JSApiModPage* create(ModDownloadDialog* dialog, BaseInstance& instance, JSResourceAPI* api)
    {
        return ModPage::create<JSApiModPage>(dialog, instance, api);
    }

    JSApiModPage(ModDownloadDialog* dialog, BaseInstance& instance, JSResourceAPI* api);
    ~JSApiModPage() override = default;

    QString displayName() const override { return m_displayName; }
    QString debugName() const override { return m_apiId; }
    QIcon icon() const override;
    QString id() const override { return m_apiId; }
    QString helpPage() const override { return "Mod-platform"; }
    QString metaEntryBase() const override { return m_apiId + "Mods"; }

    bool shouldDisplay() const override { return true; }
    std::unique_ptr<ModFilterWidget> createFilterWidget() override;

   private:
    QString m_apiId;
    QString m_displayName;
    QString m_iconName;
};

}  // namespace ResourceDownload
