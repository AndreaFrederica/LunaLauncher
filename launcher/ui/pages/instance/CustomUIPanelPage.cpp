// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Luna Launcher - Minecraft Launcher
 */

#include "CustomUIPanelPage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLabel>
#include <QLocale>
#include <QPair>
#include <QPushButton>
#include <QScrollArea>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QMetaObject>
#include <QDebug>

#include "Application.h"
#include "FileSystem.h"
#include "minecraft/mod/tasks/LocalModParseTask.h"
#include "translations/TranslationsModel.h"
#include "ui/widgets/InfoFrame.h"

namespace {
QJsonValue jsonFromVariant(const QVariant& value)
{
    return QJsonValue::fromVariant(value);
}

QString valueToString(const QJsonValue& value)
{
    if (value.isString())
        return value.toString();
    return value.toVariant().toString();
}

bool valueToBool(const QJsonValue& value, bool fallback)
{
    if (value.isBool())
        return value.toBool();
    if (value.isDouble())
        return value.toInt() != 0;
    if (value.isString()) {
        auto lower = value.toString().trimmed().toLower();
        return lower == "1" || lower == "true" || lower == "yes" || lower == "on";
    }
    return fallback;
}

QStringList modFileNameFilters()
{
    return { "*.jar", "*.zip", "*.litemod", "*.nilmod", "*.jar.disabled", "*.zip.disabled", "*.litemod.disabled",
             "*.nilmod.disabled" };
}

Qt::CaseSensitivity pathCaseSensitivity()
{
#if defined(Q_OS_WIN)
    return Qt::CaseInsensitive;
#else
    return Qt::CaseSensitive;
#endif
}

bool isPathInRoot(const QString& candidatePath, const QString& rootPath)
{
    auto normalizedCandidate = QDir::cleanPath(candidatePath);
    auto normalizedRoot = QDir::cleanPath(rootPath);
    auto cs = pathCaseSensitivity();

    if (QString::compare(normalizedCandidate, normalizedRoot, cs) == 0) {
        return true;
    }

    auto withSep = normalizedRoot;
    if (!withSep.endsWith('/')) {
        withSep += '/';
    }
    return normalizedCandidate.startsWith(withSep, cs);
}
}  // namespace

CustomUIPanelPage::CustomUIPanelPage(MinecraftInstancePtr instance, QWidget* parent) : QWidget(parent), m_instance(std::move(instance))
{
    m_panelDisplayName = tr("Custom UI");
    m_languageId = currentLanguageId();

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    m_tabWidget = new QTabWidget(this);
    m_emptyLabel = new QLabel(this);
    m_emptyLabel->setWordWrap(true);
    m_emptyLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    rootLayout->addWidget(m_tabWidget);
    rootLayout->addWidget(m_emptyLabel);

    retranslate();
}

CustomUIPanelPage::~CustomUIPanelPage()
{
    cleanupJsRuntime();
}

QIcon CustomUIPanelPage::icon() const
{
    auto icon = QIcon::fromTheme("custom-commands");
    if (icon.isNull())
        icon = QIcon::fromTheme("instance-settings");
    return icon;
}

bool CustomUIPanelPage::apply()
{
    return saveState();
}

void CustomUIPanelPage::openedImpl()
{
    rebuildTabs();
}

void CustomUIPanelPage::retranslate()
{
    m_languageId = currentLanguageId();
    m_panelDisplayName = tr("Custom UI");
    m_emptyLabel->setText(
        tr("No custom tabs found.\n\nPlace JSON/JS files under:\n%1").arg(QDir::toNativeSeparators(customUiDir())));
    rebuildTabs();
}

void CustomUIPanelPage::rebuildTabs()
{
    clearTabs();
    m_languageId = currentLanguageId();
    m_panelDisplayName = tr("Custom UI");
    m_variantGroups = QJsonObject{};
    loadState();

    bool hasJs = false;
    QStringList files;
    QDir root(customUiDir());
    if (root.exists()) {
        QDirIterator it(root.absolutePath(), { "*.json", "*.js" }, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            auto path = it.next();
            QFileInfo info(path);
            if (info.fileName().compare("state.json", Qt::CaseInsensitive) == 0) {
                continue;
            }
            files.append(path);
            if (info.suffix().compare("js", Qt::CaseInsensitive) == 0) {
                hasJs = true;
            }
        }
    }

    files.sort(Qt::CaseInsensitive);

    if (hasJs && !initJsRuntime()) {
        qWarning() << "[CustomUIPanel] Failed to initialize JS runtime. JS tabs will be skipped.";
    }

    for (const auto& file : files) {
        QFileInfo info(file);
        if (info.suffix().compare("json", Qt::CaseInsensitive) == 0) {
            loadTabsFromJsonFile(file);
        } else if (info.suffix().compare("js", Qt::CaseInsensitive) == 0) {
            loadTabsFromJsFile(file);
        }
    }

    m_hasCustomTabs = m_tabWidget->count() > 0;
    m_tabWidget->setVisible(m_hasCustomTabs);
    m_emptyLabel->setVisible(!m_hasCustomTabs);
    if (m_container) {
        m_container->refreshContainer();
    }
}

void CustomUIPanelPage::clearTabs()
{
    while (m_tabWidget->count() > 0) {
        QWidget* widget = m_tabWidget->widget(0);
        m_tabWidget->removeTab(0);
        delete widget;
    }
}

void CustomUIPanelPage::loadState()
{
    m_state = QJsonObject{};

    QFile file(stateFilePath());
    if (!file.exists())
        return;
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QJsonParseError error{};
    auto doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "[CustomUIPanel] Failed to parse state file:" << stateFilePath() << error.errorString();
        return;
    }

    m_state = doc.object();
}

bool CustomUIPanelPage::saveState() const
{
    const auto path = stateFilePath();
    FS::ensureFolderPathExists(QFileInfo(path).absolutePath());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        qWarning() << "[CustomUIPanel] Failed to save state file:" << path;
        return false;
    }

    auto doc = QJsonDocument(m_state);
    file.write(doc.toJson(QJsonDocument::Indented));
    return true;
}

