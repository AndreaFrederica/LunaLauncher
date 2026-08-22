// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QString>

namespace ModApiMirror {

enum Type { Official = 0, MCIM = 1 };

QString modrinthBaseUrl();
QString curseForgeBaseUrl();

}  // namespace ModApiMirror
