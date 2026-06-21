// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QString>

#include "net/Mode.h"
#include "tasks/Task.h"

namespace Meta {
class Version;
class VersionList;

namespace Cleanroom {
constexpr auto uid = "com.cleanroommc.cleanroom";

bool isUid(const QString& uid);
QString packageUrl(const QString& version);
Task::Ptr loadVersionListTask(VersionList* list, Net::Mode mode);
Task::Ptr loadVersionTask(Version* version, Net::Mode mode);
}  // namespace Cleanroom
}  // namespace Meta
