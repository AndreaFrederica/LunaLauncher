// SPDX-License-Identifier: MPL-2.0
#include "LauncherApiAppearance.h"

#include <QCryptographicHash>
#include <QDirIterator>
#include <QFile>
#include <QJsonDocument>
#include <QSet>
#include <QTemporaryDir>
#include <QUrl>
#include "Application.h"
#include "api/LauncherApiSupport.h"
#include "archive/ArchiveReader.h"
#include "cli/OperationService.h"
#include "net/Download.h"
#include "net/NetJob.h"
#include "settings/SettingsObject.h"
#include "translations/TranslationsModel.h"
#include "ui/themes/AppearanceService.h"
#include "ui/themes/ThemeInstallTask.h"
#include "ui/themes/ThemeManager.h"
#include "ui/themes/ThemePackage.h"
#include "ui/themes/ThemeStore.h"

namespace {
using namespace ApiSupport;
QString themePrefix(const QString& kind)
{
    if (kind == "theme")
        return "themes/";
    if (kind == "icons")
        return "icons/";
    if (kind == "background")
        return "cats/";
    return {};
}
QDir themeDirectory(const QString& kind)
{
    auto manager = APPLICATION->themeManager();
    if (kind == "theme")
        return manager->getApplicationThemesFolder();
    if (kind == "icons")
        return manager->getIconThemesFolder();
    return manager->getCatPacksFolder();
}
QString repository(const QString& source)
{
    const QUrl url(source);
    const auto parts = url.path().split('/', Qt::SkipEmptyParts);
    if (url.scheme() != "https" || url.host() != "github.com" || parts.size() != 2 || !identifier(parts[0]) || !identifier(parts[1]))
        return {};
    return parts.join('/');
}
QJsonObject themeRelease(LauncherApi& api, const QJsonObject& p, UserInteraction& interaction)
{
    const auto repo = repository(p.value("source").toString());
    if (repo.isEmpty())
        return OperationService::failure("source must be an HTTPS GitHub repository URL.", 2);
    auto job = makeShared<NetJob>("Theme release catalog", APPLICATION->network());
    auto [request, bytes] = Net::Download::makeByteArray(QUrl("https://api.github.com/repos/" + repo + "/releases/latest"));
    job->addNetAction(request);
    QString error;
    if (!wait(api, job, interaction, error))
        return OperationService::failure(error);
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(*bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return OperationService::failure("Invalid theme release response.");
    const auto release = document.object();
    QJsonArray assets;
    for (const auto& item : release.value("assets").toArray()) {
        auto asset = item.toObject();
        const auto name = asset.value("name").toString();
        const auto kind = name.endsWith("-theme.zip")     ? "theme"
                          : name.endsWith("-icons.zip")   ? "icons"
                          : name.endsWith("-catpack.zip") ? "background"
                                                          : "";
        if (!*kind)
            continue;
        assets.append(QJsonObject{ { "name", name },
                                   { "kind", kind },
                                   { "url", asset.value("browser_download_url") },
                                   { "size", asset.value("size") },
                                   { "digest", asset.value("digest") } });
    }
    return OperationService::success(
        QJsonObject{ { "tag", release.value("tag_name") }, { "assets", assets }, { "source", p.value("source") } });
}
QJsonObject installTheme(LauncherApi& api, const QJsonObject& p, UserInteraction& interaction)
{
    APPLICATION->themeManager()->appearance()->initialize();
    const auto kind = p.value("kind").toString();
    const auto prefix = themePrefix(kind);
    if (prefix.isEmpty())
        return OperationService::failure("Unknown package kind.", 2);
    if (p.contains("path") == p.contains("url"))
        return OperationService::failure("Provide exactly one of path or url.", 2);
    QTemporaryDir download;
    if (!download.isValid())
        return OperationService::failure("Could not create download directory.");
    QString path = p.value("path").toString();
    if (p.contains("url")) {
        const QUrl url(p.value("url").toString());
        if (!url.isValid() || (url.scheme() != "https" && url.scheme() != "http") || url.host().isEmpty())
            return OperationService::failure("Invalid download URL.", 2);
        path = download.filePath("package.zip");
        auto job = makeShared<NetJob>("Theme package download", APPLICATION->network());
        job->addNetAction(Net::Download::makeFile(url, path));
        QString error;
        if (!wait(api, job, interaction, error))
            return OperationService::failure(error);
    }
    QFile input(path);
    if (!input.open(QIODevice::ReadOnly))
        return OperationService::failure(input.errorString());
    if (p.contains("size") && input.size() != p.value("size").toInteger())
        return OperationService::failure("Package size mismatch.", 2);
    auto digest = p.value("digest").toString();
    if (!digest.isEmpty()) {
        if (!digest.startsWith("sha256:") || digest.size() != 71)
            return OperationService::failure("Expected sha256 digest.", 2);
        QCryptographicHash hash(QCryptographicHash::Sha256);
        if (!hash.addData(&input) || QString::fromLatin1(hash.result().toHex()).compare(digest.mid(7), Qt::CaseInsensitive))
            return OperationService::failure("Package checksum mismatch.", 2);
    }
    input.close();
    auto target = themeDirectory(kind);
    if (!target.mkpath("."))
        return OperationService::failure("Could not create theme directory.");
    QTemporaryDir staging(target.absoluteFilePath(".api-install-XXXXXX"));
    if (!staging.isValid())
        return OperationService::failure("Could not stage theme package.");
    QSet<QString> packages, manifests;
    QString error;
    QDir unpacked(staging.filePath("unpacked"));
    QDir().mkpath(unpacked.absolutePath());
    if (!ThemePackage::extract(path, unpacked, error))
        return OperationService::failure(error, 2);
    const auto manifest = kind == "theme" ? "theme.json" : kind == "icons" ? "index.theme" : "catpack.json";
    QDirIterator files(unpacked.absolutePath(), QDir::Files, QDirIterator::Subdirectories);
    while (files.hasNext()) {
        const auto name = unpacked.relativeFilePath(files.next());
        const auto parts = name.split('/');
        if (!name.startsWith(prefix) || parts.size() < 3 || !identifier(parts[1]))
            return OperationService::failure("Invalid package folder.", 2);
        packages.insert(parts[1]);
        if (parts.size() == 3 && parts[2] == manifest)
            manifests.insert(parts[1]);
    }
    if (packages.isEmpty() || packages != manifests)
        return OperationService::failure("Every package must contain its manifest.", 2);
    ThemePackage::inspect(unpacked, error);
    if (!error.isEmpty())
        return OperationService::failure(error, 2);
    if (!QDir().rename(unpacked.filePath(prefix), staging.filePath("new")))
        return OperationService::failure("Could not prepare package transaction.");
    for (const auto& id : packages) {
        if (QFileInfo::exists(target.filePath(id)) && APPLICATION->themeManager()->appearance()->isUsed(id))
            return OperationService::failure(
                "This package is referenced by the current or pending appearance. Import an immutable new version instead.", 2);
        const QFileInfo existing(target.filePath(id));
        if (existing.isSymLink() || (existing.exists() && (!existing.isDir() || !p.value("overwrite").toBool())))
            return OperationService::failure("Package already exists or is a link: " + id, 2);
    }
    QStringList published, backedUp;
    QDir().mkpath(staging.filePath("old"));
    for (const auto& id : packages) {
        if (QFileInfo::exists(target.filePath(id))) {
            if (!QDir().rename(target.filePath(id), staging.filePath("old/" + id))) {
                error = "Could not back up package: " + id;
                break;
            }
            backedUp.append(id);
        }
        if (!QDir().rename(staging.filePath("new/" + id), target.filePath(id))) {
            error = "Could not publish package: " + id;
            break;
        }
        published.append(id);
    }
    if (!error.isEmpty()) {
        for (const auto& id : published)
            QDir().rename(target.filePath(id), staging.filePath("new/" + id));
        bool restored = true;
        for (const auto& id : backedUp)
            restored &= QDir().rename(staging.filePath("old/" + id), target.filePath(id));
        if (!restored) {
            staging.setAutoRemove(false);
            error += "; recovery files: " + staging.path();
        }
        return OperationService::failure(error);
    }
    APPLICATION->themeManager()->refresh();
    return OperationService::success(QJsonObject{ { "installed", QJsonArray::fromStringList(published) }, { "kind", kind } });
}
struct Appearance {
    ThemeManager* themes() { return APPLICATION->themeManager(); }
    QJsonObject selected()
    {
        auto settings = APPLICATION->settings();
        return { { "theme", settings->get("ApplicationTheme").toString() },
                 { "icons", settings->get("IconTheme").toString() },
                 { "background", settings->get("BackgroundCat").toString() } };
    }
    QJsonObject catalog()
    {
        auto manager = themes();
        manager->appearance()->initialize();
        QJsonArray themes, icons, backgrounds;
        for (auto theme : manager->getValidApplicationThemes())
            themes.append(QJsonObject{ { "id", theme->id() }, { "name", theme->name() }, { "description", theme->tooltip() } });
        for (auto icon : manager->getValidIconThemes())
            icons.append(QJsonObject{ { "id", icon->id() }, { "name", icon->name() }, { "path", icon->path() } });
        for (auto background : manager->getValidCatPacks())
            backgrounds.append(QJsonObject{ { "id", background->id() }, { "name", background->name() }, { "path", background->path() } });
        return { { "themes", themes },
                 { "icons", icons },
                 { "backgrounds", backgrounds },
                 { "selected", selected() },
                 { "directories", QJsonObject{ { "themes", manager->getApplicationThemesFolder().absolutePath() },
                                               { "icons", manager->getIconThemesFolder().absolutePath() },
                                               { "backgrounds", manager->getCatPacksFolder().absolutePath() } } } };
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
            for (auto background : manager->getValidCatPacks())
                found |= background->id() == p.value("background").toString();
            if (!found)
                return OperationService::failure("Unknown background pack.", 2);
        }
        auto config = p;
        if (p.contains("theme")) {
            config.insert("mode", p.value("theme") == "system" ? "system" : "independent");
            config.insert("palette", "");
            config.insert("colorMode", "fixed");
        }
        auto service = manager->appearance();
        service->initialize();
        QString error;
        auto appearanceState = service->apply(config, service->state().value("revision").toInteger(), error);
        if (appearanceState.isEmpty())
            return OperationService::failure(error, 2);
        auto result = selected();
        result.insert("appliedToBackend", !appearanceState.value("requiresRestart").toBool());
        result.insert("requiresRestart", appearanceState.value("requiresRestart"));
        result.insert("state", appearanceState);
        return OperationService::success(result);
    }
};

bool knownLanguage(const QString& id)
{
    auto model = APPLICATION->translations();
    for (int i = 0; i < model->rowCount(); ++i)
        if (model->index(i, 0).data(Qt::UserRole).toString() == id)
            return true;
    return false;
}

QJsonObject languages()
{
    auto model = APPLICATION->translations();
    QJsonArray entries;
    for (int i = 0; i < model->rowCount(); ++i) {
        const auto index = model->index(i, 0);
        entries.append(QJsonObject{ { "id", index.data(Qt::UserRole).toString() },
                                    { "name", index.data().toString() },
                                    { "completeness", model->index(i, 1).data().toString() },
                                    { "description", index.data(Qt::ToolTipRole).toString() } });
    }
    return { { "languages", entries },
             { "selected", model->selectedLanguage() },
             { "useSystemLocale", APPLICATION->settings()->get("UseSystemLocale").toBool() } };
}
}  // namespace

