// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
 *  Copyright (c) 2022 Jamie Mansfield <jmansfield@cadixdev.org>
 *  Copyright (c) 2022 Lenny McLennington <lenny@sneed.church>
 *  Copyright (C) 2023 TheKodeToad <TheKodeToad@proton.me>
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

#include "APIPage.h"
#include "ui_APIPage.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QTabBar>
#include <QValidator>
#include <QVariant>

#include "Application.h"
#include "BuildConfig.h"
#include "minecraft/MirrorDownload.h"
#include "modplatform/ModApiMirror.h"
#include "modplatform/flame/CurseForgeDownloadPageService.h"
#include "net/PasteUpload.h"
#include "net/PclDownloadLibrary.h"
#include "settings/SettingsObject.h"
#include "tools/BaseProfiler.h"
#include "ui/pages/global/PclDownloadPage.h"

APIPage::APIPage(QWidget* parent) : QWidget(parent), ui(new Ui::APIPage)
{
    // This is here so you can reorder the entries in the combobox without messing stuff up
    int comboBoxEntries[] = { PasteUpload::PasteType::Mclogs, PasteUpload::PasteType::NullPointer, PasteUpload::PasteType::PasteGG,
                              PasteUpload::PasteType::Hastebin };

    static const QRegularExpression s_validUrlRegExp("https?://.+");
    static const QRegularExpression s_validMSAClientID(
        QRegularExpression::anchoredPattern("[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}"));
    static const QRegularExpression s_validFlameKey(QRegularExpression::anchoredPattern("\\$2[ayb]\\$.{56}"));

    ui->setupUi(this);

    for (auto pasteType : comboBoxEntries) {
        ui->pasteTypeComboBox->addItem(PasteUpload::PasteTypes.at(pasteType).name, pasteType);
    }

    // Add mirror type dropdown
    int mirrorTypes[] = { MirrorDownload::Official, MirrorDownload::BMCLAPI, MirrorDownload::Custom };
    for (auto mirrorType : mirrorTypes) {
        ui->mirrorTypeComboBox->addItem(tr(MirrorDownload::MirrorTypes.at(mirrorType).name.toUtf8()), mirrorType);
    }

    for (auto* comboBox : { ui->modrinthMirrorComboBox, ui->curseForgeMirrorComboBox }) {
        comboBox->addItem(tr("Official"), ModApiMirror::Official);
        comboBox->addItem(tr("MCIM"), ModApiMirror::MCIM);
    }

    ui->curseForgeDownloadBrowserComboBox->addItem(tr("Embedded browser"), "Embedded");
    ui->curseForgeDownloadBrowserComboBox->addItem(tr("System browser"), "System");
    ui->curseForgeDownloadBrowserComboBox->addItem(tr("External tool"), "External");
    if (!CurseForgeDownloadPageService::isAvailable()) {
        const auto embeddedIndex = ui->curseForgeDownloadBrowserComboBox->findData("Embedded");
        ui->curseForgeDownloadBrowserComboBox->setItemData(embeddedIndex, false, Qt::UserRole - 1);
        ui->curseForgeDownloadBrowserComboBox->setItemData(
            embeddedIndex, tr("The embedded CurseForge browser was not built or is unavailable."), Qt::ToolTipRole);
    }
    if (!CurseForgeDownloadPageService::isAvailable(CurseForgeDownloadPageService::Provider::External)) {
        const auto externalIndex = ui->curseForgeDownloadBrowserComboBox->findData("External");
        ui->curseForgeDownloadBrowserComboBox->setItemData(
            externalIndex, tr("Configure and enable the CurseForge external download tool on the Tools settings page."), Qt::ToolTipRole);
    }

    // Add download backend dropdown
    ui->downloadBackendComboBox->addItem(tr("Qt (Built-in)"), 0);
    ui->downloadBackendComboBox->addItem(tr("aria2"), 1);
    ui->downloadBackendComboBox->addItem(tr("dotNetDownload"), 2);

    // Loading also initializes the NativeAOT engine. Merely checking isLoaded()
    // would leave this option disabled until another page happened to load it.
    auto& pclDownload = PclDownloadLibrary::instance();
    if (!BuildConfig.PCL_DOWNLOAD_ENABLED || (!pclDownload.isLoaded() && !pclDownload.load())) {
        ui->downloadBackendComboBox->setItemData(2, false, Qt::UserRole - 1);
        ui->downloadBackendComboBox->setItemData(
            2, tr("PCL.Download library was not built or is unavailable: %1").arg(pclDownload.errorString()), Qt::ToolTipRole);
    }

    void (QComboBox::*currentIndexChangedSignal)(int)(&QComboBox::currentIndexChanged);
    connect(ui->downloadBackendComboBox, currentIndexChangedSignal, this, [this](int index) {
        if (!m_container)
            return;
        auto* pclPage = dynamic_cast<PclDownloadPage*>(m_container->getPage("dotnet-download-settings"));
        if (pclPage) {
            pclPage->setEnabledFromBackend(ui->downloadBackendComboBox->itemData(index).toInt() == 2);
        }
    });
    connect(ui->pasteTypeComboBox, currentIndexChangedSignal, this, &APIPage::updateBaseURLPlaceholder);
    // This function needs to be called even when the ComboBox's index is still in its default state.
    updateBaseURLPlaceholder(ui->pasteTypeComboBox->currentIndex());
    // NOTE: this allows http://, but we replace that with https later anyway
    ui->metaURL->setValidator(new QRegularExpressionValidator(s_validUrlRegExp, ui->metaURL));
    ui->resourceURL->setValidator(new QRegularExpressionValidator(s_validUrlRegExp, ui->resourceURL));
    ui->libraryURL->setValidator(new QRegularExpressionValidator(s_validUrlRegExp, ui->libraryURL));
    ui->fmlLibsURL->setValidator(new QRegularExpressionValidator(s_validUrlRegExp, ui->fmlLibsURL));
    ui->mojangDownloadsMirrorURL->setValidator(new QRegularExpressionValidator(s_validUrlRegExp, ui->mojangDownloadsMirrorURL));
    ui->baseURLEntry->setValidator(new QRegularExpressionValidator(s_validUrlRegExp, ui->baseURLEntry));
    ui->msaClientID->setValidator(new QRegularExpressionValidator(s_validMSAClientID, ui->msaClientID));
    ui->flameKey->setValidator(new QRegularExpressionValidator(s_validFlameKey, ui->flameKey));

    ui->metaURL->setPlaceholderText(BuildConfig.META_URL);
    ui->resourceURL->setPlaceholderText(BuildConfig.DEFAULT_RESOURCE_BASE);
    ui->libraryURL->setPlaceholderText(BuildConfig.LIBRARY_BASE);
    ui->fmlLibsURL->setPlaceholderText(BuildConfig.LEGACY_FMLLIBS_BASE_URL);
    ui->userAgentLineEdit->setPlaceholderText(BuildConfig.USER_AGENT);

    // Connect mirror selection change
    connect(ui->mirrorTypeComboBox, currentIndexChangedSignal, this, &APIPage::updateMirrorSelection);

    loadSettings();
    updateMirrorSelection();  // Initialize UI state

    resetBaseURLNote();
    connect(ui->pasteTypeComboBox, currentIndexChangedSignal, this, &APIPage::updateBaseURLNote);
    connect(ui->baseURLEntry, &QLineEdit::textEdited, this, &APIPage::resetBaseURLNote);
}

