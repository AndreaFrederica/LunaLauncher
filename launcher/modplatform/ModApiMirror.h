// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QString>

class SettingsObject;

namespace ModApiMirror {

enum Type { Official = 0, MCIM = 1, Custom = 2 };

QString modrinthBaseUrl(SettingsObject* settings = nullptr);
QString curseForgeBaseUrl(SettingsObject* settings = nullptr);

}  // namespace ModApiMirror
