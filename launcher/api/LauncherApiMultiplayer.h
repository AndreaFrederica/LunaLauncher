// SPDX-License-Identifier: GPL-3.0-only

#pragma once

class LauncherApi;

/** Register the two P2P multiplayer backends without exposing their UI pages. */
void registerLauncherApiMultiplayerOperations(LauncherApi& api);
