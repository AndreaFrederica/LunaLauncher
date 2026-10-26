// SPDX-License-Identifier: GPL-3.0-only
#include "AppearanceService.h"

#include <QApplication>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QEvent>
#include <QFile>
#include <QJsonDocument>
#include <QLockFile>
#include <QLayout>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScopedValueRollback>
#include <QSet>
#include <QSettings>
#include <QStyle>
#include <QStyleHints>
#include <QTemporaryFile>
#include <QTimer>
#include <QWidget>
#include <stdexcept>
#include "Application.h"
#include "CustomTheme.h"
#include "ThemeManager.h"
#include "ThemePackage.h"
#include "ThemeStyle.h"
#include "WindowChromeController.h"
#include "settings/SettingsObject.h"

namespace {
class ManagedTheme final : public ITheme {
   public:
    ManagedTheme(QString id, QJsonObject descriptor, ITheme* base)
        : m_id(std::move(id)), m_descriptor(std::move(descriptor)), m_palette(base->colorScheme())
    {
        const auto path = m_descriptor.value("path").toString();
        m_root = m_descriptor.value("directory").toString();
        const auto format = m_descriptor.value("format").toString();
        if (format == "kvantum") {
            QString error;
            if (!ThemePackage::validateKvantum(path, error))
                throw std::runtime_error(error.toStdString());
            m_style = "Kvantum";
        } else if (format == "prism" || format == "legacy-qss") {
            QFileInfo info(path);
            m_legacy = std::make_unique<CustomTheme>(base, info, format == "prism");
            m_palette = m_legacy->colorScheme();
            m_style = m_legacy->qtTheme();
            QString error;
            m_qss = ThemePackage::scopedStyleSheet(m_legacy->appStyleSheet(), QDir(m_root), prefix(), error);
            if (!error.isEmpty())
                throw std::runtime_error(error.toStdString());
        } else {
            m_style = m_descriptor.value("engine").toString("Fusion");
            QFile input(path);
            if (!input.open(QIODevice::ReadOnly))
                throw std::runtime_error("Cannot read widget theme");
            QString error;
            m_qss = ThemePackage::scopedStyleSheet(QString::fromUtf8(input.readAll()), QDir(m_root), prefix(), error);
            if (!error.isEmpty())
                throw std::runtime_error(error.toStdString());
        }
        QDir::setSearchPaths(prefix(), { m_root });
    }
    QString id() override { return m_id; }
    QString name() override { return m_descriptor.value("name").toString(); }
    QString tooltip() override { return m_descriptor.value("description").toString(); }
    bool hasStyleSheet() override { return !m_qss.isEmpty(); }
    QString appStyleSheet() override { return m_qss; }
    QString qtTheme() override { return m_style; }
    QString widgetThemeFile() override { return m_descriptor.value("format") == "kvantum" ? m_descriptor.value("path").toString() : QString(); }
    QPalette colorScheme() override
    {
        if (m_descriptor.value("format") == "kvantum") {
            std::unique_ptr<QStyle> style(ThemeStyle::create(m_style, widgetThemeFile()));
            if (style)
                return style->standardPalette();
        }
        return m_palette;
    }
    QColor fadeColor() override { return Qt::gray; }
    double fadeAmount() override { return .5; }
    LogColors logColorScheme() override { return m_legacy ? m_legacy->logColorScheme() : defaultLogColors(colorScheme()); }
    QString prefix() const
    {
        return "luna" + QString::fromLatin1(QCryptographicHash::hash(m_id.toUtf8(), QCryptographicHash::Sha256).toHex().left(16));
    }

   private:
    QString m_id, m_root, m_qss, m_style;
    QJsonObject m_descriptor;
    QPalette m_palette;
    std::unique_ptr<CustomTheme> m_legacy;
};

bool containsReference(const QJsonObject& config, const QString& id)
{
    for (const auto& key : { "theme", "palette", "lightPalette", "darkPalette", "icons", "background", "decorationTheme" })
        if (config.value(key).toString() == id)
            return true;
    return false;
}
class ManagedBackground final : public CatPack {
   public:
    ManagedBackground(QString id, QFileInfo info) : m_id(std::move(id)), m_pack(info) {}
    QString id() const override { return m_id; }
    QString name() const override { return m_pack.name(); }
    QString path() const override { return m_pack.path(); }

   private:
    QString m_id;
    JsonCatPack m_pack;
};
}  // namespace

AppearanceService::AppearanceService(ThemeManager* manager) : m_manager(manager), m_chrome(std::make_unique<WindowChromeController>())
{
    reloadCatalog();
}
AppearanceService::~AppearanceService() = default;

QString AppearanceService::packageDirectory() const
{
    return QDir::current().absoluteFilePath("theme-packages");
}
QString AppearanceService::cacheDirectory() const
{
    return QDir::current().absoluteFilePath("cache/theme-store");
}

QJsonObject AppearanceService::defaults() const
{
    auto settings = APPLICATION->settings();
    auto theme = settings->get("ApplicationTheme").toString();
    auto icons = settings->get("IconTheme").toString();
    if (!m_manager->isValidApplicationTheme(theme))
        theme = "system";
    if (!m_manager->isValidIconTheme(icons))
        icons = "pe_colored";
    auto background = settings->get("BackgroundCat").toString();
    bool found = false;
    for (auto pack : m_manager->getValidCatPacks())
        found |= pack->id() == background;
    if (!found)
        background = "kitteh";
    return { { "mode", theme == "system" ? "system" : "independent" },
             { "theme", theme },
             { "icons", icons },
             { "background", background },
             { "palette", "" },
             { "colorMode", "fixed" },
             { "lightPalette", "" },
             { "darkPalette", "" },
             { "decoration", "system" },
             { "decorationTheme", "" },
             { "systemTitlebarPalette", true },
             { "windowEffectsEnabled", true },
             { "windowOpacity", 100 },
             { "windowBlur", 100 },
             { "windowMaterial", "theme" },
             { "windowRoundedCorners", true } };
}