APIPage::~APIPage()
{
    delete ui;
}

void APIPage::setPclDownloadEnabled(bool enabled)
{
    const int backend = enabled ? 2 : 0;
    if (!enabled && ui->downloadBackendComboBox->currentData().toInt() != 2)
        return;

    const QSignalBlocker blocker(ui->downloadBackendComboBox);
    ui->downloadBackendComboBox->setCurrentIndex(ui->downloadBackendComboBox->findData(backend));
}

void APIPage::resetBaseURLNote()
{
    ui->baseURLNote->hide();
    baseURLPasteType = ui->pasteTypeComboBox->currentIndex();
}

void APIPage::updateBaseURLNote(int index)
{
    if (baseURLPasteType == index) {
        ui->baseURLNote->hide();
    } else if (!ui->baseURLEntry->text().isEmpty()) {
        ui->baseURLNote->show();
    }
}

void APIPage::updateBaseURLPlaceholder(int index)
{
    int pasteType = ui->pasteTypeComboBox->itemData(index).toInt();
    QString pasteDefaultURL = PasteUpload::PasteTypes.at(pasteType).defaultBase;
    ui->baseURLEntry->setPlaceholderText(pasteDefaultURL);
}

void APIPage::updateMirrorSelection()
{
    int mirrorType = ui->mirrorTypeComboBox->currentData().toInt();
    bool isCustom = (mirrorType == MirrorDownload::Custom);

    // Enable/disable individual URL fields based on selection
    ui->metaURL->setEnabled(isCustom);
    ui->resourceURL->setEnabled(isCustom);
    ui->libraryURL->setEnabled(isCustom);
    ui->fmlLibsURL->setEnabled(isCustom);
    ui->mojangDownloadsMirrorURL->setEnabled(isCustom);

    if (!isCustom) {
        // Populate fields with preset values (for display only)
        const auto& mirrorInfo = MirrorDownload::MirrorTypes.at(mirrorType);

        if (mirrorType == MirrorDownload::Official) {
            ui->metaURL->setText("");
            ui->metaURL->setPlaceholderText(BuildConfig.META_URL);
            ui->resourceURL->setText("");
            ui->resourceURL->setPlaceholderText(BuildConfig.DEFAULT_RESOURCE_BASE);
            ui->libraryURL->setText("");
            ui->libraryURL->setPlaceholderText(BuildConfig.LIBRARY_BASE);
            ui->fmlLibsURL->setText("");
            ui->fmlLibsURL->setPlaceholderText(BuildConfig.LEGACY_FMLLIBS_BASE_URL);
            ui->mojangDownloadsMirrorURL->setText("");
            ui->mojangDownloadsMirrorURL->setPlaceholderText(tr("Use Default"));
        } else if (mirrorType == MirrorDownload::BMCLAPI) {
            // BMCLAPI does not provide Prism Launcher's meta files, only Minecraft assets
            // So metaURL should remain empty (uses default)
            ui->metaURL->setText("");
            ui->metaURL->setPlaceholderText(BuildConfig.META_URL);
            ui->resourceURL->setText(mirrorInfo.defaultAssetsUrl);
            ui->libraryURL->setText(mirrorInfo.defaultLibrariesUrl);
            ui->fmlLibsURL->setText("");
            ui->fmlLibsURL->setPlaceholderText(BuildConfig.LEGACY_FMLLIBS_BASE_URL);
            ui->mojangDownloadsMirrorURL->setText("");
            ui->mojangDownloadsMirrorURL->setPlaceholderText(tr("BMCLAPI (auto)"));
        }
    }
}

