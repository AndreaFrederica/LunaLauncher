// SPDX-License-Identifier: GPL-3.0-only

#include "ModApiMirror.h"

#include "Application.h"
#include "BuildConfig.h"
#include "settings/SettingsObject.h"

namespace ModApiMirror {

QString modrinthBaseUrl()
{
    if (APPLICATION->settings()->get("ModrinthMirror").toInt() == MCIM) {
        return QStringLiteral("https://mod.mcimirror.top/modrinth/v2");
    }
    return BuildConfig.MODRINTH_PROD_URL;
}

QString curseForgeBaseUrl()
{
    if (APPLICATION->settings()->get("CurseForgeMirror").toInt() == MCIM) {
        return QStringLiteral("https://mod.mcimirror.top/curseforge/v1");
    }
    return BuildConfig.FLAME_BASE_URL;
}

}  // namespace ModApiMirror
