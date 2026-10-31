// SPDX-License-Identifier: GPL-3.0-only
#include "LauncherResources.h"

#include "Application.h"
#include "BuildConfig.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

namespace LauncherResources {
namespace {

QString cleanPath(const QString& path)
{
    return QDir::cleanPath(QDir(path).absolutePath());
}

void appendUnique(QStringList& paths, const QString& path)
{
    if (path.isEmpty())
        return;
    const auto cleaned = cleanPath(path);
    if (!paths.contains(cleaned))
        paths.append(cleaned);
}

QString sourceRoot()
{
    const auto appDir = QDir(QCoreApplication::applicationDirPath());
    const QStringList candidates{
        appDir.filePath("../.."), // Meson/CMake build/<config>/launcher
        appDir.filePath(".."),   // a launcher binary built in the source tree
        QDir::currentPath(),
    };
    for (const auto& candidate : candidates) {
        const auto root = cleanPath(candidate);
        if (QFileInfo::exists(QDir(root).filePath("launcher/exresources")))
            return root;
        if (QFileInfo::exists(QDir(root).filePath("exresources")))
            return root;
    }
    return {};
}

} // namespace

QString userRoot()
{
    if (auto* application = qobject_cast<Application*>(QCoreApplication::instance()); application && !application->dataRoot().isEmpty())
        return cleanPath(application->dataRoot());
    return cleanPath(QDir::currentPath());
}

QStringList installRoots()
{
    QStringList roots;
    const auto appDir = QDir(QCoreApplication::applicationDirPath());

#if defined(Q_OS_WIN)
    appendUnique(roots, appDir.filePath("resource"));
#elif defined(Q_OS_MACOS)
    appendUnique(roots, appDir.filePath("../Resources"));
#else
    appendUnique(roots, appDir.filePath("../share/%1/resources").arg(BuildConfig.LAUNCHER_NAME));
    appendUnique(roots, appDir.filePath("../share/%1/resources").arg(BuildConfig.LAUNCHER_APP_BINARY_NAME));
    appendUnique(roots, QStringLiteral("/usr/share/%1/resources").arg(BuildConfig.LAUNCHER_NAME));
    appendUnique(roots, QStringLiteral("/usr/share/%1/resources").arg(BuildConfig.LAUNCHER_APP_BINARY_NAME));
    appendUnique(roots, QStringLiteral("/usr/local/share/%1/resources").arg(BuildConfig.LAUNCHER_NAME));
    appendUnique(roots, QStringLiteral("/usr/local/share/%1/resources").arg(BuildConfig.LAUNCHER_APP_BINARY_NAME));
#endif
    return roots;
}

QStringList searchPaths(const QString& relativeDirectory)
{
    QStringList paths;
    const auto appendRelative = [&paths, &relativeDirectory](const QString& root) {
        appendUnique(paths, relativeDirectory.isEmpty() ? root : QDir(root).filePath(relativeDirectory));
    };

    // User files override packaged files, matching the existing themes,
    // translations and icon behavior.
    appendRelative(userRoot());
    for (const auto& root : installRoots())
        appendRelative(root);

    // Keep source-tree resources usable for local development without making
    // the installed binary depend on a particular build directory layout.
    const auto source = sourceRoot();
    if (!source.isEmpty()) {
        const auto exresources = QFileInfo::exists(QDir(source).filePath("launcher/exresources"))
                                     ? QDir(source).filePath("launcher/exresources")
                                     : QDir(source).filePath("exresources");
        appendRelative(exresources);
    }
    return paths;
}

QString installPath(const QString& relativeDirectory)
{
    const auto user = cleanPath(userRoot());
    for (const auto& path : searchPaths(relativeDirectory)) {
        if (cleanPath(path) != cleanPath(QDir(user).filePath(relativeDirectory)) && QDir(path).exists())
            return cleanPath(path);
    }

    const auto roots = installRoots();
    if (!roots.isEmpty())
        return cleanPath(relativeDirectory.isEmpty() ? roots.front() : QDir(roots.front()).filePath(relativeDirectory));
    return {};
}

QString locateFile(const QString& relativePath)
{
    for (const auto& directory : searchPaths(QFileInfo(relativePath).path())) {
        const auto file = QDir(directory).filePath(QFileInfo(relativePath).fileName());
        if (QFileInfo(file).isFile())
            return cleanPath(file);
    }
    return {};
}

} // namespace LauncherResources
