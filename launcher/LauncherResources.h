// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QStringList>

/**
 * Resolves files shipped beside the launcher and files overridden in the
 * current launcher data directory.
 *
 * Install layouts differ between Windows, macOS bundles and Unix prefixes.
 * Keep those rules here instead of duplicating applicationDirPath()/../share
 * calculations in each resource consumer.
 */
namespace LauncherResources {

/// The resolved per-user launcher data directory.
QString userRoot();

/// Candidate roots containing files installed with the launcher.
QStringList installRoots();

/// Candidate roots in lookup order: user override, install, development tree.
QStringList searchPaths(const QString& relativeDirectory = {});

/// Return the first existing installed directory, or the first candidate.
QString installPath(const QString& relativeDirectory = {});

/// Return the first existing file using the user-over-install precedence.
QString locateFile(const QString& relativePath);

} // namespace LauncherResources
