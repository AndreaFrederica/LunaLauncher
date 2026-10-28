// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QPalette>
#include <functional>

namespace ThemePackage {
struct Palette {
    QString name;
    QPalette colors;
    QJsonObject semantic;
    bool dark = false;
};

// These functions never change the application's active appearance.
bool readColors(const QString& path, Palette& result, QString& error);
bool validateKvantum(const QString& configuration, QString& error);
// Read the window-compositing hints carried by a Kvantum configuration. The
// returned object uses stable, renderer-independent names so Classic UI can
// apply the same theme on platforms that do not run KWin.
QJsonObject readKvantumEffects(const QString& configuration, QString& error);
QJsonObject readGlobalTheme(const QString& defaults, QString& error);
QString containedPath(const QDir& root, const QString& relative, QString& error);
// On Windows, try to enable NTFS' directory-local case-sensitive flag. This
// is deliberately best-effort: callers must retain a portable fallback for
// FAT/exFAT, network shares, older Windows versions, and directories without
// the required ACL. The function never creates the directory.
bool enableCaseSensitiveDirectory(const QDir& directory);
bool extract(const QString& archive, const QDir& destination, QString& error, const std::function<bool()>& cancelled = {});
QJsonArray inspect(const QDir& root, QString& error);
bool validateStyleSheet(const QString& text, const QDir& root, QString& error);
QString scopedStyleSheet(const QString& text, const QDir& root, const QString& resourcePrefix, QString& error);
QString digest(const QString& path, QString& error);
}  // namespace ThemePackage
