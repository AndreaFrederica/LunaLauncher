// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "TerracottaReleaseUtils.h"

#include <QDirIterator>
#include <QFileInfo>
#include <QJsonObject>

namespace TerracottaReleaseUtils {

static QString osToken(Os os)
{
    switch (os) {
        case Os::Windows:
            return "windows";
        case Os::MacOS:
            return "macos";
        case Os::Linux:
            return "linux";
    }
    return {};
}

static QStringList archPreference(Os os)
{
    switch (os) {
        case Os::Windows:
            return {"x86_64", "arm64"};
        case Os::MacOS:
            return {"arm64", "x86_64"};
        case Os::Linux:
            return {"x86_64", "arm64", "loongarch64", "riscv64"};
    }
    return {};
}

QString pickDownloadUrl(const QJsonArray& assets, Os os)
{
    const QString token = osToken(os);
    const QStringList archOrder = archPreference(os);

    for (const auto& arch : archOrder) {
        for (const QJsonValue& assetVal : assets) {
            const QJsonObject asset = assetVal.toObject();
            const QString name = asset["name"].toString();
            if (!name.contains(token, Qt::CaseInsensitive)) {
                continue;
            }
            if (!name.contains(arch, Qt::CaseInsensitive)) {
                continue;
            }
            return asset["browser_download_url"].toString();
        }
    }

    for (const QJsonValue& assetVal : assets) {
        const QJsonObject asset = assetVal.toObject();
        const QString name = asset["name"].toString();
        if (name.contains(token, Qt::CaseInsensitive)) {
            return asset["browser_download_url"].toString();
        }
    }

    return {};
}

bool isArchiveUrl(const QString& url)
{
    return url.endsWith(".tar.gz") || url.endsWith(".zip");
}

QString findExecutableInDir(const QString& rootDir, Os os)
{
    QString exactName;
    bool requireExeSuffix = false;
    if (os == Os::Windows) {
        exactName = "terracotta.exe";
        requireExeSuffix = true;
    } else {
        exactName = "terracotta";
    }

    QString fallback;

    QDirIterator it(rootDir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QFileInfo info = it.fileInfo();
        const QString fileName = info.fileName();

        if (fileName.compare(exactName, Qt::CaseInsensitive) == 0) {
            return info.absoluteFilePath();
        }

        if (!fileName.contains("terracotta", Qt::CaseInsensitive)) {
            continue;
        }

        if (requireExeSuffix && !fileName.endsWith(".exe", Qt::CaseInsensitive)) {
            continue;
        }

        if (fallback.isEmpty()) {
            fallback = info.absoluteFilePath();
        }
    }

    return fallback;
}

}  // namespace TerracottaReleaseUtils

