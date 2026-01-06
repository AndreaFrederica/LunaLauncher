// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
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
 */

#pragma once

#include <QWidget>

#include "ui/pages/BasePage.h"

namespace Ui {
class YukariConnectPage;
}

// Forward declarations
namespace YukariConnectTypes {
struct MetaInfo;
}

enum class YukariConnectDownloadSource {
    GitHub,
    Mirror
};

class YukariConnectPage : public QWidget, public BasePage {
    Q_OBJECT

   public:
    explicit YukariConnectPage(QWidget* parent = nullptr);
    ~YukariConnectPage();

    QString displayName() const override { return tr("YukariConnect"); }
    QIcon icon() const override { return QIcon::fromTheme("proxy"); }
    QString id() const override { return "yukari-connect"; }
    QString helpPage() const override { return "yukari-connect"; }
    virtual bool apply() override;
    void retranslate() override;
    void openedImpl() override;

   private slots:
    void onDownloadButtonClicked();
    void onInstallFromFileButtonClicked();
    void onDeleteButtonClicked();
    void onOpenOnlineButtonClicked();
    void onLicenseInfoButtonClicked();
    void onAvailabilityChanged(bool available);
    void onMetaChanged(const YukariConnectTypes::MetaInfo& meta);
    void onEditConfigButtonClicked();

   private:
    void refreshStatus();
    void updateStatus();

   private:
    Ui::YukariConnectPage* ui;
};