QJsonObject AppearanceService::component(const QString& id) const
{
    for (const auto& value : m_components)
        if (value.toObject().value("id").toString() == id)
            return value.toObject();
    return {};
}

void AppearanceService::reloadCatalog()
{
    m_components = {};
    m_packages = {};
    for (auto theme : m_manager->getValidApplicationThemes()) {
        if (theme->id().startsWith("managed-"))
            continue;
        auto legacy = dynamic_cast<CustomTheme*>(theme);
        m_components.append(QJsonObject{ { "id", theme->id() },
                                         { "kind", "widgets" },
                                         { "name", theme->name() },
                                         { "format", dynamic_cast<CustomTheme*>(theme) ? "prism" : "builtin" },
                                         { "engine", theme->qtTheme() },
                                         { "description", theme->tooltip() },
                                         { "fixedColors", theme->hasStyleSheet() || legacy != nullptr },
                                         { "directory", legacy ? legacy->directory() : QString() } });
    }
    for (auto theme : m_manager->getValidIconThemes()) {
        if (theme->id().startsWith("managed-"))
            continue;
        m_components.append(QJsonObject{ { "id", theme->id() },
                                         { "kind", "icons" },
                                         { "name", theme->name() },
                                         { "format", "freedesktop" },
                                         { "directory", theme->path() },
                                         { "path", theme->path() } });
    }
    for (auto background : m_manager->getValidCatPacks()) {
        if (background->id().startsWith("managed-"))
            continue;
        m_components.append(QJsonObject{ { "id", background->id() },
                                         { "kind", "background" },
                                         { "name", background->name() },
                                         { "format", "prism" },
                                         { "path", background->path() } });
    }
    QDir packages(packageDirectory());
    for (const auto& directory : packages.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (!QRegularExpression("^[a-f0-9]{64}$").match(directory).hasMatch())
            continue;
        QDir root(packages.filePath(directory));
        QFile file(root.filePath(".luna-package.json"));
        if (!file.open(QIODevice::ReadOnly) || file.size() > 4 * 1024 * 1024)
            continue;
        auto record = QJsonDocument::fromJson(file.readAll()).object();
        QString error;
        auto entries = record.value("catalog").toArray();
        if (entries.isEmpty())
            entries = ThemePackage::inspect(root, error);
        for (const auto& value : entries) {
            const auto entry = value.toObject();
            if (!QRegularExpression("^[a-f0-9]{16}$").match(entry.value("id").toString()).hasMatch() ||
                ThemePackage::containedPath(root, entry.value("file").toString(), error).isEmpty() ||
                (entry.value("root") != "." && ThemePackage::containedPath(root, entry.value("root").toString(), error).isEmpty())) {
                if (error.isEmpty())
                    error = tr("Invalid installed component catalog.");
                break;
            }
        }
        if (!error.isEmpty()) {
            qWarning() << "Invalid installed theme package" << directory << error;
            continue;
        }
        QJsonArray registered;
        for (const auto& entry : entries) {
            auto data = entry.toObject();
            const auto selected = record.value("selectedComponents").toArray();
            if (!selected.isEmpty() && !selected.contains(data.value("id")))
                continue;
            const auto id = "managed-" + directory.left(16) + '-' + data.value("id").toString();
            data.insert("id", id);
            data.insert("packageId", directory);
            data.insert("path", root.absoluteFilePath(data.value("file").toString()));
            data.insert("directory", root.absoluteFilePath(data.value("root").toString()));
            const auto kind = data.value("kind").toString();
            if (kind == "widgets") {
                // Older package records predate the effect metadata. Fill it
                // from the immutable Kvantum file so upgrading does not
                // require reinstalling a theme.
                if (data.value("format").toString() == "kvantum" && !data.contains("effects")) {
                    QString effectsError;
                    const auto effects = ThemePackage::readKvantumEffects(data.value("path").toString(), effectsError);
                    if (effectsError.isEmpty())
                        data.insert("effects", effects);
                }
                if (data.value("format") == "prism" || data.value("format") == "legacy-qss")
                    data.insert("fixedColors", true);
                try {
                    auto theme = std::make_unique<ManagedTheme>(id, data, m_manager->getTheme("dark"));
                    data.insert("engine", theme->qtTheme());
                    if (!ThemeStyle::engines().contains(theme->qtTheme(), Qt::CaseInsensitive))
                        data.insert("unavailable", tr("Required Qt widget style is not installed: %1").arg(theme->qtTheme()));
                    else
                        m_manager->addTheme(std::move(theme));
                } catch (const std::exception& e) {
                    data.insert("unavailable", QString::fromUtf8(e.what()));
                }
                if (!ThemeStyle::engines().contains(data.value("engine").toString("Fusion"), Qt::CaseInsensitive) &&
                    data.value("format").toString() == "qss-v2")
                    data.insert("unavailable", tr("Required widget style is not installed."));
            } else if (kind == "icons") {
                data.insert("directory", QFileInfo(data.value("path").toString()).absolutePath());
                IconTheme theme(id, QFileInfo(data.value("path").toString()).absolutePath());
                if (theme.load())
                    m_manager->addIconTheme(std::move(theme));
            } else if (kind == "background") {
                // Background manifests retain the existing Prism loader.
                QFileInfo manifest(data.value("path").toString());
                try {
                    m_manager->addCatPack(std::make_unique<ManagedBackground>(id, manifest));
                } catch (...) {
                    data.insert("unavailable", tr("Invalid background pack."));
                }
            }
            registered.append(id);
            m_components.append(data);
        }
        record.insert("id", directory);
        record.insert("components", registered);
        record.insert("directory", root.absolutePath());
        m_packages.append(record);
    }
    for (qsizetype i = 0; i < m_components.size(); ++i) {
        auto data = m_components[i].toObject();
        const auto kind = data.value("kind").toString();
        const auto parent = kind == "widgets" ? m_manager->getApplicationThemesFolder()
                            : kind == "icons" ? m_manager->getIconThemesFolder()
                                              : m_manager->getCatPacksFolder();
        auto location = data.value("directory").toString();
        if (kind == "background")
            location = QFileInfo(parent.filePath(data.value("id").toString())).isDir() ? parent.filePath(data.value("id").toString())
                                                                                       : data.value("path").toString();
        QFileInfo local(location);
        data.insert("removable", data.contains("packageId") || (!location.isEmpty() && !local.isSymLink() && local.exists() &&
                                                                local.dir().canonicalPath() == parent.canonicalPath()));
        if (data.value("kind") == "icons") {
            QSettings settings(QDir(data.value("directory").toString()).filePath("index.theme"), QSettings::IniFormat);
            auto names = settings.value("Icon Theme/Inherits").toStringList();
            if (names.size() == 1)
                names = names.first().split(',', Qt::SkipEmptyParts);
            data.insert("inherits", QJsonArray::fromStringList(names));
        }
        m_components[i] = data;
    }
    emit catalogChanged();
}

