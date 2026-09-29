// SPDX-License-Identifier: GPL-3.0-only
#include "YggdrasilPage.h"

#include <QCheckBox>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

#include "Application.h"
#include "settings/SettingsObject.h"

YggdrasilPage::YggdrasilPage(QWidget* parent) : QWidget(parent)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    auto* content = new QWidget;
    auto* layout = new QVBoxLayout(content);
    auto* title = new QLabel(tr("Local Yggdrasil service"), content);
    auto titleFont = title->font();
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);

    auto* description = new QLabel(
        tr("The launcher-managed Rust service provides metadata caching, cached account profiles and local authentication when an authority is unreachable."),
        content);
    description->setWordWrap(true);
    layout->addWidget(description);

    m_metadataCache = new QCheckBox(tr("Enable metadata cache and the local service"), content);
    m_metadataCache->setToolTip(tr("Caches launcher metadata and prepares local authentication for verified Microsoft accounts."));
    layout->addWidget(m_metadataCache);

    m_accountCache = new QCheckBox(tr("Cache Microsoft and third-party Yggdrasil profiles"), content);
    m_accountCache->setToolTip(tr("Stores UUID, name, skin and cape observations without storing the provider password or access token."));
    layout->addWidget(m_accountCache);

    auto* note = new QLabel(
        tr("Cached local sessions keep the original UUID and profile data. They work with single-player and servers configured to trust this service; external official servers still perform their own verification."),
        content);
    note->setWordWrap(true);
    layout->addWidget(note);
    layout->addStretch(1);
    scrollArea->setWidget(content);
    outer->addWidget(scrollArea);

    auto* settings = APPLICATION->settings();
    m_metadataCache->setChecked(settings->get("LocalServiceCache").toBool());
    m_accountCache->setChecked(settings->get("YggdrasilAccountCache").toBool());
}

bool YggdrasilPage::apply()
{
    auto* settings = APPLICATION->settings();
    settings->set("LocalServiceCache", m_metadataCache->isChecked());
    settings->set("YggdrasilAccountCache", m_accountCache->isChecked());
    return true;
}

void YggdrasilPage::retranslate()
{
    // Text is initialized from translations when the page is created. The
    // settings dialog recreates pages after a language change.
}