bool CustomUIPanelPage::initJsRuntime()
{
    cleanupJsRuntime();

    m_jsRuntime = JS_NewRuntime();
    if (!m_jsRuntime)
        return false;

    JS_SetMemoryLimit(m_jsRuntime, 16 * 1024 * 1024);
    JS_SetMaxStackSize(m_jsRuntime, 1024 * 1024);

    m_jsContext = JS_NewContext(m_jsRuntime);
    if (!m_jsContext) {
        cleanupJsRuntime();
        return false;
    }

    JS_SetContextOpaque(m_jsContext, this);

    JSValue global = JS_GetGlobalObject(m_jsContext);

    JSValue console = JS_NewObject(m_jsContext);
    JS_SetPropertyStr(m_jsContext, console, "log", JS_NewCFunction(m_jsContext, jsLog, "log", 1));
    JS_SetPropertyStr(m_jsContext, global, "console", console);

    JSValue launcher = JS_NewObject(m_jsContext);
    JS_SetPropertyStr(m_jsContext, launcher, "setModEnabled", JS_NewCFunction(m_jsContext, jsSetModEnabled, "setModEnabled", 2));
    JS_SetPropertyStr(m_jsContext, launcher, "activateVariant", JS_NewCFunction(m_jsContext, jsActivateVariant, "activateVariant", 2));
    JS_SetPropertyStr(m_jsContext, launcher, "getModState", JS_NewCFunction(m_jsContext, jsGetModState, "getModState", 1));
    JS_SetPropertyStr(m_jsContext, launcher, "isModEnabled", JS_NewCFunction(m_jsContext, jsIsModEnabled, "isModEnabled", 1));
    JS_SetPropertyStr(m_jsContext, launcher, "listMods", JS_NewCFunction(m_jsContext, jsListMods, "listMods", 1));
    JS_SetPropertyStr(m_jsContext, launcher, "getLanguageId", JS_NewCFunction(m_jsContext, jsGetLanguageId, "getLanguageId", 0));
    JS_SetPropertyStr(m_jsContext, launcher, "setState", JS_NewCFunction(m_jsContext, jsSetState, "setState", 2));
    JS_SetPropertyStr(m_jsContext, launcher, "getState", JS_NewCFunction(m_jsContext, jsGetState, "getState", 1));
    JS_SetPropertyStr(m_jsContext, launcher, "saveState", JS_NewCFunction(m_jsContext, jsSaveState, "saveState", 0));

    JSValue fs = JS_NewObject(m_jsContext);
    JS_SetPropertyStr(m_jsContext, fs, "exists", JS_NewCFunction(m_jsContext, jsFsExists, "exists", 1));
    JS_SetPropertyStr(m_jsContext, fs, "readFile", JS_NewCFunction(m_jsContext, jsFsReadFile, "readFile", 1));
    JS_SetPropertyStr(m_jsContext, fs, "writeFile", JS_NewCFunction(m_jsContext, jsFsWriteFile, "writeFile", 2));
    JS_SetPropertyStr(m_jsContext, fs, "readdir", JS_NewCFunction(m_jsContext, jsFsReaddir, "readdir", 1));
    JS_SetPropertyStr(m_jsContext, fs, "mkdir", JS_NewCFunction(m_jsContext, jsFsMkdir, "mkdir", 2));
    JS_SetPropertyStr(m_jsContext, fs, "rm", JS_NewCFunction(m_jsContext, jsFsRm, "rm", 2));
    JS_SetPropertyStr(m_jsContext, launcher, "fs", fs);

    JS_SetPropertyStr(m_jsContext, global, "launcher", launcher);

    JS_FreeValue(m_jsContext, global);
    return true;
}

void CustomUIPanelPage::cleanupJsRuntime()
{
    if (m_jsContext) {
        JS_FreeContext(m_jsContext);
        m_jsContext = nullptr;
    }
    if (m_jsRuntime) {
        JS_FreeRuntime(m_jsRuntime);
        m_jsRuntime = nullptr;
    }
}

bool CustomUIPanelPage::loadTabsFromJsonFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[CustomUIPanel] Failed to open JSON file:" << filePath;
        return false;
    }

    QJsonParseError error{};
    auto doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "[CustomUIPanel] Failed to parse JSON file:" << filePath << error.errorString();
        return false;
    }

    if (doc.isObject()) {
        appendTabsFromValue(doc.object(), filePath);
        return true;
    }
    if (doc.isArray()) {
        appendTabsFromValue(doc.array(), filePath);
        return true;
    }

    return false;
}

bool CustomUIPanelPage::evalJsWithTimeout(JSValue& outResult, const QByteArray& code, const QByteArray& fileName, int timeoutMs)
{
    if (!m_jsRuntime || !m_jsContext)
        return false;

    m_jsDeadline.deadlineMs = QDateTime::currentMSecsSinceEpoch() + timeoutMs;
    JS_SetInterruptHandler(m_jsRuntime, jsInterruptHandler, &m_jsDeadline);
    outResult = JS_Eval(m_jsContext, code.constData(), code.size(), fileName.constData(), JS_EVAL_TYPE_GLOBAL);
    JS_SetInterruptHandler(m_jsRuntime, nullptr, nullptr);

    if (JS_IsException(outResult)) {
        JSValue ex = JS_GetException(m_jsContext);
        const char* str = JS_ToCString(m_jsContext, ex);
        qWarning() << "[CustomUIPanel] JS eval failed:" << fileName << (str ? str : "unknown error");
        if (str)
            JS_FreeCString(m_jsContext, str);
        JS_FreeValue(m_jsContext, ex);
        JS_FreeValue(m_jsContext, outResult);
        outResult = JS_UNDEFINED;
        return false;
    }
    return true;
}

bool CustomUIPanelPage::callJsWithTimeout(JSValue& outResult, JSValue function, JSValue thisObj, int argc, JSValue* argv, int timeoutMs)
{
    if (!m_jsRuntime || !m_jsContext)
        return false;

    m_jsDeadline.deadlineMs = QDateTime::currentMSecsSinceEpoch() + timeoutMs;
    JS_SetInterruptHandler(m_jsRuntime, jsInterruptHandler, &m_jsDeadline);
    outResult = JS_Call(m_jsContext, function, thisObj, argc, const_cast<JSValueConst*>(argv));
    JS_SetInterruptHandler(m_jsRuntime, nullptr, nullptr);

    if (JS_IsException(outResult)) {
        JSValue ex = JS_GetException(m_jsContext);
        const char* str = JS_ToCString(m_jsContext, ex);
        qWarning() << "[CustomUIPanel] JS call failed:" << (str ? str : "unknown error");
        if (str)
            JS_FreeCString(m_jsContext, str);
        JS_FreeValue(m_jsContext, ex);
        JS_FreeValue(m_jsContext, outResult);
        outResult = JS_UNDEFINED;
        return false;
    }
    return true;
}

bool CustomUIPanelPage::loadTabsFromJsFile(const QString& filePath)
{
    if (!m_jsContext) {
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[CustomUIPanel] Failed to open JS file:" << filePath;
        return false;
    }

    JSValue global = JS_GetGlobalObject(m_jsContext);
    JS_SetPropertyStr(m_jsContext, global, "tabs", JS_UNDEFINED);

    JSValue evalResult = JS_UNDEFINED;
    auto jsCode = file.readAll();
    if (!evalJsWithTimeout(evalResult, jsCode, filePath.toUtf8(), 1000)) {
        JS_FreeValue(m_jsContext, global);
        return false;
    }
    JS_FreeValue(m_jsContext, evalResult);

    JSValue getTabs = JS_GetPropertyStr(m_jsContext, global, "getTabs");
    if (JS_IsFunction(m_jsContext, getTabs)) {
        JSValue state = jsonValueToJs(m_state);
        QJsonObject contextObj;
        contextObj.insert("languageId", m_languageId);
        contextObj.insert("instanceRoot", QDir::toNativeSeparators(m_instance->instanceRoot()));
        contextObj.insert("gameRoot", QDir::toNativeSeparators(m_instance->gameRoot()));
        JSValue context = jsonValueToJs(contextObj);
        JSValue argv[] = { state, context };
        JSValue result = JS_UNDEFINED;
        if (callJsWithTimeout(result, getTabs, global, 2, argv, 800)) {
            auto tabsJson = jsValueToJson(result);
            if (!tabsJson.isUndefined() && !tabsJson.isNull()) {
                appendTabsFromValue(tabsJson, filePath);
            }
            JS_FreeValue(m_jsContext, result);
        }
        JS_FreeValue(m_jsContext, context);
        JS_FreeValue(m_jsContext, state);
    } else {
        JSValue tabs = JS_GetPropertyStr(m_jsContext, global, "tabs");
        if (!JS_IsUndefined(tabs) && !JS_IsNull(tabs)) {
            appendTabsFromValue(jsValueToJson(tabs), filePath);
        }
        JS_FreeValue(m_jsContext, tabs);
    }

    JS_FreeValue(m_jsContext, getTabs);
    JS_FreeValue(m_jsContext, global);
    return true;
}

