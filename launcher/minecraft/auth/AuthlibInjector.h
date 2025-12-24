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
 * @brief Manages authlib-injector download and Yggdrasil metadata fetching
 *
 * This class handles downloading the authlib-injector.jar file if not present,
 * and fetching Yggdrasil server metadata for the prefetched configuration.
 *
 * Reference: https://authlib-injector.yushi.moe/
 */
class AuthlibInjector {
public:
    static AuthlibInjector& instance();

    /**
     * @brief Get the local path to authlib-injector.jar
     * @return Absolute path to the cached jar file
     */
    QString getLocalPath() const;

    /**
     * @brief Get the path to the metadata JSON file
     * @return Absolute path to the metadata file
     */
    QString getMetadataPath() const;

    /**
     * @brief Get the currently installed version
     * @return Version string, or empty string if not installed/version unknown
     */
    QString getVersion() const;

    /**
     * @brief Check if the cached jar file exists and is valid
     * @return true if cache is valid
     */
    bool checkCache() const;

    /**
     * @brief Download the latest version of authlib-injector.jar
     * @return Path to the downloaded jar file, or empty string on failure
     *
     * Will first try BMCLAPI mirror (recommended for Chinese users),
     * then fall back to the official source.
     */
    QString downloadLatest();

    /**
     * @brief Fetch Yggdrasil server metadata from the API
     * @param apiUrl The Yggdrasil API URL
     * @return Base64-encoded metadata, or empty string on failure
     */
    QString getYggdrasilMeta(const QString& apiUrl);

private:
    AuthlibInjector() = default;
    ~AuthlibInjector() = default;

    static AuthlibInjector* s_instance;
};
