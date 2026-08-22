// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QList>
#include <QString>
#include <memory>

#include "QObjectPtr.h"
#include "tasks/Task.h"

namespace Meta {
class Version;
class VersionList;

/// Abstract interface for metadata providers.
/// Each provider knows how to fetch version lists and version details
/// for a set of UIDs from a specific source (e.g., PrismLauncher meta,
/// Maven repository, GitHub releases, etc.).
class MetadataProvider {
   public:
    virtual ~MetadataProvider() = default;

    /// Return true if this provider handles the given UID.
    virtual bool handlesUid(const QString& uid) const = 0;

    /// Return all UIDs this provider can handle.
    /// Used at startup to pre-register UIDs in the metadata index,
    /// so hasUid() returns true before any version list is loaded.
    virtual QStringList knownUids() const = 0;

    /// Create a task that loads the full version list for the given UID.
    /// The task, on success, should have populated `list` with versions.
    virtual Task::Ptr loadVersionListTask(VersionList* list) const = 0;

    /// Create a task that loads metadata for a specific version.
    /// The task, on success, should have parsed the version data into `version`.
    virtual Task::Ptr loadVersionTask(Version* version) const = 0;
};

/// Register all built-in metadata providers.
/// Call once at application startup, after the metadata Index is created.
void registerBuiltinProviders();

/// Get the list of all registered providers.
const QList<std::shared_ptr<MetadataProvider>>& builtinProviders();

/// Find the provider that handles the given UID, or nullptr.
MetadataProvider* findProviderForUid(const QString& uid);

}  // namespace Meta
