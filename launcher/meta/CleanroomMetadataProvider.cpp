// SPDX-License-Identifier: GPL-3.0-only

#include "CleanroomMetadataProvider.h"

#include "CleanroomMeta.h"
#include "meta/Version.h"
#include "meta/VersionList.h"
#include "net/Mode.h"

namespace Meta {

bool CleanroomMetadataProvider::handlesUid(const QString& uid) const
{
    return Cleanroom::isUid(uid);
}

QStringList CleanroomMetadataProvider::knownUids() const
{
    return { QLatin1String(Cleanroom::uid) };
}

Task::Ptr CleanroomMetadataProvider::loadVersionListTask(VersionList* list) const
{
    return Cleanroom::loadVersionListTask(list, Net::Mode::Online);
}

Task::Ptr CleanroomMetadataProvider::loadVersionTask(Version* version) const
{
    return Cleanroom::loadVersionTask(version, Net::Mode::Online);
}

}  // namespace Meta