QJsonObject AppearanceService::packageConfiguration(const QString& id, QString& error) const
{
    QJsonObject package, bundle;
    const auto selected = component(id);
    const auto packageId = selected.isEmpty() ? id : selected.value("packageId").toString();
    for (const auto& value : m_packages)
        if (value.toObject().value("id") == packageId)
            package = value.toObject();
    if (package.isEmpty()) {
        error = tr("Theme package is unavailable.");
        return {};
    }
    if (selected.value("kind") == "bundle")
        bundle = selected;
    else
        for (const auto& value : m_components) {
            const auto entry = value.toObject();
            if (entry.value("packageId") == packageId && entry.value("kind") == "bundle") {
                if (!bundle.isEmpty()) {
                    error = tr("This package contains several global themes. Select a specific global theme first.");
                    return {};
                }
                bundle = entry;
            }
        }
    const auto dependencies = bundle.value("dependencies").toArray();
    QSet<QString> related{ packageId };
    for (const auto& value : m_packages) {
        const auto candidate = value.toObject();
        for (const auto& dependency : dependencies) {
            const auto dep = dependency.toObject();
            if (dep.value("supported").toBool() && candidate.value("provider") == dep.value("provider") &&
                candidate.value("contentId") == dep.value("contentId"))
                related.insert(candidate.value("id").toString());
        }
    }
    auto configuration = m_requested.isEmpty() ? defaults() : m_requested;
    QJsonArray missing;
    QJsonObject choices;
    bool hasApplicationComponents = false;
    const auto references = bundle.value("references").toObject();
    for (const auto& kind : { "widgets", "palette", "icons", "background", "decoration" }) {
        QJsonArray candidates, matching;
        const auto reference = references.value(kind).toString();
        auto matches = [&](const QJsonObject& entry) {
            if (reference.isEmpty())
                return false;
            if (QString(kind) == "widgets" && reference.compare("kvantum", Qt::CaseInsensitive) == 0)
                return entry.value("engine").toString().compare("Kvantum", Qt::CaseInsensitive) == 0;
            for (const auto& value : { entry.value("name").toString(), entry.value("id").toString(),
                                       QFileInfo(entry.value("path").toString()).completeBaseName(),
                                       QFileInfo(entry.value("directory").toString()).fileName() })
                if (value.compare(reference, Qt::CaseInsensitive) == 0)
                    return true;
            // The shipped Breeze icon variants use Prism's historical IDs.
            return QString(kind) == "icons" &&
                   ((reference == "breeze" && entry.value("id") == "breeze_light") ||
                    (reference == "breeze-dark" && entry.value("id") == "breeze_dark"));
        };
        for (const auto& value : m_components) {
            const auto entry = value.toObject();
            if (entry.value("kind") != kind || entry.contains("unavailable"))
                continue;
            if (related.contains(entry.value("packageId").toString()))
                candidates.append(entry);
            if (matches(entry))
                matching.append(entry);
        }
        if (!reference.isEmpty()) {
            QJsonArray preferred;
            for (const auto& entry : candidates)
                if (matches(entry.toObject()))
                    preferred.append(entry);
            if (!preferred.isEmpty())
                candidates = preferred;
            else if (!matching.isEmpty())
                candidates = matching;
            else {
                // A KDE global theme may refer to a dependency by its desktop
                // name while the dependency package exposes a different
                // resource name (Nordic refers to candy-Nordic-green while
                // the store archive registers Nordic-folders).  Keep only
                // candidates coming from a separate related package here;
                // components belonging to the global package itself must not
                // satisfy an unresolved reference such as UnavailablePalette.
                QJsonArray dependencyCandidates;
                for (const auto& entry : candidates)
                    if (entry.toObject().value("packageId").toString() != packageId)
                        dependencyCandidates.append(entry);
                candidates = dependencyCandidates;
            }
        }
        if (selected.value("kind") == kind && !selected.contains("unavailable"))
            candidates = QJsonArray{ selected };
        if (candidates.isEmpty()) {
            if (!reference.isEmpty())
                missing.append(QJsonObject{ { "kind", kind }, { "name", reference } });
            continue;
        }
        hasApplicationComponents = true;
        if (candidates.size() > 1) {
            choices.insert(kind, candidates);
            continue;
        }
        const auto entry = candidates.first().toObject();
        const auto key = QString(kind) == "widgets" ? "theme" : QString(kind) == "decoration" ? "decorationTheme" : kind;
        configuration.insert(key, entry.value("id"));
        if (QString(kind) == "widgets" || QString(kind) == "palette") {
            configuration.insert("mode", "independent");
            configuration.insert("colorMode", "fixed");
        }
        if (QString(kind) == "decoration")
            configuration.insert("decoration", "luna");
    }
    if (!hasApplicationComponents && references.isEmpty())
        missing.append(QJsonObject{ { "kind", "application" }, { "name", tr("No supported application components are included.") } });
    const auto widgets = component(configuration.value("theme").toString());
    if (widgets.value("fixedColors").toBool() ||
        (widgets.value("format") == "qss-v2" && widgets.value("paletteBinding") != "semantic")) {
        if (references.contains("widgets") || related.contains(widgets.value("packageId").toString()))
            configuration.insert("palette", "");
        else if (!configuration.value("palette").toString().isEmpty())
            configuration.insert("theme", "Fusion");
    }
    return { { "configuration", configuration }, { "missing", missing }, { "choices", choices },
             { "dependencies", dependencies }, { "complete", missing.isEmpty() && choices.isEmpty() },
             { "packageId", packageId } };
}

