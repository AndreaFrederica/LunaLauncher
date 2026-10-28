// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QString>

namespace CurseForgeExternalTool {

inline constexpr int ProtocolVersion = 1;

QString resolveExecutable(const QString& configuredPath);
bool probe(const QString& configuredPath, QString* error, bool* supportsHeadless = nullptr);

}  // namespace CurseForgeExternalTool