void APIPage::loadSettings()
{
    auto s = APPLICATION->settings();

    int pasteType = s->get("PastebinType").toInt();
    QString pastebinURL = s->get("PastebinCustomAPIBase").toString();

    ui->baseURLEntry->setText(pastebinURL);
    int pasteTypeIndex = ui->pasteTypeComboBox->findData(pasteType);
    if (pasteTypeIndex == -1) {
        pasteTypeIndex = ui->pasteTypeComboBox->findData(PasteUpload::PasteType::Mclogs);
        ui->baseURLEntry->clear();
    }

    ui->pasteTypeComboBox->setCurrentIndex(pasteTypeIndex);

    // Load mirror type
    int mirrorType = s->get("DownloadMirrorType").toInt();
    int mirrorTypeIndex = ui->mirrorTypeComboBox->findData(mirrorType);
    if (mirrorTypeIndex == -1) {
        mirrorTypeIndex = ui->mirrorTypeComboBox->findData(MirrorDownload::Official);
    }
    ui->mirrorTypeComboBox->setCurrentIndex(mirrorTypeIndex);

    const auto loadModApiMirror = [](QComboBox* comboBox, int mirror) {
        int index = comboBox->findData(mirror);
        if (index == -1) {
            index = comboBox->findData(ModApiMirror::Official);
        }
        comboBox->setCurrentIndex(index);
    };
    loadModApiMirror(ui->modrinthMirrorComboBox, s->get("ModrinthMirror").toInt());
    loadModApiMirror(ui->curseForgeMirrorComboBox, s->get("CurseForgeMirror").toInt());

    const auto curseForgeDownloadBrowser = s->get("CurseForgeDownloadBrowser").toString();
    auto curseForgeDownloadBrowserIndex = ui->curseForgeDownloadBrowserComboBox->findData(curseForgeDownloadBrowser);
    if (curseForgeDownloadBrowserIndex < 0 || (curseForgeDownloadBrowser == "Embedded" && !CurseForgeDownloadPageService::isAvailable())) {
        curseForgeDownloadBrowserIndex = ui->curseForgeDownloadBrowserComboBox->findData("System");
    }
    ui->curseForgeDownloadBrowserComboBox->setCurrentIndex(curseForgeDownloadBrowserIndex);

    // Load download backend
    int backend = s->get("DownloadBackend").toInt();
    int backendIndex = ui->downloadBackendComboBox->findData(backend);
    if (backendIndex == -1) {
        backendIndex = 0;  // Default to Qt
    }
    ui->downloadBackendComboBox->setCurrentIndex(backendIndex);

    QString msaClientID = s->get("MSAClientIDOverride").toString();
    ui->msaClientID->setText(msaClientID);
    QString metaURL = s->get("MetaURLOverride").toString();
    ui->metaURL->setText(metaURL);
    QString resourceURL = s->get("ResourceURLOverride").toString();
    ui->resourceURL->setText(resourceURL);
    QString libraryURL = s->get("LibrariesURL").toString();
    ui->libraryURL->setText(libraryURL);
    QString fmlLibsURL = s->get("LegacyFMLLibsURLOverride").toString();
    ui->fmlLibsURL->setText(fmlLibsURL);
    QString mojangDownloadsMirrorURL = s->get("MojangDownloadsMirrorURL").toString();
    ui->mojangDownloadsMirrorURL->setText(mojangDownloadsMirrorURL);
    QString flameKey = s->get("FlameKeyOverride").toString();
    ui->flameKey->setText(flameKey);
    QString modrinthToken = s->get("ModrinthToken").toString();
    ui->modrinthToken->setText(modrinthToken);
    QString customUserAgent = s->get("UserAgentOverride").toString();
    ui->userAgentLineEdit->setText(customUserAgent);
    ui->technicClientID->setText(s->get("TechnicClientID").toString());
}