QJsonObject AppearanceService::capabilities() const
{
    return { { "target", "classic" },
             { "formats", QJsonArray{ "prism", "qss", "kde-colors", "freedesktop", "luna-v2", "kvantum" } },
             { "engines", QJsonArray::fromStringList(ThemeStyle::engines()) },
             { "customDecoration", WindowChromeController::supported() },
             { "windowEffects", WindowChromeController::effectsSupported() },
             { "windowEffectMaterials", QJsonArray{ "theme", "none", "mica", "acrylic" } },
             { "windowEffectControls", QJsonObject{ { "opacity", QJsonObject{ { "minimum", 10 }, { "maximum", 100 } } },
                                                       { "blur", QJsonObject{ { "minimum", 0 }, { "maximum", 100 } } },
                                                       { "roundedCorners", true } } },
             { "paletteHotSwap", true },
             { "thirdPartyStyleRequiresRestart", true },
             { "scope", "application" } };
}

QJsonObject AppearanceService::state() const
{
    return { { "requested", m_requested },
             { "effective", m_effective },
             { "pending", m_pending.isEmpty() ? QJsonValue() : QJsonValue(m_pending) },
             { "lastKnownGood", m_lastGood },
             { "revision", m_revision },
             { "requiresRestart", !m_pending.isEmpty() },
             { "error", m_error },
             { "target", "classic" } };
}

bool AppearanceService::persist(QString& error, bool initializing) const
{
    QSaveFile file(QDir::current().absoluteFilePath("classic-appearance.json"));
    auto document = state();
    document.insert("schemaVersion", 1);
    document.insert("initializing", initializing);
    const auto bytes = QJsonDocument(document).toJson();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        error = tr("Could not save appearance settings: %1").arg(file.errorString());
        return false;
    }
    return true;
}

bool AppearanceService::validate(const QJsonObject& config, QString& error) const
{
    auto reject = [&](const QString& reason) {
        error = reason;
        return false;
    };
    const auto known = defaults();
    for (auto it = config.begin(); it != config.end(); ++it) {
        if (!known.contains(it.key()))
            return reject(tr("Unknown appearance option: %1").arg(it.key()));
        if (known.value(it.key()).isString() && !it.value().isString())
            return reject(tr("Invalid appearance option type."));
        if (known.value(it.key()).isBool() && !it.value().isBool())
            return reject(tr("Invalid appearance option type."));
        if (known.value(it.key()).isDouble() && !it.value().isDouble())
            return reject(tr("Invalid appearance option type."));
    }
    if (config.value("mode") != "system" && config.value("mode") != "independent")
        return reject(tr("Invalid appearance mode."));
    if (!m_manager->isValidApplicationTheme(config.value("theme").toString()))
        return reject(tr("Widget theme is unavailable."));
    if (!m_manager->isValidIconTheme(config.value("icons").toString()))
        return reject(tr("Icon theme is unavailable."));
    bool backgroundFound = false;
    for (auto background : m_manager->getValidCatPacks())
        backgroundFound |= background->id() == config.value("background").toString();
    if (!backgroundFound)
        return reject(tr("Background pack is unavailable."));
    if (config.value("colorMode") != "fixed" && config.value("colorMode") != "system")
        return reject(tr("Invalid color mode."));
    for (const auto& key : { "palette", "lightPalette", "darkPalette" }) {
        const auto id = config.value(key).toString();
        if (!id.isEmpty() && component(id).value("kind") != "palette")
            return reject(tr("The selected palette is unavailable."));
    }
    const auto widget = component(config.value("theme").toString());
    if (widget.contains("unavailable"))
        return reject(widget.value("unavailable").toString());
    if (config.value("mode") == "independent" && (!config.value("palette").toString().isEmpty() || config.value("colorMode") == "system") &&
        (widget.value("fixedColors").toBool() || (widget.value("format") == "qss-v2" && widget.value("paletteBinding") != "semantic")))
        return reject(tr("This widget theme uses fixed colors. Choose its own palette or a theme supporting independent colors."));
    if (config.value("colorMode") == "system" &&
        (config.value("lightPalette").toString().isEmpty() || config.value("darkPalette").toString().isEmpty()))
        return reject(tr("Choose both a light and a dark palette to follow system brightness."));
    if (config.value("decoration") != "system" && config.value("decoration") != "luna")
        return reject(tr("Invalid window decoration."));
    if (config.value("decoration") == "luna" && !WindowChromeController::supported())
        return reject(tr("Custom decoration is unavailable on this platform."));
    const auto decoration = config.value("decorationTheme").toString();
    if (!decoration.isEmpty() && component(decoration).value("kind") != "decoration")
        return reject(tr("Window decoration theme is unavailable."));
    const auto opacity = config.value("windowOpacity").toInt(-1);
    const auto blur = config.value("windowBlur").toInt(-1);
    if (opacity < 10 || opacity > 100)
        return reject(tr("Window opacity must be between 10 and 100."));
    if (blur < 0 || blur > 100)
        return reject(tr("Window blur must be between 0 and 100."));
    const auto material = config.value("windowMaterial").toString();
    if (material != "theme" && material != "none" && material != "mica" && material != "acrylic")
        return reject(tr("Invalid window material."));
    return true;
}