void CustomUIPanelPage::appendTabsFromValue(const QJsonValue& value, const QString& sourceTag)
{
    if (value.isObject()) {
        auto obj = value.toObject();
        applyPanelMetadata(obj);
        mergeVariantGroups(obj.value("variantGroups").toObject());

        if (obj.contains("tabs") && obj.value("tabs").isArray()) {
            for (const auto& tabVal : obj.value("tabs").toArray()) {
                if (tabVal.isObject())
                    appendSingleTab(tabVal.toObject(), sourceTag);
            }
            return;
        }

        if (obj.contains("controls") && obj.value("controls").isArray()) {
            appendSingleTab(obj, sourceTag);
        }
        return;
    }

    if (value.isArray()) {
        for (const auto& tabVal : value.toArray()) {
            if (tabVal.isObject())
                appendSingleTab(tabVal.toObject(), sourceTag);
        }
    }
}

void CustomUIPanelPage::appendSingleTab(const QJsonObject& tab, const QString& sourceTag)
{
    auto controlsVal = tab.value("controls");
    if (!controlsVal.isArray() || controlsVal.toArray().isEmpty())
        return;

    mergeVariantGroups(tab.value("variantGroups").toObject());

    QString title = resolveLocalizedField(tab, "title");
    if (title.isEmpty()) {
        title = resolveLocalizedField(tab, "name");
    }
    if (title.isEmpty()) {
        title = QFileInfo(sourceTag).baseName();
    }

    auto* content = createTabContent(tab);
    if (!content)
        return;

    m_tabWidget->addTab(content, title);
}

void CustomUIPanelPage::mergeVariantGroups(const QJsonObject& groupsObj)
{
    for (auto it = groupsObj.begin(); it != groupsObj.end(); ++it) {
        if (!it.value().isObject())
            continue;
        m_variantGroups.insert(it.key(), it.value().toObject());
    }
}

QWidget* CustomUIPanelPage::createTabContent(const QJsonObject& tabObj)
{
    auto* scroll = new QScrollArea(m_tabWidget);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* page = new QWidget(scroll);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(10);

    auto controls = tabObj.value("controls").toArray();
    for (const auto& controlVal : controls) {
        if (!controlVal.isObject())
            continue;
        addControl(page, layout, controlVal.toObject());
    }
    layout->addStretch(1);

    scroll->setWidget(page);
    return scroll;
}

