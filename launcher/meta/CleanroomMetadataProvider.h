// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "MetadataProvider.h"

namespace Meta {

/// Metadata provider for Cleanroom loader.
/// Fetches version list from Maven XML metadata and version details
/// from the MMC zip package on Maven.
class CleanroomMetadataProvider : public MetadataProvider {
   public:
    bool handlesUid(const QString& uid) const override;
    QStringList knownUids() const override;
    Task::Ptr loadVersionListTask(VersionList* list) const override;
    Task::Ptr loadVersionTask(Version* version) const override;
};

}  // namespace Meta