QJsonObject AppearanceService::resolvedConfiguration(const QJsonObject& input) const
{
    auto config = input;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (config.value("colorMode") == "system")
        config.insert("palette", config.value(qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark ? "darkPalette" : "lightPalette"));
#endif
    return config;
}

QJsonObject AppearanceService::resolvedEffects(const QJsonObject& config) const
{
    auto effects = component(config.value("theme").toString()).value("effects").toObject();
    const bool enabled = config.value("windowEffectsEnabled").toBool(true);
    const int opacity = qBound(10, config.value("windowOpacity").toInt(100), 100);
    const int blur = qBound(0, config.value("windowBlur").toInt(100), 100);
    const auto material = config.value("windowMaterial").toString("theme").toLower();
    if (!effects.contains("composite"))
        effects.insert("composite", true);
    if (!effects.contains("backdrop"))
        effects.insert("backdrop", "none");
    if (!effects.contains("translucentWindows"))
        effects.insert("translucentWindows", false);
    effects.insert("windowOpacity", opacity);
    effects.insert("blurStrength", blur);
    effects.insert("roundedCorners", enabled && config.value("windowRoundedCorners").toBool(true));
    if (!enabled) {
        effects.insert("composite", false);
        effects.insert("translucentWindows", false);
        effects.insert("blurring", false);
        effects.insert("blurTranslucent", false);
        effects.insert("backdrop", "none");
        return effects;
    }
    if (material != "theme") {
        effects.insert("composite", true);
        effects.insert("translucentWindows", material != "none" || opacity < 100);
        effects.insert("blurring", material == "mica" || (material == "acrylic" && blur > 0));
        effects.insert("blurTranslucent", material == "mica" || (material == "acrylic" && blur > 0));
        effects.insert("backdrop", material == "none" ? "none" : material);
    } else if (opacity < 100) {
        // Keep the theme's material, but make the user's opacity override
        // visible even for themes that only request plain translucency.
        effects.insert("translucentWindows", true);
    }
    return effects;
}

void AppearanceService::refreshWindowLayouts()
{
    // A style/palette change invalidates size hints, but Qt may defer the
    // corresponding LayoutRequest until the event loop returns. Recompute
    // every existing window explicitly so pages and dialogs do not remain in
    // their pre-theme geometry until the user resizes them.
    const auto refresh = [](QWidget* widget, const auto& self) -> void {
        if (!widget)
            return;
        widget->ensurePolished();
        widget->updateGeometry();
        if (auto layout = widget->layout()) {
            layout->invalidate();
            layout->activate();
        }
        for (auto child : widget->children())
            if (auto childWidget = qobject_cast<QWidget*>(child))
                self(childWidget, self);
        if (auto layout = widget->layout()) {
            layout->invalidate();
            layout->activate();
        }
        widget->update();
    };

    for (auto window : QApplication::topLevelWidgets()) {
        refresh(window, refresh);
        if (auto layout = window->layout())
            layout->activate();
    }
    if (m_chrome)
        m_chrome->relayoutWindows();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
    for (auto window : QApplication::topLevelWidgets()) {
        if (auto layout = window->layout())
            layout->activate();
        window->updateGeometry();
        window->update();
    }
}

void AppearanceService::queueWindowLayoutRefresh()
{
    refreshWindowLayouts();
    if (m_layoutRefreshQueued)
        return;
    m_layoutRefreshQueued = true;
    QTimer::singleShot(0, this, [this] {
        m_layoutRefreshQueued = false;
        refreshWindowLayouts();
    });
}

bool AppearanceService::render(const QJsonObject& requested, bool initial, QString& error)
{
    QScopedValueRollback<bool> rendering(m_rendering, true);
    auto config = resolvedConfiguration(requested);
    const bool system = config.value("mode") == "system";
    auto theme = m_manager->getTheme(system ? "system" : config.value("theme").toString());
    if (!theme) {
        error = tr("The widget theme is unavailable.");
        return false;
    }
    ThemePackage::Palette palette;
    const auto paletteId = config.value("palette").toString();
    const auto path = component(paletteId).value("path").toString();
    if (!system && !paletteId.isEmpty() && !ThemePackage::readColors(path, palette, error))
        return false;
    if (initial || m_effective.value("theme") != config.value("theme") || m_effective.value("mode") != config.value("mode"))
        theme->apply(initial);
    m_manager->setIconTheme(config.value("icons").toString());
    if (!system && !paletteId.isEmpty()) {
        // plasma-integration forwards this property to the window manager.
        qApp->setProperty("KDE_COLOR_SCHEME_PATH", config.value("systemTitlebarPalette").toBool() ? path : QString());
        QApplication::setPalette(palette.colors);
        m_manager->m_logColors = ITheme::defaultLogColors(palette.colors);
    } else {
        qApp->setProperty("KDE_COLOR_SCHEME_PATH", QString());
        if (!system && !initial)
            QApplication::setPalette(theme->colorScheme());
        m_manager->m_logColors = theme->logColorScheme();
    }
    if (system)
        m_manager->restoreSystemWindowColorsOnMac();
    else
        m_manager->setTitlebarColorOfAllWindowsOnMac(qApp->palette().window().color());
    // Kvantum themes describe compositor effects in their .kvconfig. Pass
    // those semantic hints to the cross-platform controller even when the
    // user keeps the native system title bar.
    m_chrome->setEffects(resolvedEffects(config));
    if (initial) {
        QJsonObject decoration;
        if (!config.value("decorationTheme").toString().isEmpty())
            decoration = component(config.value("decorationTheme").toString()).value("config").toObject();
        m_chrome->initialize(config.value("decoration") == "luna", decoration);
    }
    // Style sheets, palettes and native window attributes can each enqueue a
    // layout request. Run once now and once after those deferred requests so
    // switching back to a theme never needs a manual resize to recover.
    queueWindowLayoutRefresh();
    return true;
}

