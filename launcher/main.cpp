// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
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

#include <iostream>
#include <QWidget>

#include <QMessageBox>

#include "Application.h"
#include "DeveloperMode.h"

int runThemePreview(int argc, char** argv, const QString& path);

#if defined Q_OS_WIN32
#include "console/WindowsConsole.h"
#endif

int main(int argc, char* argv[])
{
    for (int i=1; i+1<argc; ++i)
        if (QByteArray(argv[i]) == "--theme-preview") return runThemePreview(argc, argv, QString::fromLocal8Bit(argv[i+1]));
    for(int i=1;i<argc;++i) if(QByteArray(argv[i])=="--safe-theme") {
        qputenv("LUNA_SAFE_THEME","1");
        for(int j=i;j<argc;++j)argv[j]=argv[j+1];
        --argc; --i;
    }
#if defined Q_OS_WIN32
    // Attach before validating the entry point so Windows GUI builds can report
    // an actionable error to the terminal that launched them.
    console::WindowsConsoleGuard _consoleGuard;
#endif

    bool headless = false;
    for (int i = 1; i < argc; ++i) {
        const QByteArray argument(argv[i]);
        if (argument == "--cli" || argument == "--mcp" || argument == "--tui") {
            headless = true;
            break;
        }
    }
#ifdef LAUNCHER_HEADLESS_ENTRYPOINT
    if (!headless) {
        std::cerr << "lunalauncher-cli requires one of --cli, --tui, or --mcp." << std::endl;
        return 2;
    }
#elif defined Q_OS_WIN32
    if (headless) {
        std::cerr << "Headless modes on Windows require lunalauncher-cli.exe." << std::endl;
        return 2;
    }
#endif
    if (headless && qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");

    // Register resources before constructing Application. ThemeManager is
    // created from Application's constructor and scans the built-in icon
    // themes immediately. Registering these qrc bundles afterwards makes
    // the scan see empty paths, so Prism icons disappear until a later
    // refresh (and the missing built-in themes can never become valid for
    // that process).
    Q_INIT_RESOURCE(multimc);
    Q_INIT_RESOURCE(lucide);
    Q_INIT_RESOURCE(backgrounds);
    Q_INIT_RESOURCE(documents);
    Q_INIT_RESOURCE(lunalauncher);
    Q_INIT_RESOURCE(pe_dark);
    Q_INIT_RESOURCE(pe_light);
    Q_INIT_RESOURCE(pe_blue);
    Q_INIT_RESOURCE(pe_colored);
    Q_INIT_RESOURCE(breeze_dark);
    Q_INIT_RESOURCE(breeze_light);
    Q_INIT_RESOURCE(kde);
    Q_INIT_RESOURCE(OSX);
    Q_INIT_RESOURCE(iOS);
    Q_INIT_RESOURCE(flat);
    Q_INIT_RESOURCE(flat_white);
    Q_INIT_RESOURCE(shaders);

    // initialize Qt
    Application app(argc, argv);
    switch (app.status()) {
        case Application::StartingUp:
        case Application::Initialized: {
            // Native Windows decorations use the application icon as the
            // fallback for dialogs that do not provide a window-specific
            // icon. Set it after the launcher resources are registered so
            // the Luna title bar follows the same behavior.
            if (app.windowIcon().isNull())
                app.setWindowIcon(app.logo());
            // Setup dialogs may already exist by this point. Give every
            // existing top-level window the same fallback icon used by the
            // native frame.
            if (!app.windowIcon().isNull())
                for (auto* window : QApplication::topLevelWidgets())
                    if (window->windowIcon().isNull())
                        window->setWindowIcon(app.windowIcon());
            if (DeveloperMode::isEnabled() && !DeveloperMode::hasDebugMarker(DeveloperMode::Marker0)) {
                // Show developer warning dialog before allowing to proceed
                auto result = QMessageBox::warning(
                    nullptr,
                    "Developer Mode Warning",
                    "<html><head/><body>"
                       "<h2 style='color: red;'>⚠️ DEVELOPER MODE ⚠️</h2>"
                       "<p>Normally, you must add a Microsoft account that owns Minecraft before you can add an offline account.</p>"
                       "<p><b>You are about to use this launcher in developer mode without ownership verification.</b></p>"
                       "<p>By proceeding, you confirm that:</p>"
                       "<ul>"
                       "<li>You are a developer testing this launcher</li>"
                       "<li>You have already purchased a legitimate copy of Minecraft</li>"
                       "<li>This build is for testing purposes only</li>"
                       "</ul>"
                       "<p style='color: red;'>If you have not purchased Minecraft, please cancel and delete this launcher immediately.</p>"
                       "</body></html>"
                       "<br><br>"
                       "Do you wish to proceed?",
                    QMessageBox::Yes | QMessageBox::No,
                    QMessageBox::No
                );

                if (result != QMessageBox::Yes) {
                    return 0;
                }
            }

            return app.exec();
        }
        case Application::Failed:
            return 1;
        case Application::Succeeded:
            return 0;
        default:
            return -1;
    }
}
