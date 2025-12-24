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

#include <QString>

#include "FileSystem.h"

/**
 * @brief Manages nide8auth.jar download for UnifiedPass authentication
 *
 * This class handles downloading the nide8auth.jar file required for
 * UnifiedPass (Nide8) authentication to work with Minecraft.
 *
 * Reference: https://login.mc-user.com:233/
 */
class Nide8Auth {
public:
    static Nide8Auth& instance();

    /**
     * @brief Get the local path to nide8auth.jar
     * @return Absolute path to the cached jar file
     */
    QString getLocalPath() const;

    /**
     * @brief Check if the cached jar file exists and is valid
     * @return true if cache is valid
     */
    bool checkCache() const;

    /**
     * @brief Download nide8auth.jar
     * @return Path to the downloaded jar file, or empty string on failure
     *
     * Downloads from the official Nide8 source.
     */
    QString download();

private:
    Nide8Auth() = default;
    ~Nide8Auth() = default;

    static Nide8Auth* s_instance;
};