void CustomUIPanelPage::addControl(QWidget* parent, QVBoxLayout* layout, const QJsonObject& control)
{
    const auto controlId = control.value("id").toString();
    const auto type = control.value("type").toString().trimmed().toLower();

    if (type == "label" || type == "text") {
        auto* label = new QLabel(resolveLocalizedField(control, "text", resolveLocalizedField(control, "label")), parent);
        label->setWordWrap(true);
        auto tooltip = resolveLocalizedField(control, "tooltip");
        if (!tooltip.isEmpty()) {
            label->setToolTip(tooltip);
        }
        layout->addWidget(label);
        return;
    }

    if (type == "separator") {
        auto* line = new QFrame(parent);
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Sunken);
        layout->addWidget(line);
        return;
    }

    if (type == "toggle" || type == "checkbox") {
        auto* check = new QCheckBox(resolveLocalizedField(control, "label", resolveLocalizedField(control, "text")), parent);
        auto tooltip = resolveLocalizedField(control, "tooltip");
        if (!tooltip.isEmpty()) {
            check->setToolTip(tooltip);
        }
        auto initial = m_state.contains(controlId) ? m_state.value(controlId) : control.value("default");
        bool checked = valueToBool(initial, false);
        check->setChecked(checked);
        if (!controlId.isEmpty()) {
            m_state.insert(controlId, checked);
        }

        connect(check, &QCheckBox::toggled, this, [this, control, controlId](bool value) {
            if (!controlId.isEmpty()) {
                m_state.insert(controlId, value);
            }
            onControlTriggered(control, QJsonValue(value));
        });

        layout->addWidget(check);
        return;
    }

    if (type == "select" || type == "combo") {
        auto* row = new QWidget(parent);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(8);

        auto* label = new QLabel(resolveLocalizedField(control, "label", controlId), row);
        auto* combo = new QComboBox(row);
        auto tooltip = resolveLocalizedField(control, "tooltip");
        if (!tooltip.isEmpty()) {
            combo->setToolTip(tooltip);
        }

        rowLayout->addWidget(label);
        rowLayout->addWidget(combo, 1);

        bool hasDescription = false;
        auto options = control.value("options").toArray();
        for (const auto& optVal : options) {
            if (optVal.isObject()) {
                auto obj = optVal.toObject();
                auto value = obj.value("value");
                auto optionLabel = resolveLocalizedField(obj, "label", valueToString(value));
                combo->addItem(optionLabel, value.toVariant());
                auto description = resolveLocalizedField(obj, "description");
                if (!description.isEmpty()) {
                    auto index = combo->count() - 1;
                    combo->setItemData(index, description, Qt::UserRole + 1);
                    combo->setItemData(index, description, Qt::ToolTipRole);
                    hasDescription = true;
                }
            } else if (optVal.isString()) {
                combo->addItem(optVal.toString(), optVal.toString());
            }
        }

        auto initial = m_state.contains(controlId) ? m_state.value(controlId) : control.value("default");
        auto initialStr = valueToString(initial);
        if (!initialStr.isEmpty()) {
            for (int i = 0; i < combo->count(); i++) {
                if (combo->itemData(i).toString().compare(initialStr, Qt::CaseInsensitive) == 0) {
                    combo->setCurrentIndex(i);
                    break;
                }
            }
        }

        if (!controlId.isEmpty()) {
            m_state.insert(controlId, jsonFromVariant(combo->currentData()));
        }

        connect(combo, &QComboBox::currentIndexChanged, this, [this, combo, control, controlId](int) {
            auto value = jsonFromVariant(combo->currentData());
            if (!controlId.isEmpty()) {
                m_state.insert(controlId, value);
            }
            onControlTriggered(control, value);
        });

        layout->addWidget(row);
        if (hasDescription) {
            auto* desc = new QLabel(parent);
            desc->setWordWrap(true);
            desc->setProperty("lunaOptionDescription", true);
            auto updateDescription = [combo, desc]() {
                auto text = combo->currentData(Qt::UserRole + 1).toString();
                desc->setText(text);
                desc->setVisible(!text.isEmpty());
            };
            updateDescription();
            connect(combo, &QComboBox::currentIndexChanged, this, [updateDescription](int) { updateDescription(); });
            layout->addWidget(desc);
        }
        return;
    }

    if (type == "button") {
        auto text = resolveLocalizedField(control, "text", resolveLocalizedField(control, "label", tr("Run")));
        auto* button = new QPushButton(text, parent);
        auto tooltip = resolveLocalizedField(control, "tooltip");
        if (!tooltip.isEmpty()) {
            button->setToolTip(tooltip);
        }
        connect(button, &QPushButton::clicked, this, [this, control] { onControlTriggered(control, QJsonValue(true)); });
        layout->addWidget(button);
        return;
    }

    if (type == "moddetails" || type == "moddetail" || type == "modinfo" || type == "mod-info") {
        auto* wrapper = new QWidget(parent);
        auto* wrapperLayout = new QVBoxLayout(wrapper);
        wrapperLayout->setContentsMargins(0, 0, 0, 0);
        wrapperLayout->setSpacing(8);

        auto heading = resolveLocalizedField(control, "label", resolveLocalizedField(control, "title"));
        if (!heading.isEmpty()) {
            auto* headingLabel = new QLabel(heading, wrapper);
            headingLabel->setWordWrap(true);
            auto tooltip = resolveLocalizedField(control, "tooltip");
            if (!tooltip.isEmpty()) {
                headingLabel->setToolTip(tooltip);
            }
            wrapperLayout->addWidget(headingLabel);
        }

        QJsonArray items = control.value("items").toArray();
        if (items.isEmpty()) {
            auto legacyMods = control.value("mods");
            if (legacyMods.isArray()) {
                for (const auto& modVal : legacyMods.toArray()) {
                    if (!modVal.isString()) {
                        continue;
                    }
                    QJsonObject item;
                    item.insert("mod", modVal.toString());
                    items.append(item);
                }
            } else if (legacyMods.isString()) {
                QJsonObject item;
                item.insert("mod", legacyMods.toString());
                items.append(item);
            }
        }

        int renderedCount = 0;
        for (const auto& itemVal : items) {
            if (!itemVal.isObject()) {
                continue;
            }
            auto item = itemVal.toObject();
            auto* frame = new InfoFrame(wrapper);

            const auto modQuery = item.value("mod").toString();
            const auto modState = modQuery.isEmpty() ? QJsonObject{} : getModState(modQuery);
            const auto found = modState.value("found").toBool(false);
            const auto enabled = modState.value("enabled").toBool(false);
            const auto foundFileName = modState.value("fileName").toString();
            const auto foundPath = modState.value("path").toString();

            bool usedRealModMetadata = false;
            QString resolvedDescription;
            if (found && !foundPath.isEmpty()) {
                QFileInfo modFileInfo(foundPath);
                if (modFileInfo.exists() && modFileInfo.isFile()) {
                    Mod parsedMod(modFileInfo);
                    if (ModUtils::process(parsedMod, ModUtils::ProcessingLevel::Full)) {
                        frame->updateWithMod(parsedMod);
                        resolvedDescription = parsedMod.description().trimmed();
                        usedRealModMetadata = true;
                    }
                }
            }

            if (!usedRealModMetadata) {
                auto name = resolveLocalizedField(item, "name", resolveLocalizedField(item, "label"));
                if (name.isEmpty()) {
                    name = foundFileName.isEmpty() ? modQuery : foundFileName;
                }
                if (name.isEmpty()) {
                    continue;
                }

                QString title = name.toHtmlEscaped();
                auto homepage = resolveLocalizedField(item, "homepage");
                auto homepageUrl = QUrl(homepage);
                if (homepageUrl.isValid() && !homepageUrl.scheme().isEmpty()) {
                    title = QString("<a href=\"%1\">%2</a>").arg(QString::fromUtf8(homepageUrl.toEncoded()), title);
                }

                auto authors = resolveLocalizedField(item, "authors");
                if (!authors.isEmpty()) {
                    title += tr(" by %1").arg(authors.toHtmlEscaped());
                }
                frame->setName(title);

                auto license = resolveLocalizedField(item, "license");
                if (!license.isEmpty()) {
                    frame->setLicense(tr("License: %1").arg(license));
                } else {
                    frame->setLicense();
                }

                auto issueTracker = resolveLocalizedField(item, "issueTracker");
                auto issueUrl = QUrl(issueTracker);
                if (issueUrl.isValid() && !issueUrl.scheme().isEmpty()) {
                    frame->setIssueTracker(
                        tr("Report issues to: <a href=\"%1\">%1</a>").arg(QString::fromUtf8(issueUrl.toEncoded())));
                } else {
                    frame->setIssueTracker();
                }
            }

            QStringList descriptionLines;
            if (!resolvedDescription.isEmpty()) {
                descriptionLines.append(resolvedDescription);
            } else {
                auto fallbackDescription = resolveLocalizedField(item, "description");
                if (!fallbackDescription.isEmpty()) {
                    descriptionLines.append(fallbackDescription);
                }
            }

            if (!modQuery.isEmpty()) {
                QString statusLine;
                if (!found) {
                    statusLine = tr("Status: not found");
                } else if (enabled) {
                    statusLine = tr("Status: enabled");
                } else {
                    statusLine = tr("Status: disabled");
                }
                descriptionLines.append(statusLine);
            }

            auto warning = resolveLocalizedField(item, "warning");
            if (!warning.isEmpty()) {
                descriptionLines.append(tr("Warning: %1").arg(warning));
            }

            if (found && !foundFileName.isEmpty() && valueToBool(item.value("showFile"), true)) {
                descriptionLines.append(tr("File: %1").arg(foundFileName));
            }

            if (!descriptionLines.isEmpty()) {
                frame->setDescription(descriptionLines.join('\n'));
            }

            wrapperLayout->addWidget(frame);
            renderedCount++;
        }

        if (renderedCount == 0) {
            auto* empty = new QLabel(tr("No matching mods found."), wrapper);
            empty->setWordWrap(true);
            wrapperLayout->addWidget(empty);
        }

        layout->addWidget(wrapper);
        return;
    }

    auto* unknown = new QLabel(tr("Unknown control type: %1").arg(type), parent);
    unknown->setWordWrap(true);
    layout->addWidget(unknown);
}

void CustomUIPanelPage::onControlTriggered(const QJsonObject& control, const QJsonValue& value)
{
    const auto controlId = control.value("id").toString();

    auto action = control.value("action");
    if (!action.isUndefined() && !action.isNull()) {
        executeActionValue(action, controlId, value);
    } else {
        if (control.contains("mod")) {
            setModEnabledByName(control.value("mod").toString(), valueToBool(value, false));
        }
        if (control.contains("variantGroup")) {
            activateVariant(control.value("variantGroup").toString(), valueToString(value));
        }
    }

    auto handlerName = control.value("onChange").toString();
    if (handlerName.isEmpty()) {
        handlerName = control.value("onClick").toString();
    }
    if (!handlerName.isEmpty()) {
        invokeHandler(handlerName, controlId, value);
    }

    if (control.value("saveOnChange").toBool(false)) {
        saveState();
    }
}

