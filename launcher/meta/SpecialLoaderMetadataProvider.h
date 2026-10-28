// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "MetadataProvider.h"

namespace Meta {

/// Metadata bridge for the official Legacy Fabric and LabyMod profiles.
class SpecialLoaderMetadataProvider final : public MetadataProvider {
   public:
    bool handlesUid(const QString& uid) const override;
    QStringList knownUids() const override;
    Task::Ptr loadVersionListTask(VersionList* list) const override;
    Task::Ptr loadVersionTask(Version* version) const override;
};

}  // namespace Meta

