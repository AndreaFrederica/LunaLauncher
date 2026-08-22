// SPDX-License-Identifier: GPL-3.0-only

#include "JSApiModPage.h"

#include "minecraft/MinecraftInstance.h"
#include "modplatform/jsapi/JSResourceAPI.h"
#include "ui_ResourcePage.h"

namespace ResourceDownload {

JSApiModPage::JSApiModPage(ModDownloadDialog* dialog, BaseInstance& instance, JSResourceAPI* api)
    : ModPage(dialog, instance)
    , m_apiId(api->getMetadata().id)
    , m_displayName(api->getMetadata().displayName)
    , m_iconName(api->getMetadata().icon)
{
    m_model = new ModModel(instance, api, m_apiId, metaEntryBase(), false);
    m_ui->packView->setModel(m_model);

    addSortings();

    connect(m_ui->sortByBox, &QComboBox::currentIndexChanged, this, &JSApiModPage::triggerSearch);
    connect(m_ui->packView->selectionModel(), &QItemSelectionModel::currentChanged, this, &JSApiModPage::onSelectionChanged);
    connect(m_ui->versionSelectionBox, &QComboBox::currentIndexChanged, this, &JSApiModPage::onVersionSelectionChanged);
    connect(m_ui->resourceSelectionButton, &QPushButton::clicked, this, &JSApiModPage::onResourceSelected);

    m_ui->packDescription->setMetaEntry(metaEntryBase());
}

QIcon JSApiModPage::icon() const
{
    return QIcon::fromTheme(m_iconName, QIcon::fromTheme("extension"));
}

std::unique_ptr<ModFilterWidget> JSApiModPage::createFilterWidget()
{
    return ModFilterWidget::create(&static_cast<MinecraftInstance&>(m_baseInstance), true);
}

}  // namespace ResourceDownload
