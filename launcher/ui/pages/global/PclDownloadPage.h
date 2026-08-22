// SPDX-License-Identifier: GPL-3.0-only
/*
 *  LunaLauncher - Minecraft Launcher
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

class QCheckBox;
class QLabel;
class QSpinBox;

class PclDownloadPage : public QWidget, public BasePage {
    Q_OBJECT

   public:
    explicit PclDownloadPage(QWidget* parent = nullptr);

    QString displayName() const override { return tr("dotNetDownload"); }
    QIcon icon() const override { return QIcon::fromTheme("proxy"); }
    QString id() const override { return "dotnet-download-settings"; }
    QString helpPage() const override { return "DotNet-Download-settings"; }
    bool apply() override;
    void setEnabledFromBackend(bool enabled);

   private:
    void loadSettings();
    void applySettings();

   private:
    QCheckBox* m_enabled = nullptr;
    QCheckBox* m_fallbackToQt = nullptr;
    QSpinBox* m_threadLimit = nullptr;
    QSpinBox* m_speedLimit = nullptr;
    QLabel* m_status = nullptr;
};