void registerLauncherApiAppearanceOperations(LauncherApi& api)
{
    using namespace ApiSupport;
    const auto state = std::make_shared<Appearance>();
    auto appearance = [] {
        auto service = APPLICATION->themeManager()->appearance();
        service->initialize();
        return service;
    };
    api.registerOperation(
        { "appearance.capabilities", "Read native Classic theme formats, styles and platform capabilities.", schema({}), "appearance" },
        [appearance](const QJsonObject&, UserInteraction&) { return OperationService::success(appearance()->capabilities()); });
    api.registerOperation(
        { "appearance.state", "Read saved, effective and pending Classic appearance and its revision.", schema({}), "appearance" },
        [appearance](const QJsonObject&, UserInteraction&) {
            auto service = appearance();
            auto result = service->state();
            result.insert("components", service->components());
            result.insert("packages", service->packages());
            return OperationService::success(result);
        });
    const auto revision = QJsonObject{ { "type", "integer" }, { "minimum", 0 } };
    QJsonObject configProperties;
    for (const auto& key :
         { "mode", "theme", "icons", "background", "palette", "colorMode", "lightPalette", "darkPalette", "decoration", "decorationTheme" })
        configProperties.insert(key, string("Classic appearance component or mode."));
    configProperties.insert("systemTitlebarPalette", boolean());
    const auto configuration = schema(configProperties);
    api.registerOperation(
        { "appearance.apply", "Validate and commit an appearance combination; structural changes wait for restart.",
          schema({ { "configuration", configuration }, { "baseRevision", revision } }, { "configuration", "baseRevision" }), "appearance",
          true },
        [appearance](const QJsonObject& p, UserInteraction&) {
            QString error;
            const auto result = appearance()->apply(p.value("configuration").toObject(), p.value("baseRevision").toInteger(-1), error);
            return result.isEmpty() ? OperationService::failure(error, 2) : OperationService::success(result);
        });
    api.registerOperation({ "appearance.undo", "Revert pending appearance changes to the currently effective combination.",
                            schema({ { "baseRevision", revision } }, { "baseRevision" }), "appearance", true },
                          [appearance](const QJsonObject& p, UserInteraction&) {
                              QString error;
                              auto service = appearance();
                              if (!service->undoPending(p.value("baseRevision").toInteger(-1), error))
                                  return OperationService::failure(error, 2);
                              return OperationService::success(service->state());
                          });
    api.registerOperation({ "appearance.preview", "Open an isolated native Qt preview without accessing launcher accounts or data locks.",
                            schema({ { "configuration", configuration } }), "appearance", true },
                          [appearance](const QJsonObject& p, UserInteraction&) {
                              QString error;
                              appearance()->preview(p.value("configuration").toObject(), nullptr, error);
                              return error.isEmpty() ? OperationService::success(QJsonObject{ { "started", true } })
                                                     : OperationService::failure(error, 2);
                          });
    auto querySchema = schema({ { "provider", string("kde or github.") },
                                { "source", string("HTTPS GitHub repository URL.") },
                                { "category", string("palette, icons, bundle, widgets, decoration, plasma.") },
                                { "sort", string("high, new, alpha, down.") },
                                { "page", revision },
                                { "refresh", boolean() },
                                { "contentId", string("Store content identity.") } });
    for (const auto& mode : { QString("query"), QString("files") })
        api.registerOperation(
            { "appearance." + mode, "Read theme store catalog/files with bounded local offline caching.", querySchema, "appearance" },
            [&api, appearance, mode](const QJsonObject& p, UserInteraction& ui) {
                auto request = p;
                request.insert("mode", mode);
                auto task = makeShared<ThemeStoreTask>(APPLICATION->network().get(), request, appearance()->cacheDirectory());
                QString error;
                if (!wait(api, task, ui, error))
                    return OperationService::failure(error);
                return OperationService::success(task->result());
            });
    const auto importSchema = schema({ { "path", string("Local archive, .colors, QSS file or directory.") },
                                       { "url", string("HTTPS download URL.") },
                                       { "name", string("Source filename for raw palette imports.") },
                                       { "provider", string("kde or github.") },
                                       { "source", string("HTTPS GitHub repository URL.") },
                                       { "category", string("Store category.") },
                                       { "contentId", string("Store content identity.") },
                                       { "fileId", string("Explicit store file identity.") },
                                       { "components", strings() },
                                       { "title", string("Package display name.") },
                                       { "version", string("Source version.") },
                                       { "digest", string("Optional sha256: checksum.") },
                                       { "size", revision } });
    auto importTheme = [&api, appearance](const QJsonObject& p, UserInteraction& ui) {
        auto service = appearance();
        auto task = makeShared<ThemeInstallTask>(APPLICATION->network().get(), p, service->packageDirectory(), service->cacheDirectory());
        service->retainTask(task);
        QString error;
        if (!wait(api, task, ui, error))
            return OperationService::failure(error);
        APPLICATION->themeManager()->refresh();
        auto result = task->result();
        result.insert("operationId", task->getUid().toString());
        return OperationService::success(result);
    };
    api.registerOperation(
        { "appearance.import", "Install immutable components from local files, KDE Store or Prism/GitHub; never auto-apply.", importSchema,
          "appearance", true },
        importTheme);
    api.registerOperation(
        { "appearance.package.remove", "Remove an unreferenced immutable theme package.",
          schema({ { "id", string("Package digest identity.") }, { "confirm", boolean() } }, { "id", "confirm" }), "appearance", true },
        [appearance](const QJsonObject& p, UserInteraction&) {
            QString error;
            if (!p.value("confirm").toBool())
                return OperationService::failure("Removal requires confirmation.", 2);
            if (!appearance()->removePackage(p.value("id").toString(), error))
                return OperationService::failure(error, 2);
            return OperationService::success(QJsonObject{ { "removed", p.value("id") } });
        });
    api.registerOperation({ "appearance.tasks", "Read all retained theme installation tasks.", schema({}), "appearance" },
                          [appearance](const QJsonObject&, UserInteraction&) {
                              QJsonArray result;
                              for (const auto& task : appearance()->tasks())
                                  result.append(QJsonObject{ { "operationId", task->getUid().toString() },
                                                             { "status", task->getStatus() },
                                                             { "current", task->getProgress() },
                                                             { "total", task->getTotalProgress() },
                                                             { "finished", task->isFinished() },
                                                             { "succeeded", task->wasSuccessful() },
                                                             { "canAbort", task->canAbort() },
                                                             { "error", task->failReason() } });
                              return OperationService::success(result);
                          });
    api.registerOperation({ "appearance.cache.clear", "Clear only theme catalog cache; installed packages are preserved.",
                            schema({ { "confirm", boolean() } }, { "confirm" }), "appearance", true },
                          [appearance](const QJsonObject& p, UserInteraction&) {
                              if (!p.value("confirm").toBool())
                                  return OperationService::failure("Cache clear requires confirmation.", 2);
                              auto dir = QDir(appearance()->cacheDirectory());
                              return !dir.exists() || dir.removeRecursively() ? OperationService::success()
                                                                              : OperationService::failure("Could not clear theme cache.");
                          });
    api.registerOperation(
        { "appearance.sources", "Read or replace GitHub theme download sources.", schema({ { "sources", strings() } }), "appearance",
          true },
        [](const QJsonObject& p, UserInteraction&) {
            if (p.contains("sources")) {
                QStringList sources;
                for (const auto& value : p.value("sources").toArray()) {
                    const auto repo = repository(value.toString());
                    if (repo.isEmpty())
                        return OperationService::failure("Invalid GitHub repository URL.", 2);
                    const auto url = "https://github.com/" + repo;
                    if (!sources.contains(url, Qt::CaseInsensitive))
                        sources.append(url);
                }
                APPLICATION->settings()->set("ThemeDownloadSources", sources);
            }
            return OperationService::success(QJsonObject{
                { "sources", QJsonArray::fromStringList(APPLICATION->settings()->get("ThemeDownloadSources").toStringList()) } });
        });
    api.registerOperation({ "appearance.release", "List installable packages in a repository's latest release.",
                            schema({ { "source", string("HTTPS GitHub repository URL.") } }, { "source" }), "appearance" },
                          [&api](const QJsonObject& p, UserInteraction& ui) { return themeRelease(api, p, ui); });
    api.registerOperation({ "appearance.install", "Download or import a theme, icon or background archive with staged replacement.",
                            schema({ { "kind", string("theme, icons, or background.") },
                                     { "path", string("Local ZIP file.") },
                                     { "url", string("Download URL.") },
                                     { "digest", string("Optional sha256: digest from release metadata.") },
                                     { "size", QJsonObject{ { "type", "integer" }, { "minimum", 0 } } },
                                     { "overwrite", boolean() },
                                     { "provider", string("kde or github.") },
                                     { "source", string("GitHub source.") },
                                     { "category", string("Store category.") },
                                     { "contentId", string("Content ID.") },
                                     { "fileId", string("File ID.") },
                                     { "name", string("Source filename.") },
                                     { "components", strings() } }),
                            "appearance", true },
                          [&api, importTheme](const QJsonObject& p, UserInteraction& ui) {
                              return p.contains("kind") ? installTheme(api, p, ui) : importTheme(p, ui);
                          });
    api.registerOperation(
        { "appearance.remove", "Remove an installed custom package; built-in and selected packages are protected.",
          schema({ { "kind", string("theme, icons, or background.") }, { "id", string("Package folder name.") }, { "confirm", boolean() } },
                 { "kind", "id", "confirm" }),
          "appearance", true },
        [state](const QJsonObject& p, UserInteraction&) {
            const auto kind = p.value("kind").toString(), id = p.value("id").toString();
            if (!p.value("confirm").toBool() || themePrefix(kind).isEmpty() || !identifier(id))
                return OperationService::failure("Invalid package or missing confirmation.", 2);
            if (state->themes()->appearance()->isUsed(id))
                return OperationService::failure("Select another package before removing this one.", 2);
            const QFileInfo file(themeDirectory(kind).filePath(id));
            if (!file.isDir() || file.isSymLink())
                return OperationService::failure("Custom package not found.", 2);
            if (!QDir(file.absoluteFilePath()).removeRecursively())
                return OperationService::failure("Could not remove package.");
            state->themes()->refresh();
            return OperationService::success(QJsonObject{ { "removed", id } });
        });
    api.registerOperation(
        { "appearance.catalog", "List installed application/icon themes and background packs.", schema({}), "appearance" },
        [state](const QJsonObject&, UserInteraction&) { return OperationService::success(state->catalog()); });
    api.registerOperation({ "appearance.refresh", "Rescan installed theme and background directories.", schema({}), "appearance" },
                          [state](const QJsonObject&, UserInteraction&) {
                              state->themes()->refresh();
                              return OperationService::success(state->catalog());
                          });
    api.registerOperation({ "appearance.select", "Validate, save and apply appearance selections in the launcher backend.",
                            schema({ { "theme", string("Application theme ID.") },
                                     { "icons", string("Icon theme ID.") },
                                     { "background", string("Background pack ID.") } }),
                            "appearance", true },
                          [state](const QJsonObject& p, UserInteraction&) { return state->select(p); });
    api.registerOperation({ "language.list", "List known languages and the active backend language.", schema({}), "appearance" },
                          [](const QJsonObject&, UserInteraction&) { return OperationService::success(languages()); });
    api.registerOperation(
        { "language.select", "Save a language and apply available local translations; request missing translations asynchronously.",
          schema({ { "language", string("Language ID from language.list.") }, { "useSystemLocale", boolean() } }, { "language" }),
          "appearance", true },
        [](const QJsonObject& p, UserInteraction&) {
            const auto id = p.value("language").toString();
            if (!knownLanguage(id))
                return OperationService::failure("Unknown language.", 2);
            auto model = APPLICATION->translations();
            if (p.contains("useSystemLocale"))
                model->setUseSystemLocale(p.value("useSystemLocale").toBool());
            APPLICATION->settings()->set("Language", id);
            const auto loaded = model->selectLanguage(id);
            if (id != "en_US")
                model->updateLanguage(id);
            return OperationService::success(
                QJsonObject{ { "selected", model->selectedLanguage() },
                             { "localTranslationLoaded", loaded },
                             { "updateRequested", id != "en_US" },
                             { "useSystemLocale", APPLICATION->settings()->get("UseSystemLocale").toBool() } });
        });
    api.registerOperation(
        { "language.refresh", "Request an asynchronous refresh of the language index or one translation.",
          schema({ { "language", string("Optional known language ID; omit to refresh the catalog.") } }), "appearance" },
        [](const QJsonObject& p, UserInteraction&) {
            if (p.contains("language")) {
                if (!knownLanguage(p.value("language").toString()))
                    return OperationService::failure("Unknown language.", 2);
                if (p.value("language").toString() == "en_US")
                    return OperationService::success(QJsonObject{ { "requested", false }, { "completed", true }, { "builtin", true } });
                APPLICATION->translations()->updateLanguage(p.value("language").toString());
            } else
                APPLICATION->translations()->downloadIndex();
            return OperationService::success(QJsonObject{ { "requested", true }, { "completed", false } });
        });
}