void AppearanceService::initialize()
{
    if (m_initialized)
        return;
    m_initialized = true;
    auto fallback = defaults();
    QFile file("classic-appearance.json");
    QJsonObject saved;
    if (file.open(QIODevice::ReadOnly) && file.size() < 1024 * 1024)
        saved = QJsonDocument::fromJson(file.readAll()).object();
    // Windows cannot atomically replace the file while this read handle is open.
    file.close();
    m_revision = saved.value("revision").toInteger();
    auto mergeDefaults = [&fallback](QJsonObject value) {
        for (auto it = fallback.begin(); it != fallback.end(); ++it)
            if (!value.contains(it.key()))
                value.insert(it.key(), it.value());
        return value;
    };
    m_lastGood = mergeDefaults(saved.value("lastKnownGood").toObject());
    m_requested = mergeDefaults(saved.value("requested").toObject());
    if (m_requested.isEmpty())
        m_requested = fallback;
    if (qEnvironmentVariableIntValue("LUNA_SAFE_THEME") == 1) {
        auto safe = fallback;
        safe.insert("mode", "system");
        safe.insert("theme", "system");
        safe.insert("icons", "pe_colored");
        safe.insert("background", "kitteh");
        QString error;
        render(safe, true, error);
        m_effective = safe;
        m_pending = m_requested;
        m_error = tr("Safe appearance mode is active. Saved themes are preserved for the next normal start.");
        return;
    }
    if (saved.value("initializing").toBool()) {
        m_requested = m_lastGood.isEmpty() ? fallback : m_lastGood;
        m_error = tr("The previous theme initialization did not finish. Restored the last usable appearance.");
    }
    QString error;
    if (!validate(m_requested, error)) {
        m_error = error;
        if (!m_lastGood.isEmpty() && validate(m_lastGood, error))
            m_requested = m_lastGood;
        else
            m_requested = fallback;
    }
    m_pending = {};
    if (!persist(error, true))
        m_error = error;
    if (!render(m_requested, true, error)) {
        m_error = error;
        m_requested = fallback;
        render(m_requested, true, error);
    }
    m_effective = m_requested;
    m_lastGood = m_effective;
    ++m_revision;
    if (!persist(error))
        m_error = error;
    auto settings = APPLICATION->settings();
    settings->set("ApplicationTheme", m_requested.value("mode") == "system" ? "system" : m_requested.value("theme").toString());
    settings->set("IconTheme", m_requested.value("icons").toString());
    settings->set("BackgroundCat", m_requested.value("background").toString());
    installSystemWatchers();
}

QJsonObject AppearanceService::apply(const QJsonObject& input, qint64 baseRevision, QString& error)
{
    if (baseRevision != m_revision) {
        error = tr("Appearance changed elsewhere. Reload the configuration before applying.");
        return {};
    }
    auto config = m_requested;
    for (auto it = input.begin(); it != input.end(); ++it)
        config.insert(it.key(), it.value());
    if (!validate(config, error))
        return {};
    const auto oldRequested = m_requested, oldPending = m_pending, oldEffective = m_effective, oldGood = m_lastGood;
    m_requested = config;
    bool restart = false;
    for (const auto& key : { "theme", "mode", "decoration", "decorationTheme" })
        restart |= config.value(key) != m_effective.value(key);
    m_pending = restart ? config : QJsonObject();
    ++m_revision;
    if (!persist(error)) {
        m_requested = oldRequested;
        m_pending = oldPending;
        --m_revision;
        return {};
    }
    if (restart) {
        // Window materials are independent from the Qt widget engine. Apply
        // them immediately so a theme's Mica/Acrylic/opacity conversion is
        // visible while the style itself waits for the normal safe restart.
        m_chrome->setEffects(resolvedEffects(config));
    }
    if (!restart) {
        if (!render(config, false, error)) {
            m_requested = oldRequested;
            m_pending = oldPending;
            --m_revision;
            QString recovery;
            render(m_effective, false, recovery);
            persist(recovery);
            return {};
        }
        m_effective = config;
        m_lastGood = config;
        if (!persist(error)) {
            m_requested = oldRequested;
            m_pending = oldPending;
            m_effective = oldEffective;
            m_lastGood = oldGood;
            --m_revision;
            QString recovery;
            render(m_effective, true, recovery);
            persist(recovery);
            return {};
        }
    }
    auto settings = APPLICATION->settings();
    settings->set("ApplicationTheme", config.value("mode") == "system" ? "system" : config.value("theme").toString());
    settings->set("IconTheme", config.value("icons").toString());
    settings->set("BackgroundCat", config.value("background").toString());
    if (oldEffective.value("background") != m_effective.value("background"))
        APPLICATION->currentCatChanged(0);
    m_error.clear();
    emit stateChanged();
    return state();
}