bool CustomUIPanelPage::executeActionValue(const QJsonValue& action, const QString& controlId, const QJsonValue& value)
{
    if (action.isObject()) {
        return executeActionObject(action.toObject(), controlId, value);
    }

    if (action.isArray()) {
        bool ok = true;
        for (const auto& item : action.toArray()) {
            if (!item.isObject())
                continue;
            ok = executeActionObject(item.toObject(), controlId, value) && ok;
        }
        return ok;
    }

    if (action.isString()) {
        return invokeHandler(action.toString(), controlId, value);
    }

    return false;
}

bool CustomUIPanelPage::executeActionObject(const QJsonObject& action, const QString& controlId, const QJsonValue& value)
{
    const auto name = action.value("action").toString();
    if (name == "setModEnabled") {
        auto mod = action.value("mod").toString();
        auto enabled = action.contains("enabled") ? valueToBool(action.value("enabled"), false) : valueToBool(value, false);
        return setModEnabledByName(mod, enabled);
    }

    if (name == "activateVariant") {
        auto group = action.value("group").toString();
        auto option = action.contains("value") ? valueToString(action.value("value")) : valueToString(value);
        return activateVariant(group, option);
    }

    if (name == "setState") {
        auto key = action.value("key").toString(controlId);
        if (key.isEmpty())
            return false;
        auto stateVal = action.contains("value") ? action.value("value") : value;
        m_state.insert(key, stateVal);
        return true;
    }

    if (name == "saveState") {
        return saveState();
    }

    if (name == "reloadTabs" || name == "refreshTabs") {
        QMetaObject::invokeMethod(this, [this]() { rebuildTabs(); }, Qt::QueuedConnection);
        return true;
    }

    if (name == "runHandler" || name == "callHandler") {
        return invokeHandler(action.value("handler").toString(), controlId, value);
    }

    qWarning() << "[CustomUIPanel] Unknown action:" << name;
    return false;
}

bool CustomUIPanelPage::invokeHandler(const QString& handlerName, const QString& controlId, const QJsonValue& value)
{
    if (!m_jsContext || handlerName.isEmpty())
        return false;

    JSValue global = JS_GetGlobalObject(m_jsContext);
    JSValue handler = JS_GetPropertyStr(m_jsContext, global, handlerName.toUtf8().constData());
    if (!JS_IsFunction(m_jsContext, handler)) {
        JS_FreeValue(m_jsContext, handler);
        JS_FreeValue(m_jsContext, global);
        return false;
    }

    QJsonObject event;
    event.insert("controlId", controlId);
    event.insert("value", value);
    event.insert("state", m_state);

    JSValue arg = jsonValueToJs(event);
    JSValue argv[] = { arg };
    JSValue result = JS_UNDEFINED;
    bool ok = callJsWithTimeout(result, handler, global, 1, argv, 700);

    JS_FreeValue(m_jsContext, arg);
    JS_FreeValue(m_jsContext, handler);
    JS_FreeValue(m_jsContext, global);

    if (!ok)
        return false;

    auto maybeAction = jsValueToJson(result);
    JS_FreeValue(m_jsContext, result);
    if (!maybeAction.isUndefined() && !maybeAction.isNull()) {
        executeActionValue(maybeAction, controlId, value);
    }

    return true;
}

QString CustomUIPanelPage::currentLanguageId() const
{
    QString out;
    if (APPLICATION && APPLICATION->translations()) {
        out = APPLICATION->translations()->selectedLanguage();
    }
    if (out.isEmpty()) {
        out = QLocale::system().name();
    }
    if (out.isEmpty()) {
        out = "en_US";
    }
    return out;
}

QString CustomUIPanelPage::resolveLocalizedValue(const QJsonValue& value, const QString& fallback) const
{
    if (value.isString()) {
        return value.toString();
    }
    if (!value.isObject()) {
        return fallback;
    }

    auto map = value.toObject();
    if (map.isEmpty()) {
        return fallback;
    }

    const auto key = m_languageId;
    const auto keyNorm = key.toLower().replace('-', '_');
    const auto keyShort = keyNorm.split('_').value(0);

    auto lookup = [&](const QString& k) -> QString {
        if (k.isEmpty()) {
            return {};
        }
        if (map.contains(k) && map.value(k).isString()) {
            return map.value(k).toString();
        }
        for (auto it = map.begin(); it != map.end(); ++it) {
            if (!it.value().isString()) {
                continue;
            }
            if (it.key().toLower().replace('-', '_') == k.toLower().replace('-', '_')) {
                return it.value().toString();
            }
        }
        return {};
    };

    QString localized = lookup(key);
    if (localized.isEmpty()) {
        localized = lookup(keyNorm);
    }
    if (localized.isEmpty()) {
        localized = lookup(keyShort);
    }
    if (localized.isEmpty()) {
        localized = lookup("en_US");
    }
    if (localized.isEmpty()) {
        localized = lookup("en");
    }
    if (localized.isEmpty()) {
        for (auto it = map.begin(); it != map.end(); ++it) {
            if (it.value().isString()) {
                localized = it.value().toString();
                break;
            }
        }
    }

    return localized.isEmpty() ? fallback : localized;
}

QString CustomUIPanelPage::resolveLocalizedField(const QJsonObject& obj, const QString& key, const QString& fallback) const
{
    if (obj.contains(key)) {
        auto resolved = resolveLocalizedValue(obj.value(key), fallback);
        if (!resolved.isEmpty()) {
            return resolved;
        }
    }
    const auto i18nKey = key + "_i18n";
    if (obj.contains(i18nKey)) {
        auto resolved = resolveLocalizedValue(obj.value(i18nKey), fallback);
        if (!resolved.isEmpty()) {
            return resolved;
        }
    }
    return fallback;
}

void CustomUIPanelPage::applyPanelMetadata(const QJsonObject& obj)
{
    QString name = resolveLocalizedField(obj, "pageName");
    if (name.isEmpty()) {
        name = resolveLocalizedField(obj, "panelName");
    }
    if (name.isEmpty()) {
        auto panelObj = obj.value("panel").toObject();
        name = resolveLocalizedField(panelObj, "name");
        if (name.isEmpty()) {
            name = resolveLocalizedField(panelObj, "displayName");
        }
    }
    if (!name.isEmpty()) {
        m_panelDisplayName = name;
    }
}

QString CustomUIPanelPage::customUiDir() const
{
    return FS::PathCombine(m_instance->instanceRoot(), "lunaui");
}

QString CustomUIPanelPage::stateFilePath() const
{
    return FS::PathCombine(customUiDir(), "state.json");
}

