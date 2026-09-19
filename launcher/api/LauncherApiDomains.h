// SPDX-License-Identifier: GPL-3.0-only

#pragma once

class LauncherApi;
class QJsonObject;
QJsonObject launcherAccountSnapshot();
QJsonObject launcherUpdateSnapshot();

/** Register read/write domains that are shared by the GUI and alternate UIs. */
void registerLauncherApiDomains(LauncherApi& api);
