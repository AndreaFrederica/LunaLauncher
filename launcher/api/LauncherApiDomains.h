// SPDX-License-Identifier: GPL-3.0-only

#pragma once

class LauncherApi;

/** Register read/write domains that are shared by the GUI and alternate UIs. */
void registerLauncherApiDomains(LauncherApi& api);