QString CustomUIPanelPage::normalizeModIdentity(QString modName) const
{
    modName = modName.trimmed();
    if (modName.endsWith(".disabled", Qt::CaseInsensitive)) {
        modName.chop(9);
    }
    return modName;
}

QStringList CustomUIPanelPage::toStringList(const QJsonValue& value) const
{
    QStringList out;
    if (value.isString()) {
        out.append(value.toString());
        return out;
    }
    if (value.isArray()) {
        for (const auto& item : value.toArray()) {
            if (item.isString()) {
                out.append(item.toString());
            }
        }
    }
    return out;
}

bool CustomUIPanelPage::findModFile(const QString& modName, QString& pathOut, bool& enabledOut) const
{
    const auto normalized = normalizeModIdentity(modName);
    const auto dirs = QStringList{ m_instance->modsRoot(), m_instance->coreModsDir(), m_instance->nilModsDir() };

    for (const auto& dirPath : dirs) {
        QDir dir(dirPath);
        if (!dir.exists())
            continue;

        const auto exactEnabled = dir.filePath(normalized);
        const auto exactDisabled = exactEnabled + ".disabled";
        if (QFile::exists(exactEnabled)) {
            pathOut = exactEnabled;
            enabledOut = true;
            return true;
        }
        if (QFile::exists(exactDisabled)) {
            pathOut = exactDisabled;
            enabledOut = false;
            return true;
        }

        QStringList matches;
        const auto entries = dir.entryList(modFileNameFilters(), QDir::Files, QDir::Name);

        for (const auto& entry : entries) {
            auto base = normalizeModIdentity(entry);
            if (base.compare(normalized, Qt::CaseInsensitive) == 0 || base.startsWith(normalized + "-", Qt::CaseInsensitive)) {
                matches.append(dir.filePath(entry));
            }
        }

        if (matches.size() == 1) {
            pathOut = matches[0];
            enabledOut = !pathOut.endsWith(".disabled", Qt::CaseInsensitive);
            return true;
        }
    }
    return false;
}

QJsonObject CustomUIPanelPage::getModState(const QString& modName) const
{
    QJsonObject out;
    out.insert("query", modName);
    out.insert("normalized", normalizeModIdentity(modName));

    QString path;
    bool enabled = false;
    if (!findModFile(modName, path, enabled)) {
        out.insert("found", false);
        return out;
    }

    QFileInfo fileInfo(path);
    out.insert("found", true);
    out.insert("enabled", enabled);
    out.insert("fileName", fileInfo.fileName());
    out.insert("path", QDir::toNativeSeparators(fileInfo.absoluteFilePath()));
    out.insert("normalized", normalizeModIdentity(fileInfo.fileName()));
    return out;
}

QJsonArray CustomUIPanelPage::listManagedMods(const QString& filter) const
{
    QJsonArray out;
    const auto filterLower = filter.trimmed().toLower();

    const QList<QPair<QString, QString>> dirs = { { "mods", m_instance->modsRoot() },
                                                  { "coremods", m_instance->coreModsDir() },
                                                  { "nilmods", m_instance->nilModsDir() } };

    for (const auto& scopeAndPath : dirs) {
        QDir dir(scopeAndPath.second);
        if (!dir.exists())
            continue;

        const auto entries = dir.entryInfoList(modFileNameFilters(), QDir::Files, QDir::Name);
        for (const auto& entry : entries) {
            const auto fileName = entry.fileName();
            const auto normalized = normalizeModIdentity(fileName);
            if (!filterLower.isEmpty()) {
                const auto nameLower = fileName.toLower();
                const auto normalizedLower = normalized.toLower();
                if (!nameLower.contains(filterLower) && !normalizedLower.contains(filterLower)) {
                    continue;
                }
            }

            QJsonObject item;
            item.insert("scope", scopeAndPath.first);
            item.insert("fileName", fileName);
            item.insert("normalized", normalized);
            item.insert("enabled", !fileName.endsWith(".disabled", Qt::CaseInsensitive));
            item.insert("path", QDir::toNativeSeparators(entry.absoluteFilePath()));
            out.append(item);
        }
    }
    return out;
}

bool CustomUIPanelPage::resolveFsPath(const QString& userPath, QString& absolutePathOut) const
{
    auto trimmed = userPath.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }

    QDir root(m_instance->instanceRoot());
    const auto rootAbs = QDir::cleanPath(root.absolutePath());
    QString candidate = QDir::isAbsolutePath(trimmed) ? QDir::cleanPath(trimmed) : QDir::cleanPath(root.filePath(trimmed));

    if (!isPathInRoot(candidate, rootAbs)) {
        return false;
    }

    absolutePathOut = candidate;
    return true;
}

bool CustomUIPanelPage::setModEnabledByName(const QString& modName, bool enabled)
{
    if (modName.isEmpty())
        return false;
    if (m_instance->isRunning()) {
        qWarning() << "[CustomUIPanel] Refused to change mod state while game is running:" << modName;
        return false;
    }

    QString currentPath;
    bool currentlyEnabled = false;
    if (!findModFile(modName, currentPath, currentlyEnabled)) {
        qWarning() << "[CustomUIPanel] Mod file not found for" << modName;
        return false;
    }

    if (currentlyEnabled == enabled)
        return true;

    QString newPath = currentPath;
    if (enabled) {
        if (!newPath.endsWith(".disabled", Qt::CaseInsensitive))
            return false;
        newPath.chop(9);
        if (QFile::exists(newPath)) {
            qWarning() << "[CustomUIPanel] Could not enable mod, target already exists:" << newPath;
            return false;
        }
    } else {
        newPath += ".disabled";
        if (QFile::exists(newPath)) {
            newPath = FS::getUniqueResourceName(newPath);
        }
    }

    QFile file(currentPath);
    if (!file.rename(newPath)) {
        qWarning() << "[CustomUIPanel] Failed to rename mod file from" << currentPath << "to" << newPath;
        return false;
    }

    return true;
}

bool CustomUIPanelPage::activateVariant(const QString& groupName, const QString& optionName)
{
    if (groupName.isEmpty() || optionName.isEmpty())
        return false;

    auto groupValue = m_variantGroups.value(groupName);
    if (!groupValue.isObject()) {
        qWarning() << "[CustomUIPanel] Variant group not found:" << groupName;
        return false;
    }

    auto groupObj = groupValue.toObject();
    if (!groupObj.contains(optionName)) {
        qWarning() << "[CustomUIPanel] Variant option not found:" << groupName << optionName;
        return false;
    }

    bool ok = true;
    for (auto it = groupObj.begin(); it != groupObj.end(); ++it) {
        QStringList mods;
        if (it.value().isObject()) {
            mods = toStringList(it.value().toObject().value("mods"));
        } else {
            mods = toStringList(it.value());
        }

        const bool shouldEnable = (it.key() == optionName);
        for (const auto& mod : mods) {
            ok = setModEnabledByName(mod, shouldEnable) && ok;
        }
    }

    m_state.insert(groupName, optionName);
    return ok;
}