bool AppearanceService::undoPending(qint64 revision, QString& error)
{
    if (revision != m_revision) {
        error = tr("Appearance changed elsewhere. Reload first.");
        return false;
    }
    const auto previous = m_requested, pending = m_pending;
    m_requested = m_effective;
    m_pending = {};
    ++m_revision;
    if (!persist(error)) {
        m_requested = previous;
        m_pending = pending;
        --m_revision;
        return false;
    }
    m_chrome->setEffects(resolvedEffects(m_effective));
    auto settings = APPLICATION->settings();
    settings->set("ApplicationTheme", m_effective.value("mode") == "system" ? "system" : m_effective.value("theme").toString());
    settings->set("IconTheme", m_effective.value("icons").toString());
    settings->set("BackgroundCat", m_effective.value("background").toString());
    emit stateChanged();
    return true;
}

void AppearanceService::adoptLegacySettings()
{
    initialize();
    auto config = m_requested;
    auto settings = APPLICATION->settings();
    const auto theme = settings->get("ApplicationTheme").toString();
    if (!theme.isEmpty() && theme != config.value("theme").toString()) {
        config.insert("theme", theme);
        config.insert("palette", "");
        config.insert("colorMode", "fixed");
        config.insert("mode", theme == "system" ? "system" : "independent");
    }
    const auto icons = settings->get("IconTheme").toString();
    if (!icons.isEmpty())
        config.insert("icons", icons);
    config.insert("background", settings->get("BackgroundCat").toString());
    QString error;
    if (apply(config, m_revision, error).isEmpty() && !error.isEmpty()) {
        qWarning() << error;
        settings->set("ApplicationTheme", m_requested.value("mode") == "system" ? "system" : m_requested.value("theme").toString());
        settings->set("IconTheme", m_requested.value("icons").toString());
        settings->set("BackgroundCat", m_requested.value("background").toString());
        m_error = error;
        emit stateChanged();
    }
}

bool AppearanceService::isUsed(const QString& id) const
{
    if (containsReference(m_effective, id) || containsReference(m_requested, id) || containsReference(m_pending, id) ||
        containsReference(m_lastGood, id))
        return true;
    auto iconComponent = [this](const QString& key) {
        for (const auto& value : m_components) {
            const auto item = value.toObject();
            if (item.value("kind") == "icons" && item.value("id") == key)
                return item;
        }
        return QJsonObject();
    };
    if (iconComponent(id).isEmpty())
        return false;
    QStringList queue;
    QSet<QString> visited;
    for (const auto& config : { m_effective, m_requested, m_pending, m_lastGood })
        queue.append(config.value("icons").toString());
    while (!queue.isEmpty()) {
        const auto current = queue.takeFirst();
        if (visited.contains(current))
            continue;
        visited.insert(current);
        for (const auto& name : iconComponent(current).value("inherits").toArray())
            for (const auto& value : m_components) {
                const auto candidate = value.toObject();
                if (candidate.value("kind") != "icons")
                    continue;
                if (candidate.value("id") == name || QFileInfo(candidate.value("directory").toString()).fileName() == name.toString()) {
                    const auto dependency = candidate.value("id").toString();
                    if (dependency == id)
                        return true;
                    queue.append(dependency);
                }
            }
    }
    return false;
}

bool AppearanceService::removeLegacyComponent(const QString& kind, const QString& id, QString& error)
{
    QJsonObject data;
    for (const auto& value : m_components)
        if (value.toObject().value("id") == id && value.toObject().value("kind") == kind)
            data = value.toObject();
    if (data.isEmpty() || data.contains("packageId") || isUsed(id)) {
        error = tr("This component is built in, unavailable, or referenced by an appearance.");
        return false;
    }
    QDir parent = kind == "widgets" ? m_manager->getApplicationThemesFolder()
                  : kind == "icons" ? m_manager->getIconThemesFolder()
                                    : m_manager->getCatPacksFolder();
    QString path = data.value("directory").toString();
    if (kind == "background") {
        const auto folder = parent.filePath(id);
        path = QFileInfo(folder).isDir() ? folder : data.value("path").toString();
    }
    QFileInfo file(path);
    if (path.isEmpty() || !file.exists() || file.isSymLink() || file.dir().canonicalPath() != parent.canonicalPath()) {
        error = tr("Only themes in this data directory can be removed. Built-in and installation themes are read-only.");
        return false;
    }
    if (file.isDir() ? !QDir(file.absoluteFilePath()).removeRecursively() : !QFile::remove(file.absoluteFilePath())) {
        error = tr("Could not remove the custom component.");
        return false;
    }
    m_manager->refresh();
    return true;
}

bool AppearanceService::removePackage(const QString& id, QString& error)
{
    if (!QRegularExpression("^[a-f0-9]{64}$").match(id).hasMatch()) {
        error = tr("Invalid package ID.");
        return false;
    }
    for (const auto& value : m_components) {
        auto item = value.toObject();
        if (item.value("packageId").toString() == id && isUsed(item.value("id").toString())) {
            error = tr("Choose another appearance before removing this package.");
            return false;
        }
    }
    QDir root(packageDirectory());
    QLockFile lock(root.filePath(".install.lock"));
    if (!lock.tryLock(0)) {
        error = tr("Another theme transaction is in progress. Retry after it finishes.");
        return false;
    }
    auto path = QFileInfo(root.filePath(id));
    if (!path.isDir() || path.isSymLink() || !QDir(path.absoluteFilePath()).removeRecursively()) {
        error = tr("Could not remove the installed package.");
        return false;
    }
    m_manager->refresh();
    return true;
}

