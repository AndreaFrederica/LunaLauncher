// SPDX-License-Identifier: GPL-3.0-only

#pragma once

class LauncherApi;

/** Register server-instance operations without exposing server pages or widgets. */
void registerLauncherApiServerOperations(LauncherApi& api);