QJsonValue CustomUIPanelPage::jsValueToJson(JSValue value) const
{
    if (!m_jsContext || JS_IsUndefined(value))
        return {};

    JSValue stringified = JS_JSONStringify(m_jsContext, value, JS_UNDEFINED, JS_UNDEFINED);
    if (JS_IsException(stringified))
        return {};

    const char* raw = JS_ToCString(m_jsContext, stringified);
    if (!raw) {
        JS_FreeValue(m_jsContext, stringified);
        return {};
    }

    QByteArray encoded(raw);
    JS_FreeCString(m_jsContext, raw);
    JS_FreeValue(m_jsContext, stringified);

    QByteArray wrapped = "{\"v\":";
    wrapped += encoded;
    wrapped += "}";

    QJsonParseError error{};
    auto doc = QJsonDocument::fromJson(wrapped, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return {};

    return doc.object().value("v");
}

JSValue CustomUIPanelPage::jsonValueToJs(const QJsonValue& value) const
{
    if (!m_jsContext || value.isUndefined())
        return JS_UNDEFINED;

    QJsonObject wrapper;
    wrapper.insert("v", value);
    auto encoded = QJsonDocument(wrapper).toJson(QJsonDocument::Compact);

    JSValue root = JS_ParseJSON(m_jsContext, encoded.constData(), encoded.size(), "<json>");
    if (JS_IsException(root))
        return JS_UNDEFINED;

    JSValue out = JS_GetPropertyStr(m_jsContext, root, "v");
    JS_FreeValue(m_jsContext, root);
    return out;
}

int CustomUIPanelPage::jsInterruptHandler(JSRuntime* rt, void* opaque)
{
    Q_UNUSED(rt);
    auto* deadline = static_cast<JsDeadline*>(opaque);
    return QDateTime::currentMSecsSinceEpoch() > deadline->deadlineMs;
}

JSValue CustomUIPanelPage::jsLog(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv)
{
    Q_UNUSED(thisVal);
    QStringList parts;
    for (int i = 0; i < argc; i++) {
        const char* str = JS_ToCString(ctx, argv[i]);
        if (str) {
            parts.append(QString::fromUtf8(str));
            JS_FreeCString(ctx, str);
        }
    }
    qDebug() << "[CustomUIPanel:JS]" << parts.join(' ');
    return JS_UNDEFINED;
}

JSValue CustomUIPanelPage::jsSetModEnabled(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv)
{
    Q_UNUSED(thisVal);
    auto* self = static_cast<CustomUIPanelPage*>(JS_GetContextOpaque(ctx));
    if (!self || argc < 2)
        return JS_NewBool(ctx, false);

    const char* mod = JS_ToCString(ctx, argv[0]);
    if (!mod)
        return JS_NewBool(ctx, false);

    bool enabled = JS_ToBool(ctx, argv[1]) > 0;
    bool ok = self->setModEnabledByName(QString::fromUtf8(mod), enabled);
    JS_FreeCString(ctx, mod);
    return JS_NewBool(ctx, ok);
}

JSValue CustomUIPanelPage::jsActivateVariant(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv)
{
    Q_UNUSED(thisVal);
    auto* self = static_cast<CustomUIPanelPage*>(JS_GetContextOpaque(ctx));
    if (!self || argc < 2)
        return JS_NewBool(ctx, false);

    const char* group = JS_ToCString(ctx, argv[0]);
    const char* option = JS_ToCString(ctx, argv[1]);
    if (!group || !option) {
        if (group)
            JS_FreeCString(ctx, group);
        if (option)
            JS_FreeCString(ctx, option);
        return JS_NewBool(ctx, false);
    }

    bool ok = self->activateVariant(QString::fromUtf8(group), QString::fromUtf8(option));
    JS_FreeCString(ctx, group);
    JS_FreeCString(ctx, option);
    return JS_NewBool(ctx, ok);
}

JSValue CustomUIPanelPage::jsGetModState(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv)
{
    Q_UNUSED(thisVal);
    auto* self = static_cast<CustomUIPanelPage*>(JS_GetContextOpaque(ctx));
    if (!self || argc < 1)
        return JS_UNDEFINED;

    const char* mod = JS_ToCString(ctx, argv[0]);
    if (!mod)
        return JS_UNDEFINED;

    auto result = self->jsonValueToJs(self->getModState(QString::fromUtf8(mod)));
    JS_FreeCString(ctx, mod);
    return result;
}

JSValue CustomUIPanelPage::jsIsModEnabled(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv)
{
    Q_UNUSED(thisVal);
    auto* self = static_cast<CustomUIPanelPage*>(JS_GetContextOpaque(ctx));
    if (!self || argc < 1)
        return JS_NULL;

    const char* mod = JS_ToCString(ctx, argv[0]);
    if (!mod)
        return JS_NULL;

    auto state = self->getModState(QString::fromUtf8(mod));
    JS_FreeCString(ctx, mod);
    if (!state.value("found").toBool(false))
        return JS_NULL;
    return JS_NewBool(ctx, state.value("enabled").toBool(false));
}

JSValue CustomUIPanelPage::jsListMods(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv)
{
    Q_UNUSED(thisVal);
    auto* self = static_cast<CustomUIPanelPage*>(JS_GetContextOpaque(ctx));
    if (!self)
        return JS_UNDEFINED;

    QString filter;
    if (argc >= 1 && !JS_IsUndefined(argv[0]) && !JS_IsNull(argv[0])) {
        const char* raw = JS_ToCString(ctx, argv[0]);
        if (raw) {
            filter = QString::fromUtf8(raw);
            JS_FreeCString(ctx, raw);
        }
    }

    return self->jsonValueToJs(self->listManagedMods(filter));
}

JSValue CustomUIPanelPage::jsGetLanguageId(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv)
{
    Q_UNUSED(thisVal);
    Q_UNUSED(argc);
    Q_UNUSED(argv);
    auto* self = static_cast<CustomUIPanelPage*>(JS_GetContextOpaque(ctx));
    if (!self)
        return JS_UNDEFINED;
    auto lang = self->currentLanguageId().toUtf8();
    return JS_NewStringLen(ctx, lang.constData(), static_cast<size_t>(lang.size()));
}

JSValue CustomUIPanelPage::jsSetState(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv)
{
    Q_UNUSED(thisVal);
    auto* self = static_cast<CustomUIPanelPage*>(JS_GetContextOpaque(ctx));
    if (!self || argc < 2)
        return JS_NewBool(ctx, false);

    const char* key = JS_ToCString(ctx, argv[0]);
    if (!key)
        return JS_NewBool(ctx, false);

    JSValue dup = JS_DupValue(ctx, argv[1]);
    auto value = self->jsValueToJson(dup);
    JS_FreeValue(ctx, dup);
    self->m_state.insert(QString::fromUtf8(key), value);
    JS_FreeCString(ctx, key);
    return JS_NewBool(ctx, true);
}

JSValue CustomUIPanelPage::jsGetState(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv)
{
    Q_UNUSED(thisVal);
    auto* self = static_cast<CustomUIPanelPage*>(JS_GetContextOpaque(ctx));
    if (!self || argc < 1)
        return JS_UNDEFINED;

    const char* key = JS_ToCString(ctx, argv[0]);
    if (!key)
        return JS_UNDEFINED;

    auto result = self->jsonValueToJs(self->m_state.value(QString::fromUtf8(key)));
    JS_FreeCString(ctx, key);
    return result;
}

JSValue CustomUIPanelPage::jsSaveState(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv)
{
    Q_UNUSED(thisVal);
    Q_UNUSED(argc);
    Q_UNUSED(argv);
    auto* self = static_cast<CustomUIPanelPage*>(JS_GetContextOpaque(ctx));
    if (!self)
        return JS_NewBool(ctx, false);
    return JS_NewBool(ctx, self->saveState());
}

JSValue CustomUIPanelPage::jsFsExists(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv)
{
    Q_UNUSED(thisVal);
    auto* self = static_cast<CustomUIPanelPage*>(JS_GetContextOpaque(ctx));
    if (!self || argc < 1) {
        return JS_NewBool(ctx, false);
    }
    const char* raw = JS_ToCString(ctx, argv[0]);
    if (!raw) {
        return JS_NewBool(ctx, false);
    }
    QString resolved;
    bool ok = self->resolveFsPath(QString::fromUtf8(raw), resolved);
    JS_FreeCString(ctx, raw);
    if (!ok) {
        return JS_NewBool(ctx, false);
    }
    return JS_NewBool(ctx, QFileInfo::exists(resolved));
}

JSValue CustomUIPanelPage::jsFsReadFile(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv)
{
    Q_UNUSED(thisVal);
    auto* self = static_cast<CustomUIPanelPage*>(JS_GetContextOpaque(ctx));
    if (!self || argc < 1) {
        return JS_UNDEFINED;
    }
    const char* raw = JS_ToCString(ctx, argv[0]);
    if (!raw) {
        return JS_UNDEFINED;
    }
    QString resolved;
    bool ok = self->resolveFsPath(QString::fromUtf8(raw), resolved);
    JS_FreeCString(ctx, raw);
    if (!ok) {
        return JS_UNDEFINED;
    }

    QFile file(resolved);
    if (!file.open(QIODevice::ReadOnly)) {
        return JS_UNDEFINED;
    }
    auto data = file.readAll();
    return JS_NewStringLen(ctx, data.constData(), static_cast<size_t>(data.size()));
}

JSValue CustomUIPanelPage::jsFsWriteFile(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv)
{
    Q_UNUSED(thisVal);
    auto* self = static_cast<CustomUIPanelPage*>(JS_GetContextOpaque(ctx));
    if (!self || argc < 2) {
        return JS_NewBool(ctx, false);
    }

    const char* rawPath = JS_ToCString(ctx, argv[0]);
    const char* rawData = JS_ToCString(ctx, argv[1]);
    if (!rawPath || !rawData) {
        if (rawPath)
            JS_FreeCString(ctx, rawPath);
        if (rawData)
            JS_FreeCString(ctx, rawData);
        return JS_NewBool(ctx, false);
    }

    QString resolved;
    bool ok = self->resolveFsPath(QString::fromUtf8(rawPath), resolved);
    auto data = QByteArray(rawData);
    JS_FreeCString(ctx, rawPath);
    JS_FreeCString(ctx, rawData);
    if (!ok) {
        return JS_NewBool(ctx, false);
    }

    FS::ensureFolderPathExists(QFileInfo(resolved).absolutePath());
    QFile file(resolved);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return JS_NewBool(ctx, false);
    }
    return JS_NewBool(ctx, file.write(data) == data.size());
}