void AppearanceService::retainTask(const Task::Ptr& task)
{
    m_tasks.append(task);
    emit taskAdded(task.get());
    // Keep terminal snapshots for the lifetime of the application, with a bound.
    while (m_tasks.size() > 100 && m_tasks.first()->isFinished())
        m_tasks.removeFirst();
}

void AppearanceService::installSystemWatchers()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged, this, [this] {
        if (m_rendering || m_effective.value("colorMode") != "system")
            return;
        QString error;
        if (!render(m_effective, false, error)) {
            m_error = error;
            emit stateChanged();
        }
    });
#endif
}

void AppearanceService::preview(const QJsonObject& input, QWidget*, QString& error)
{
    initialize();
    auto config = m_requested;
    for (auto it = input.begin(); it != input.end(); ++it)
        config.insert(it.key(), it.value());
    if (!validate(config, error))
        return;
    config = resolvedConfiguration(config);
    auto theme = m_manager->getTheme(config.value("mode") == "system" ? "system" : config.value("theme").toString());
    QPalette palette = theme->colorScheme();
    if (config.value("mode") != "system" && !config.value("palette").toString().isEmpty()) {
        ThemePackage::Palette parsed;
        if (!ThemePackage::readColors(component(config.value("palette").toString()).value("path").toString(), parsed, error))
            return;
        palette = parsed.colors;
    }
    QJsonArray roles;
    for (auto group : { QPalette::Active, QPalette::Inactive, QPalette::Disabled }) {
        QJsonArray row;
        for (int role = 0; role < QPalette::NColorRoles; ++role)
            row.append(palette.color(group, QPalette::ColorRole(role)).name(QColor::HexArgb));
        roles.append(row);
    }
    QJsonObject paths;
    for (const auto& prefix :
         { QString("theme"),
           QString("luna") +
               QString::fromLatin1(QCryptographicHash::hash(theme->id().toUtf8(), QCryptographicHash::Sha256).toHex().left(16)) })
        paths.insert(prefix, QJsonArray::fromStringList(prefix == "theme" ? theme->searchPaths() : QDir::searchPaths(prefix)));
    QJsonObject specification{ { "palette", roles },
                               { "style", theme->qtTheme() },
                               { "widgetThemeFile", theme->widgetThemeFile() },
                               { "qss", theme->appStyleSheet() },
                               { "paths", paths },
                               { "decoration", config.value("decoration") },
                               { "name", theme->name() } };
    specification.insert("decorationConfig", component(config.value("decorationTheme").toString()).value("config"));
    specification.insert("effects", resolvedEffects(config));
    auto iconPaths = QIcon::themeSearchPaths();
    auto iconName = config.value("icons").toString();
    for (const auto& value : m_components) {
        const auto item = value.toObject();
        if (item.value("kind") != "icons")
            continue;
        const auto directory = item.value("directory").toString();
        if (!directory.isEmpty() && !directory.startsWith(":/")) {
            iconPaths.prepend(QFileInfo(directory).dir().absolutePath());
            if (item.value("id") == iconName)
                iconName = QFileInfo(directory).fileName();
        }
    }
    iconPaths.removeDuplicates();
    specification.insert("iconPaths", QJsonArray::fromStringList(iconPaths));
    specification.insert("icons", iconName);
    for (auto pack : m_manager->getValidCatPacks())
        if (pack->id() == config.value("background").toString())
            specification.insert("background", pack->path());
    const auto logColors = config.value("mode") != "system" && !config.value("palette").toString().isEmpty()
                               ? ITheme::defaultLogColors(palette)
                               : theme->logColorScheme();
    QJsonArray logs;
    for (const auto& sample : QList<QPair<MessageLevel, QString>>{ { MessageLevel::Info, "[Info] Preparing instance" },
                                                                   { MessageLevel::Warning, "[Warning] Example warning" },
                                                                   { MessageLevel::Error, "[Error] Example error" } })
        logs.append(
            QJsonObject{ { "text", sample.second },
                         { "foreground", logColors.foreground.value(sample.first, palette.text().color()).name(QColor::HexArgb) },
                         { "background", logColors.background.value(sample.first, palette.base().color()).name(QColor::HexArgb) } });
    specification.insert("logs", logs);
    auto file = new QTemporaryFile(QDir::tempPath() + "/luna-theme-preview-XXXXXX.json", this);
    if (!file->open()) {
        error = file->errorString();
        delete file;
        return;
    }
    const auto bytes = QJsonDocument(specification).toJson();
    if (file->write(bytes) != bytes.size()) {
        error = file->errorString();
        delete file;
        return;
    }
    file->close();
    auto process = new QProcess(this);
    auto environment = QProcessEnvironment::systemEnvironment();
#ifdef Q_OS_WIN
    environment.insert("QT_QPA_PLATFORM", "windows");
#else
    if (environment.value("QT_QPA_PLATFORM") == "offscreen" || environment.value("QT_QPA_PLATFORM") == "minimal")
        environment.remove("QT_QPA_PLATFORM");
#endif
    process->setProcessEnvironment(environment);
    connect(process, &QProcess::finished, this, [this, file, process](int code, QProcess::ExitStatus status) {
        if (status != QProcess::NormalExit || code != 0)
            emit previewFailed(tr("Theme preview exited unsuccessfully (%1). Your saved appearance is unchanged.").arg(code));
        file->deleteLater();
        process->deleteLater();
    });
    connect(process, &QProcess::errorOccurred, this, [this, file, process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            emit previewFailed(tr("Could not start theme preview: %1").arg(process->errorString()));
            file->deleteLater();
            process->deleteLater();
        }
    });
    process->start(QCoreApplication::applicationFilePath(), { "--theme-preview", file->fileName() });
}
