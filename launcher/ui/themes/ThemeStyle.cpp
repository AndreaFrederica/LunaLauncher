// SPDX-License-Identifier: GPL-3.0-only
#include "ThemeStyle.h"
#include <QStyleFactory>
#include <QResource>
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
#include "Kvantum.h"

static void initializeKvantum()
{
    // Referenced explicitly so the resource object is retained in static builds.
    Q_INIT_RESOURCE(defaulttheme);
}
#endif

QStringList ThemeStyle::engines()
{
    auto result = QStyleFactory::keys();
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    result.append("Kvantum");
#endif
    result.removeDuplicates();
    return result;
}

QStyle* ThemeStyle::create(const QString& engine, const QString& configuration)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    if (engine.compare("Kvantum", Qt::CaseInsensitive) == 0) {
        initializeKvantum();
        // An empty path deliberately selects the built-in theme, never the
        // user's desktop-wide Kvantum configuration.
        return new Kvantum::Style(false, configuration);
    }
#else
    Q_UNUSED(configuration);
#endif
    return QStyleFactory::create(engine);
}
