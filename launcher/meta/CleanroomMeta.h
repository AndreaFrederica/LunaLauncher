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
bool normalizeInstance(const QString& root);
Task::Ptr loadVersionListTask(VersionList* list, Net::Mode mode);
Task::Ptr loadVersionTask(Version* version, Net::Mode mode);
Task::Ptr updateInstanceTask(const QString& instanceRoot, const QString& version);
}  // namespace Cleanroom
}  // namespace Meta
