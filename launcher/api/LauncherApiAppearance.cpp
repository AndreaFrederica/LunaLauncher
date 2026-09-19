// SPDX-License-Identifier: GPL-3.0-only
#include "LauncherApiAppearance.h"

#include "Application.h"
#include "api/LauncherApiSupport.h"
#include "cli/OperationService.h"
#include "settings/SettingsObject.h"
#include "translations/TranslationsModel.h"
#include "ui/themes/ThemeManager.h"

namespace {
using namespace ApiSupport;
struct Appearance {
    ThemeManager* themes()
    {
        return APPLICATION->themeManager();
    }
    QJsonObject selected()
    {
        auto settings = APPLICATION->settings();
        return { { "theme", settings->get("ApplicationTheme").toString() }, { "icons", settings->get("IconTheme").toString() },
                 { "background", settings->get("BackgroundCat").toString() } };
    }
    QJsonObject catalog()
    {
        auto manager = themes();
        QJsonArray themes, icons, backgrounds;
        for (auto theme : manager->getValidApplicationThemes())
            themes.append(QJsonObject{ { "id", theme->id() }, { "name", theme->name() }, { "description", theme->tooltip() } });
        for (auto icon : manager->getValidIconThemes())
            icons.append(QJsonObject{ { "id", icon->id() }, { "name", icon->name() }, { "path", icon->path() } });
        for (auto background : manager->getValidCatPacks())
            backgrounds.append(QJsonObject{ { "id", background->id() }, { "name", background->name() }, { "path", background->path() } });
        return { { "themes", themes }, { "icons", icons }, { "backgrounds", backgrounds }, { "selected", selected() },
                 { "directories", QJsonObject{ { "themes", manager->getApplicationThemesFolder().absolutePath() },
                     { "icons", manager->getIconThemesFolder().absolutePath() }, { "backgrounds", manager->getCatPacksFolder().absolutePath() } } } };
    }
    QJsonObject select(const QJsonObject& p)
    {
        auto manager = themes();
        if (p.contains("theme") && !manager->isValidApplicationTheme(p.value("theme").toString()))
            return OperationService::failure("Unknown application theme.", 2);
        if (p.contains("icons") && !manager->isValidIconTheme(p.value("icons").toString()))
            return OperationService::failure("Unknown icon theme.", 2);
        if (p.contains("background")) {
            bool found = false;
            for (auto background : manager->getValidCatPacks()) found |= background->id() == p.value("background").toString();
            if (!found) return OperationService::failure("Unknown background pack.", 2);
        }
        auto settings = APPLICATION->settings();
        if (p.contains("theme")) settings->set("ApplicationTheme", p.value("theme").toString());
        if (p.contains("icons")) settings->set("IconTheme", p.value("icons").toString());
        if (p.contains("background")) settings->set("BackgroundCat", p.value("background").toString());
        manager->applyCurrentlySelectedTheme();
        auto result = selected();
        result.insert("appliedToBackend", true);
        return OperationService::success(result);
    }
};

bool knownLanguage(const QString& id)
{
    auto model = APPLICATION->translations();
    for (int i = 0; i < model->rowCount(); ++i)
        if (model->index(i, 0).data(Qt::UserRole).toString() == id) return true;
    return false;
}

QJsonObject languages()
{
    auto model = APPLICATION->translations();
    QJsonArray entries;
    for (int i = 0; i < model->rowCount(); ++i) {
        const auto index = model->index(i, 0);
        entries.append(QJsonObject{ { "id", index.data(Qt::UserRole).toString() }, { "name", index.data().toString() },
            { "completeness", model->index(i, 1).data().toString() }, { "description", index.data(Qt::ToolTipRole).toString() } });
    }
    return { { "languages", entries }, { "selected", model->selectedLanguage() },
             { "useSystemLocale", APPLICATION->settings()->get("UseSystemLocale").toBool() } };
}
}  // namespace

void registerLauncherApiAppearanceOperations(LauncherApi& api)
{
    using namespace ApiSupport;
    const auto state = std::make_shared<Appearance>();
    api.registerOperation({ "appearance.catalog", "List installed application/icon themes and background packs.", schema({}), "appearance" },
        [state](const QJsonObject&, UserInteraction&) { return OperationService::success(state->catalog()); });
    api.registerOperation({ "appearance.refresh", "Rescan installed theme and background directories.", schema({}), "appearance" },
        [state](const QJsonObject&, UserInteraction&) { state->themes()->refresh(); return OperationService::success(state->catalog()); });
    api.registerOperation({ "appearance.select", "Validate, save and apply appearance selections in the launcher backend.",
        schema({ { "theme", string("Application theme ID.") }, { "icons", string("Icon theme ID.") }, { "background", string("Background pack ID.") } }), "appearance", true },
        [state](const QJsonObject& p, UserInteraction&) { return state->select(p); });
    api.registerOperation({ "language.list", "List known languages and the active backend language.", schema({}), "appearance" },
        [](const QJsonObject&, UserInteraction&) { return OperationService::success(languages()); });
    api.registerOperation({ "language.select", "Save a language and apply available local translations; request missing translations asynchronously.",
        schema({ { "language", string("Language ID from language.list.") }, { "useSystemLocale", boolean() } }, { "language" }), "appearance", true },
        [](const QJsonObject& p, UserInteraction&) {
            const auto id = p.value("language").toString();
            if (!knownLanguage(id)) return OperationService::failure("Unknown language.", 2);
            auto model = APPLICATION->translations();
            if (p.contains("useSystemLocale")) model->setUseSystemLocale(p.value("useSystemLocale").toBool());
            APPLICATION->settings()->set("Language", id);
            const auto loaded = model->selectLanguage(id);
            if (id != "en_US") model->updateLanguage(id);
            return OperationService::success(QJsonObject{ { "selected", model->selectedLanguage() }, { "localTranslationLoaded", loaded },
                { "updateRequested", id != "en_US" }, { "useSystemLocale", APPLICATION->settings()->get("UseSystemLocale").toBool() } });
        });
    api.registerOperation({ "language.refresh", "Request an asynchronous refresh of the language index or one translation.",
        schema({ { "language", string("Optional known language ID; omit to refresh the catalog.") } }), "appearance" },
        [](const QJsonObject& p, UserInteraction&) {
            if (p.contains("language")) {
                if (!knownLanguage(p.value("language").toString())) return OperationService::failure("Unknown language.", 2);
                if (p.value("language").toString() == "en_US")
                    return OperationService::success(QJsonObject{ { "requested", false }, { "completed", true }, { "builtin", true } });
                APPLICATION->translations()->updateLanguage(p.value("language").toString());
            } else APPLICATION->translations()->downloadIndex();
            return OperationService::success(QJsonObject{ { "requested", true }, { "completed", false } });
        });
}
