// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2026 Octol1ttle <l1ttleofficial@outlook.com>
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

#include "EnsureOfflineLibraries.h"

#include "minecraft/PackProfile.h"

EnsureOfflineLibraries::EnsureOfflineLibraries(LaunchTask* parent, MinecraftInstance* instance) : LaunchStep(parent), m_instance(instance)
{}

void EnsureOfflineLibraries::executeTask()
{
    QString clientError;
    if (!m_instance->validateClientArtifact(&clientError)) {
        emitFailed(clientError);
        return;
    }
    const auto profile = m_instance->getPackProfile()->getProfile();
    QStringList allJars;
    profile->getLibraryFiles(m_instance->runtimeContext(), allJars, allJars, m_instance->getLocalLibraryPath(), m_instance->binRoot(),
                             false);
    // External vanilla instances launch the client in versions/, not the
    // synthesized MMC cache path. Validate the same route as getClassPath().
    if (const auto route = m_instance->clientArtifactRoute(); route.has_value())
        allJars = route->applyToClassPath(allJars);

    QStringList missing;
    for (const auto& jar : allJars) {
        if (!QFileInfo::exists(jar)) {
            missing.append(jar);
        }
    }

    if (missing.isEmpty()) {
        emitSucceeded();
        return;
    }

    emit logLine("Missing libraries:", MessageLevel::Error);
    for (const auto& jar : missing) {
        emit logLine("  " + jar, MessageLevel::Error);
    }
    emit logLine(tr("\nThis instance cannot be launched because some libraries are missing or have not been downloaded yet. Please "
                    "try again in online mode with a working Internet connection"),
                 MessageLevel::Fatal);
    emitFailed("Required libraries are missing");
}
