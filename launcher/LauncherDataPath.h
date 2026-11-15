// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace LauncherDataPath {

// Shared by the native launcher, the Neo UI startup-context query, and updater.
// Explicit --dir and environment overrides take precedence at the call sites.
inline bool configuredDirectory(const QString& root, QString& directory, QString& error)
{
    QFile file(QDir(root).filePath(QStringLiteral(".luna-data-dir")));
    if (!QFileInfo::exists(file.fileName()))
        return true;
    if (!file.open(QIODevice::ReadOnly) || file.size() > 65536) {
        error = QStringLiteral("Could not read the configured data directory: %1").arg(file.fileName());
        return false;
    }
    directory = QString::fromUtf8(file.readAll()).trimmed();
    if (directory.isEmpty() || !QDir::isAbsolutePath(directory) || directory.contains('\n') || directory.contains('\r')) {
        error = QStringLiteral("The configured data directory must be an absolute folder path: %1").arg(file.fileName());
        return false;
    }
    directory = QDir::cleanPath(directory);
    return true;
}

}  // namespace LauncherDataPath
