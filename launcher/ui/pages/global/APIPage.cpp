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
#include <QStandardPaths>
#include <QTabBar>
#include <QValidator>
#include <QVariant>

#include "Application.h"
#include "BuildConfig.h"
#include "minecraft/MirrorDownload.h"
#include "net/PasteUpload.h"
#include "settings/SettingsObject.h"
#include "tools/BaseProfiler.h"

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

    void (QComboBox::*currentIndexChangedSignal)(int)(&QComboBox::currentIndexChanged);
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

    QString msaClientID = s->get("MSAClientIDOverride").toString();
    ui->msaClientID->setText(msaClientID);
    QString metaURL = s->get("MetaURLOverride").toString();
    ui->metaURL->setText(metaURL);
    QString resourceURL = s->get("ResourceURL").toString();
    ui->resourceURL->setText(resourceURL);
    QString libraryURL = s->get("LibrariesURL").toString();
    ui->libraryURL->setText(libraryURL);
    QString fmlLibsURL = s->get("FMLLibsURL").toString();
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
        s->set("ResourceURL", resourceURL.toString());
        s->set("LibrariesURL", libraryURL.toString());
        s->set("FMLLibsURL", fmlLibsURL.toString());
        s->set("MojangDownloadsMirrorURL", mojangDownloadsMirrorURL.toString());
    } else if (mirrorType == MirrorDownload::Official) {
        // Clear overrides to use official servers
        s->set("MetaURLOverride", "");
        s->set("ResourceURL", BuildConfig.DEFAULT_RESOURCE_BASE);
        s->set("LibrariesURL", "");
        s->set("FMLLibsURL", "");
        s->set("MojangDownloadsMirrorURL", "");
    } else if (mirrorType == MirrorDownload::BMCLAPI) {
        // Set BMCLAPI URLs for Minecraft assets only
        // BMCLAPI does NOT provide Prism Launcher's meta files, so don't override MetaURLOverride
        const auto& mirrorInfo = MirrorDownload::MirrorTypes.at(MirrorDownload::BMCLAPI);
        s->set("MetaURLOverride", "");  // Keep default for Prism Launcher meta
        s->set("ResourceURL", mirrorInfo.defaultAssetsUrl);
        s->set("LibrariesURL", mirrorInfo.defaultLibrariesUrl);
        s->set("FMLLibsURL", mirrorInfo.defaultFMLLibsUrl);
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