void APIPage::applySettings()
{
    auto s = APPLICATION->settings();

    s->set("PastebinType", ui->pasteTypeComboBox->currentData().toInt());
    s->set("PastebinCustomAPIBase", ui->baseURLEntry->text());

    // Save mirror type and URLs based on selection
    int mirrorType = ui->mirrorTypeComboBox->currentData().toInt();
    s->set("DownloadMirrorType", mirrorType);
    s->set("ModrinthMirror", ui->modrinthMirrorComboBox->currentData().toInt());
    s->set("CurseForgeMirror", ui->curseForgeMirrorComboBox->currentData().toInt());
    s->set("CurseForgeDownloadBrowser", ui->curseForgeDownloadBrowserComboBox->currentData().toString());

    // Save download backend
    s->set("DownloadBackend", ui->downloadBackendComboBox->currentData().toInt());

    if (mirrorType == MirrorDownload::Custom) {
        // Save custom URLs
        QUrl metaURL(ui->metaURL->text());
        QUrl resourceURL(ui->resourceURL->text());
        QUrl libraryURL(ui->libraryURL->text());
        QUrl fmlLibsURL(ui->fmlLibsURL->text());
        QUrl mojangDownloadsMirrorURL(ui->mojangDownloadsMirrorURL->text());

        // Add required trailing slash
        if (!metaURL.isEmpty() && !metaURL.path().endsWith('/')) {
            QString path = metaURL.path();
            path.append('/');
            metaURL.setPath(path);
        }

        if (!resourceURL.isEmpty() && !resourceURL.path().endsWith('/')) {
            QString path = resourceURL.path();
            path.append('/');
            resourceURL.setPath(path);
        }

        if (!libraryURL.isEmpty() && !libraryURL.path().endsWith('/')) {
            QString path = libraryURL.path();
            path.append('/');
            libraryURL.setPath(path);
        }

        if (!fmlLibsURL.isEmpty() && !fmlLibsURL.path().endsWith('/')) {
            QString path = fmlLibsURL.path();
            path.append('/');
            fmlLibsURL.setPath(path);
        }

        // Mojang Downloads Mirror URL doesn't need trailing slash (it's a base URL for path replacement)

        auto isLocalhost = [](const QUrl& url) { return url.host() == "localhost" || url.host() == "127.0.0.1" || url.host() == "::1"; };
        auto isUnsafe = [isLocalhost](const QUrl& url) { return !url.isEmpty() && url.scheme() == "http" && !isLocalhost(url); };

        // Don't allow HTTP, since meta is basically RCE with all the jar files.
        if (isUnsafe(metaURL)) {
            metaURL.setScheme("https");
        }

        // Also don't allow HTTP
        if (isUnsafe(resourceURL)) {
            resourceURL.setScheme("https");
        }

        if (isUnsafe(libraryURL)) {
            libraryURL.setScheme("https");
        }

        if (isUnsafe(fmlLibsURL)) {
            fmlLibsURL.setScheme("https");
        }

        if (isUnsafe(mojangDownloadsMirrorURL)) {
            mojangDownloadsMirrorURL.setScheme("https");
        }

        s->set("MetaURLOverride", metaURL.toString());
        s->set("ResourceURLOverride", resourceURL.toString());
        s->set("LibrariesURL", libraryURL.toString());
        s->set("LegacyFMLLibsURLOverride", fmlLibsURL.toString());
        s->set("MojangDownloadsMirrorURL", mojangDownloadsMirrorURL.toString());
    } else if (mirrorType == MirrorDownload::Official) {
        // Clear overrides to use official servers
        s->set("MetaURLOverride", "");
        s->set("ResourceURLOverride", "");
        s->set("LibrariesURL", "");
        s->set("LegacyFMLLibsURLOverride", "");
        s->set("MojangDownloadsMirrorURL", "");
    } else if (mirrorType == MirrorDownload::BMCLAPI) {
        // Set BMCLAPI URLs for Minecraft assets only
        // BMCLAPI does NOT provide Prism Launcher's meta files, so don't override MetaURLOverride
        const auto& mirrorInfo = MirrorDownload::MirrorTypes.at(MirrorDownload::BMCLAPI);
        s->set("MetaURLOverride", "");  // Keep default for Prism Launcher meta
        s->set("ResourceURLOverride", mirrorInfo.defaultAssetsUrl);
        s->set("LibrariesURL", mirrorInfo.defaultLibrariesUrl);
        s->set("LegacyFMLLibsURLOverride", mirrorInfo.defaultFMLLibsUrl);
        s->set("MojangDownloadsMirrorURL", "");  // BMCLAPI URL is handled in code, not stored
    }

    // Save other settings (independent of mirror mode)
    QString msaClientID = ui->msaClientID->text();
    s->set("MSAClientIDOverride", msaClientID);
    QString flameKey = ui->flameKey->text();
    s->set("FlameKeyOverride", flameKey);
    QString modrinthToken = ui->modrinthToken->text();
    s->set("ModrinthToken", modrinthToken);
    s->set("UserAgentOverride", ui->userAgentLineEdit->text());
    s->set("TechnicClientID", ui->technicClientID->text());
}

bool APIPage::apply()
{
    applySettings();
    return true;
}

void APIPage::retranslate()
{
    ui->retranslateUi(this);
}
