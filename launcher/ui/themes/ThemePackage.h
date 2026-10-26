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
QString containedPath(const QDir& root, const QString& relative, QString& error);
bool extract(const QString& archive, const QDir& destination, QString& error, const std::function<bool()>& cancelled = {});
QJsonArray inspect(const QDir& root, QString& error);
bool validateStyleSheet(const QString& text, const QDir& root, QString& error);
QString scopedStyleSheet(const QString& text, const QDir& root, const QString& resourcePrefix, QString& error);
QString digest(const QString& path, QString& error);
}  // namespace ThemePackage
