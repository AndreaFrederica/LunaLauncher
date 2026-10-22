// SPDX-License-Identifier: GPL-3.0-only

#include "ModApiMirror.h"

#include "Application.h"
#include "BuildConfig.h"
#include "settings/SettingsObject.h"

namespace {

SettingsObject* effectiveSettings(SettingsObject* settings)
{
    if (settings)
        return settings;
    return APPLICATION ? APPLICATION->settings() : nullptr;
}

QString customBase(SettingsObject* settings, const QString& key)
{
    if (!settings)
        return {};
    QString base = settings->get(key).toString().trimmed();
    while (base.endsWith('/'))
        base.chop(1);
    return base;
}

}  // namespace

namespace ModApiMirror {

QString modrinthBaseUrl(SettingsObject* settings)
{
    settings = effectiveSettings(settings);
    if (!settings)
        return BuildConfig.MODRINTH_PROD_URL;

    switch (settings->get("ModrinthMirror").toInt()) {
        case MCIM:
            return QStringLiteral("https://mod.mcimirror.top/modrinth/v2");
        case Custom: {
            const QString base = customBase(settings, "ModrinthMirrorURL");
            return base.isEmpty() ? BuildConfig.MODRINTH_PROD_URL : base;
        }
        default:
            return BuildConfig.MODRINTH_PROD_URL;
    }
}

QString curseForgeBaseUrl(SettingsObject* settings)
{
    settings = effectiveSettings(settings);
    if (!settings)
        return BuildConfig.FLAME_BASE_URL;

    switch (settings->get("CurseForgeMirror").toInt()) {
        case MCIM:
            return QStringLiteral("https://mod.mcimirror.top/curseforge/v1");
        case Custom: {
            const QString base = customBase(settings, "CurseForgeMirrorURL");
            return base.isEmpty() ? BuildConfig.FLAME_BASE_URL : base;
        }
        default:
            return BuildConfig.FLAME_BASE_URL;
    }
}

}  // namespace ModApiMirror
