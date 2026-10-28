// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QStringList>
class QStyle;
namespace ThemeStyle {
QStringList engines();
QStyle* create(const QString& engine, const QString& configuration = {});
}