JSValue CustomUIPanelPage::jsFsReaddir(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv)
{
    Q_UNUSED(thisVal);
    auto* self = static_cast<CustomUIPanelPage*>(JS_GetContextOpaque(ctx));
    if (!self || argc < 1) {
        return JS_UNDEFINED;
    }

    const char* rawPath = JS_ToCString(ctx, argv[0]);
    if (!rawPath) {
        return JS_UNDEFINED;
    }
    QString resolved;
    bool ok = self->resolveFsPath(QString::fromUtf8(rawPath), resolved);
    JS_FreeCString(ctx, rawPath);
    if (!ok) {
        return JS_UNDEFINED;
    }

    QDir dir(resolved);
    if (!dir.exists()) {
        return self->jsonValueToJs(QJsonArray{});
    }

    QJsonArray out;
    const auto entries = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden, QDir::Name);
    for (const auto& entry : entries) {
        QJsonObject item;
        item.insert("name", entry.fileName());
        item.insert("path", QDir::toNativeSeparators(entry.absoluteFilePath()));
        item.insert("isFile", entry.isFile());
        item.insert("isDir", entry.isDir());
        item.insert("size", static_cast<qint64>(entry.size()));
        out.append(item);
    }
    return self->jsonValueToJs(out);
}

JSValue CustomUIPanelPage::jsFsMkdir(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv)
{
    Q_UNUSED(thisVal);
    auto* self = static_cast<CustomUIPanelPage*>(JS_GetContextOpaque(ctx));
    if (!self || argc < 1) {
        return JS_NewBool(ctx, false);
    }

    const char* rawPath = JS_ToCString(ctx, argv[0]);
    if (!rawPath) {
        return JS_NewBool(ctx, false);
    }
    QString resolved;
    bool ok = self->resolveFsPath(QString::fromUtf8(rawPath), resolved);
    JS_FreeCString(ctx, rawPath);
    if (!ok) {
        return JS_NewBool(ctx, false);
    }

    bool recursive = true;
    if (argc >= 2 && !JS_IsUndefined(argv[1]) && !JS_IsNull(argv[1])) {
        recursive = JS_ToBool(ctx, argv[1]) > 0;
    }

    QDir dir;
    if (recursive) {
        return JS_NewBool(ctx, dir.mkpath(resolved));
    }
    return JS_NewBool(ctx, dir.mkdir(resolved));
}

JSValue CustomUIPanelPage::jsFsRm(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv)
{
    Q_UNUSED(thisVal);
    auto* self = static_cast<CustomUIPanelPage*>(JS_GetContextOpaque(ctx));
    if (!self || argc < 1) {
        return JS_NewBool(ctx, false);
    }

    const char* rawPath = JS_ToCString(ctx, argv[0]);
    if (!rawPath) {
        return JS_NewBool(ctx, false);
    }
    QString resolved;
    bool ok = self->resolveFsPath(QString::fromUtf8(rawPath), resolved);
    JS_FreeCString(ctx, rawPath);
    if (!ok) {
        return JS_NewBool(ctx, false);
    }

    bool recursive = false;
    if (argc >= 2 && !JS_IsUndefined(argv[1]) && !JS_IsNull(argv[1])) {
        recursive = JS_ToBool(ctx, argv[1]) > 0;
    }

    QFileInfo info(resolved);
    if (!info.exists()) {
        return JS_NewBool(ctx, true);
    }

    if (info.isDir()) {
        if (recursive) {
            return JS_NewBool(ctx, FS::deletePath(resolved));
        }
        return JS_NewBool(ctx, QDir().rmdir(resolved));
    }

    QFile file(resolved);
    return JS_NewBool(ctx, file.remove());
}
