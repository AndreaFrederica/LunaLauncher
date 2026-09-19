// SPDX-License-Identifier: GPL-3.0-only
// Adapted from ui/pages/instance/CustomUIPanelPage.cpp; no widget construction.
#include "CustomUiRuntime.h"
#include "Application.h"
#include "DesktopServices.h"
#include "FileSystem.h"
#include "translations/TranslationsModel.h"
#include "minecraft/auth/AccountList.h"
#include "minecraft/auth/MinecraftAccount.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDirIterator>
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QLocale>
#include <QScopedValueRollback>
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

void CustomUiRuntime::loadState()
{
    m_state = QJsonObject{};
    m_stateRevision.clear();

    QString safePath;
    if (!resolveFsPath(stateFilePath(), safePath)) return;
    QFile file(safePath);
    if (!file.exists())
        return;
    if (!file.open(QIODevice::ReadOnly))
        return;

    QJsonParseError error{};
    if (file.size() > 4 * 1024 * 1024) return;
    const auto bytes = file.readAll();
    m_stateRevision = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
    auto doc = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "[CustomUIPanel] Failed to parse state file:" << stateFilePath() << error.errorString();
        return;
    }

    m_state = doc.object();
}

bool CustomUiRuntime::saveState() const
{
    QString path;
    if (!resolveFsPath(stateFilePath(), path)) return false;
    QFile existing(path);
    if (existing.exists()) {
        if (!existing.open(QIODevice::ReadOnly) || existing.size() > 4 * 1024 * 1024 ||
            QCryptographicHash::hash(existing.readAll(), QCryptographicHash::Sha256) != m_stateRevision) return false;
    } else if (!m_stateRevision.isEmpty()) return false;
    existing.close();
    FS::ensureFolderPathExists(QFileInfo(path).absolutePath());

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "[CustomUIPanel] Failed to save state file:" << path;
        return false;
    }

    auto doc = QJsonDocument(m_state);
    const auto bytes = doc.toJson(QJsonDocument::Indented);
    if (bytes.size() > 4 * 1024 * 1024 || file.write(bytes) != bytes.size() || !file.commit()) return false;
    m_stateRevision = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
    return true;
}

bool CustomUiRuntime::initJsRuntime()
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
    JS_SetPropertyStr(m_jsContext, launcher, "getInstanceSetting",
                      JS_NewCFunction(m_jsContext, jsGetInstanceSetting, "getInstanceSetting", 1));
    JS_SetPropertyStr(m_jsContext, launcher, "setInstanceSetting",
                      JS_NewCFunction(m_jsContext, jsSetInstanceSetting, "setInstanceSetting", 2));
    JS_SetPropertyStr(m_jsContext, launcher, "openFolder", JS_NewCFunction(m_jsContext, jsOpenFolder, "openFolder", 2));

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

void CustomUiRuntime::cleanupJsRuntime()
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

bool CustomUiRuntime::loadTabsFromJsonFile(const QString& filePath)
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

bool CustomUiRuntime::evalJsWithTimeout(JSValue& outResult, const QByteArray& code, const QByteArray& fileName, int timeoutMs)
{
    QScopedValueRollback<int> depth(m_jsDepth, m_jsDepth + 1);
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

bool CustomUiRuntime::callJsWithTimeout(JSValue& outResult, JSValue function, JSValue thisObj, int argc, JSValue* argv, int timeoutMs)
{
    QScopedValueRollback<int> depth(m_jsDepth, m_jsDepth + 1);
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

bool CustomUiRuntime::loadTabsFromJsFile(const QString& filePath)
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

void CustomUiRuntime::appendTabsFromValue(const QJsonValue& value, const QString& sourceTag)
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

void CustomUiRuntime::mergeVariantGroups(const QJsonObject& groupsObj)
{
    for (auto it = groupsObj.begin(); it != groupsObj.end(); ++it) {
        if (!it.value().isObject())
            continue;
        m_variantGroups.insert(it.key(), it.value().toObject());
    }
}

bool CustomUiRuntime::executeActionValue(const QJsonValue& action, const QString& controlId, const QJsonValue& value)
{
    if (m_actionDepth >= 32) return false;
    QScopedValueRollback<int> depth(m_actionDepth, m_actionDepth + 1);
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

bool CustomUiRuntime::executeActionObject(const QJsonObject& action, const QString& controlId, const QJsonValue& value)
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

    if (name == "setInstanceSetting") {
        const auto setting = action.value("setting").toString();
        if (setting.isEmpty() || !m_instance->settings()->getSetting(setting))
            return false;
        const auto settingValue = action.contains("value") ? action.value("value") : value;
        return m_instance->settings()->set(setting, settingValue.toVariant());
    }

    if (name == "openFolder") {
        const auto path = action.value("path").toString();
        const auto create = action.value("create").toBool(false);
        return openInstanceFolder(path, create);
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

bool CustomUiRuntime::invokeHandler(const QString& handlerName, const QString& controlId, const QJsonValue& value)
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
        return executeActionValue(maybeAction, controlId, value);
    }

    return true;
}

QString CustomUiRuntime::currentLanguageId() const
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

QString CustomUiRuntime::resolveLocalizedValue(const QJsonValue& value, const QString& fallback) const
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

QString CustomUiRuntime::resolveLocalizedField(const QJsonObject& obj, const QString& key, const QString& fallback) const
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

void CustomUiRuntime::applyPanelMetadata(const QJsonObject& obj)
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

QString CustomUiRuntime::customUiDir() const
{
    return FS::PathCombine(m_instance->instanceRoot(), "lunaui");
}

QString CustomUiRuntime::stateFilePath() const
{
    return FS::PathCombine(customUiDir(), "state.json");
}

QString CustomUiRuntime::normalizeModIdentity(QString modName) const
{
    modName = modName.trimmed();
    if (modName.endsWith(".disabled", Qt::CaseInsensitive)) {
        modName.chop(9);
    }
    return modName;
}

QStringList CustomUiRuntime::toStringList(const QJsonValue& value) const
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

bool CustomUiRuntime::findModFile(const QString& modName, QString& pathOut, bool& enabledOut) const
{
    const auto normalized = normalizeModIdentity(modName);
    if (normalized.isEmpty() || normalized.contains('/') || normalized.contains('\\') || normalized.contains(':') || normalized.contains("..")) return false;
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

QJsonObject CustomUiRuntime::getModState(const QString& modName) const
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

QJsonArray CustomUiRuntime::listManagedMods(const QString& filter) const
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

bool CustomUiRuntime::resolveFsPath(const QString& userPath, QString& absolutePathOut) const
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

    auto ancestor = FS::nearestExistentAncestor(candidate);
    const auto canonicalRoot = QFileInfo(rootAbs).canonicalFilePath();
    if (canonicalRoot.isEmpty() || ancestor.isEmpty() || !isPathInRoot(QFileInfo(ancestor).canonicalFilePath(), canonicalRoot)) return false;
    absolutePathOut = candidate;
    return true;
}

bool CustomUiRuntime::openInstanceFolder(const QString& userPath, bool create)
{
    QString resolved;
    if (!resolveFsPath(userPath, resolved))
        return false;

    const auto rootCanonical = QFileInfo(m_instance->instanceRoot()).canonicalFilePath();
    const auto ancestor = FS::nearestExistentAncestor(resolved);
    const auto ancestorCanonical = ancestor.isEmpty() ? QString() : QFileInfo(ancestor).canonicalFilePath();
    if (rootCanonical.isEmpty() || ancestorCanonical.isEmpty() || !isPathInRoot(ancestorCanonical, rootCanonical))
        return false;

    QFileInfo target(resolved);
    if (!target.exists()) {
        if (!create || !FS::ensureFolderPathExists(resolved))
            return false;
        target.refresh();
    }

    const auto targetCanonical = target.canonicalFilePath();
    if (!target.isDir() || targetCanonical.isEmpty() || !isPathInRoot(targetCanonical, rootCanonical))
        return false;
    return DesktopServices::openPath(targetCanonical);
}

bool CustomUiRuntime::setModEnabledByName(const QString& modName, bool enabled)
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

bool CustomUiRuntime::activateVariant(const QString& groupName, const QString& optionName)
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

QJsonValue CustomUiRuntime::jsValueToJson(JSValue value) const
{
    if (!m_jsContext || JS_IsUndefined(value))
        return {};

    if (m_jsDepth == 0) {
        m_jsDeadline.deadlineMs = QDateTime::currentMSecsSinceEpoch() + 700;
        JS_SetInterruptHandler(m_jsRuntime, jsInterruptHandler, &m_jsDeadline);
    }
    JSValue stringified = JS_JSONStringify(m_jsContext, value, JS_UNDEFINED, JS_UNDEFINED);
    if (m_jsDepth == 0) JS_SetInterruptHandler(m_jsRuntime, nullptr, nullptr);
    if (JS_IsException(stringified)) {
        JS_FreeValue(m_jsContext, JS_GetException(m_jsContext));
        return {};
    }

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

JSValue CustomUiRuntime::jsonValueToJs(const QJsonValue& value) const
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

int CustomUiRuntime::jsInterruptHandler(JSRuntime* rt, void* opaque)
{
    Q_UNUSED(rt);
    auto* deadline = static_cast<JsDeadline*>(opaque);
    return QDateTime::currentMSecsSinceEpoch() > deadline->deadlineMs;
}

JSValue CustomUiRuntime::jsLog(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv)
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

JSValue CustomUiRuntime::jsSetModEnabled(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv)
{
    Q_UNUSED(thisVal);
    auto* self = static_cast<CustomUiRuntime*>(JS_GetContextOpaque(ctx));
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

JSValue CustomUiRuntime::jsActivateVariant(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv)
{
    Q_UNUSED(thisVal);
    auto* self = static_cast<CustomUiRuntime*>(JS_GetContextOpaque(ctx));
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

JSValue CustomUiRuntime::jsGetModState(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv)
{
    Q_UNUSED(thisVal);
    auto* self = static_cast<CustomUiRuntime*>(JS_GetContextOpaque(ctx));
    if (!self || argc < 1)
        return JS_UNDEFINED;

    const char* mod = JS_ToCString(ctx, argv[0]);
    if (!mod)
        return JS_UNDEFINED;

    auto result = self->jsonValueToJs(self->getModState(QString::fromUtf8(mod)));
    JS_FreeCString(ctx, mod);
    return result;
}

JSValue CustomUiRuntime::jsIsModEnabled(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv)
{
    Q_UNUSED(thisVal);
    auto* self = static_cast<CustomUiRuntime*>(JS_GetContextOpaque(ctx));
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

JSValue CustomUiRuntime::jsListMods(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv)
{
    Q_UNUSED(thisVal);
    auto* self = static_cast<CustomUiRuntime*>(JS_GetContextOpaque(ctx));
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

JSValue CustomUiRuntime::jsGetLanguageId(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv)
{
    Q_UNUSED(thisVal);
    Q_UNUSED(argc);
    Q_UNUSED(argv);
    auto* self = static_cast<CustomUiRuntime*>(JS_GetContextOpaque(ctx));
    if (!self)
        return JS_UNDEFINED;
    auto lang = self->currentLanguageId().toUtf8();
    return JS_NewStringLen(ctx, lang.constData(), static_cast<size_t>(lang.size()));
}

JSValue CustomUiRuntime::jsSetState(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv)
{
    Q_UNUSED(thisVal);
    auto* self = static_cast<CustomUiRuntime*>(JS_GetContextOpaque(ctx));
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

JSValue CustomUiRuntime::jsGetState(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv)
{
    Q_UNUSED(thisVal);
    auto* self = static_cast<CustomUiRuntime*>(JS_GetContextOpaque(ctx));
    if (!self || argc < 1)
        return JS_UNDEFINED;

    const char* key = JS_ToCString(ctx, argv[0]);
    if (!key)
        return JS_UNDEFINED;

    auto result = self->jsonValueToJs(self->m_state.value(QString::fromUtf8(key)));
    JS_FreeCString(ctx, key);
    return result;
}

JSValue CustomUiRuntime::jsSaveState(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv)
{
    Q_UNUSED(thisVal);
    Q_UNUSED(argc);
    Q_UNUSED(argv);
    auto* self = static_cast<CustomUiRuntime*>(JS_GetContextOpaque(ctx));
    if (!self)
        return JS_NewBool(ctx, false);
    return JS_NewBool(ctx, self->saveState());
}

JSValue CustomUiRuntime::jsGetInstanceSetting(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv)
{
    Q_UNUSED(thisVal);
    auto* self = static_cast<CustomUiRuntime*>(JS_GetContextOpaque(ctx));
    if (!self || argc < 1)
        return JS_UNDEFINED;
    const char* raw = JS_ToCString(ctx, argv[0]);
    if (!raw)
        return JS_UNDEFINED;
    const auto key = QString::fromUtf8(raw);
    JS_FreeCString(ctx, raw);
    if (!self->m_instance->settings()->getSetting(key))
        return JS_UNDEFINED;
    return self->jsonValueToJs(jsonFromVariant(self->m_instance->settings()->get(key)));
}

JSValue CustomUiRuntime::jsSetInstanceSetting(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv)
{
    Q_UNUSED(thisVal);
    auto* self = static_cast<CustomUiRuntime*>(JS_GetContextOpaque(ctx));
    if (!self || argc < 2)
        return JS_NewBool(ctx, false);
    const char* raw = JS_ToCString(ctx, argv[0]);
    if (!raw)
        return JS_NewBool(ctx, false);
    const auto key = QString::fromUtf8(raw);
    JS_FreeCString(ctx, raw);
    if (!self->m_instance->settings()->getSetting(key))
        return JS_NewBool(ctx, false);
    JSValue copy = JS_DupValue(ctx, argv[1]);
    const auto value = self->jsValueToJson(copy);
    JS_FreeValue(ctx, copy);
    return JS_NewBool(ctx, self->m_instance->settings()->set(key, value.toVariant()));
}

JSValue CustomUiRuntime::jsOpenFolder(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv)
{
    Q_UNUSED(thisVal);
    auto* self = static_cast<CustomUiRuntime*>(JS_GetContextOpaque(ctx));
    if (!self || argc < 1)
        return JS_NewBool(ctx, false);

    const char* rawPath = JS_ToCString(ctx, argv[0]);
    if (!rawPath)
        return JS_NewBool(ctx, false);
    const auto path = QString::fromUtf8(rawPath);
    JS_FreeCString(ctx, rawPath);

    const bool create = argc >= 2 && !JS_IsUndefined(argv[1]) && !JS_IsNull(argv[1]) && JS_ToBool(ctx, argv[1]) > 0;
    return JS_NewBool(ctx, self->openInstanceFolder(path, create));
}

JSValue CustomUiRuntime::jsFsExists(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv)
{
    Q_UNUSED(thisVal);
    auto* self = static_cast<CustomUiRuntime*>(JS_GetContextOpaque(ctx));
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

JSValue CustomUiRuntime::jsFsReadFile(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv)
{
    Q_UNUSED(thisVal);
    auto* self = static_cast<CustomUiRuntime*>(JS_GetContextOpaque(ctx));
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
    if (file.size() > 4 * 1024 * 1024) return JS_UNDEFINED;
    auto data = file.readAll();
    return JS_NewStringLen(ctx, data.constData(), static_cast<size_t>(data.size()));
}

JSValue CustomUiRuntime::jsFsWriteFile(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv)
{
    Q_UNUSED(thisVal);
    auto* self = static_cast<CustomUiRuntime*>(JS_GetContextOpaque(ctx));
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

    if (self->m_instance->isRunning() || data.size() > 4 * 1024 * 1024) return JS_NewBool(ctx, false);
    FS::ensureFolderPathExists(QFileInfo(resolved).absolutePath());
    QSaveFile file(resolved);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return JS_NewBool(ctx, false);
    }
    return JS_NewBool(ctx, file.write(data) == data.size() && file.commit());
}

JSValue CustomUiRuntime::jsFsReaddir(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv)
{
    Q_UNUSED(thisVal);
    auto* self = static_cast<CustomUiRuntime*>(JS_GetContextOpaque(ctx));
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

JSValue CustomUiRuntime::jsFsMkdir(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv)
{
    Q_UNUSED(thisVal);
    auto* self = static_cast<CustomUiRuntime*>(JS_GetContextOpaque(ctx));
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

JSValue CustomUiRuntime::jsFsRm(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv)
{
    Q_UNUSED(thisVal);
    auto* self = static_cast<CustomUiRuntime*>(JS_GetContextOpaque(ctx));
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

    if (QFileInfo(resolved).canonicalFilePath() == QFileInfo(self->m_instance->instanceRoot()).canonicalFilePath() || self->m_instance->isRunning()) return JS_NewBool(ctx, false);
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
            return JS_NewBool(ctx, QDir(resolved).removeRecursively());
        }
        return JS_NewBool(ctx, QDir().rmdir(resolved));
    }

    QFile file(resolved);
    return JS_NewBool(ctx, file.remove());
}

bool CustomUiRuntime::load()
{
    if (!m_instance) return false;
    cleanupJsRuntime();
    m_tabs = {}; m_errors = {}; m_variantGroups = {};
    m_panelDisplayName = tr("Custom UI");
    m_languageId = currentLanguageId();
    loadState();
    QStringList files;
    QDirIterator it(customUiDir(), { "*.json", "*.js" }, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const auto path = it.next();
        if (QFileInfo(path).fileName().compare("state.json", Qt::CaseInsensitive) == 0) continue;
        if (files.size() >= 256) { m_errors.append("Panel exceeds 256 files."); break; }
        files.append(path);
    }
    files.sort(Qt::CaseInsensitive);
    if (!initJsRuntime()) { m_errors.append("Could not initialize JavaScript runtime."); return false; }
    for (const auto& file : files) {
        QString resolved;
        if (!resolveFsPath(file, resolved) || QFileInfo(file).size() > 1024 * 1024) {
            m_errors.append("Unsafe or oversized panel file: " + file); continue;
        }
        const bool ok = file.endsWith(".js", Qt::CaseInsensitive) ? loadTabsFromJsFile(file) : loadTabsFromJsonFile(file);
        if (!ok) m_errors.append("Failed to load panel file: " + file);
    }
    return m_errors.isEmpty();
}
void CustomUiRuntime::appendSingleTab(const QJsonObject& tab, const QString& sourceTag)
{
    if (tab.value("controls").toArray().isEmpty()) return;
    mergeVariantGroups(tab.value("variantGroups").toObject());
    auto result = tab;
    result.insert("source", QDir(customUiDir()).relativeFilePath(sourceTag));
    result.insert("resolvedTitle", resolveLocalizedField(tab, "title", resolveLocalizedField(tab, "name", QFileInfo(sourceTag).baseName())));
    // Match the initial state inserted by checkbox and combo widgets in the original panel.
    for (const auto& value : tab.value("controls").toArray()) {
        const auto control = value.toObject();
        const auto id = control.value("id").toString(), type = control.value("type").toString().trimmed().toLower();
        if (id.isEmpty() || !control.value("sourceSetting").toString().isEmpty()) continue;
        const auto initial = m_state.contains(id) ? m_state.value(id) : control.value("default");
        if (type == "toggle" || type == "checkbox") m_state.insert(id, valueToBool(initial, false));
        if (type == "select" || type == "combo") {
            QJsonValue selected;
            bool first = true;
            for (const auto& item : control.value("options").toArray()) {
                if (!item.isObject() && !item.isString()) continue;
                const auto option = item.isObject() ? item.toObject().value("value") : item;
                if (first || valueToString(option).compare(valueToString(initial), Qt::CaseInsensitive) == 0) selected = option;
                first = false;
                if (valueToString(option).compare(valueToString(initial), Qt::CaseInsensitive) == 0) break;
            }
            m_state.insert(id, selected);
        }
    }
    m_tabs.append(result);
}
QJsonObject CustomUiRuntime::snapshot() const
{
    QJsonArray tabs;
    for (const auto& value : m_tabs) {
        auto tab = value.toObject(); QJsonArray controls;
        for (const auto& item : tab.value("controls").toArray()) {
            auto control = item.toObject();
            const auto type = control.value("type").toString().trimmed().toLower(), id = control.value("id").toString();
            const auto setting = control.value("sourceSetting").toString();
            QJsonValue current = m_state.contains(id) ? m_state.value(id) : control.value("default");
            if (m_instance && !setting.isEmpty() && m_instance->settings()->getSetting(setting)) current = QJsonValue::fromVariant(m_instance->settings()->get(setting));
            if (type == "toggle" || type == "checkbox") current = valueToBool(current, false);
            if (type == "input" || type == "lineedit" || type == "textarea") current = valueToString(current);
            if (type == "number" || type == "spinbox") current = qBound(control.value("minimum").toInt(0), current.toInt(), qMax(control.value("minimum").toInt(0), control.value("maximum").toInt(1024 * 1024)));
            if (m_instance && (type == "accountselect" || type == "account-select")) current = m_instance->settings()->get("InstanceAccountId").toString();
            control.insert("value", current);
            for (const auto field : { "label", "text", "tooltip", "title", "description" }) {
                if (control.contains(field)) control.insert(QString("resolved") + QString(field).left(1).toUpper() + QString(field).mid(1), resolveLocalizedField(control, field));
            }
            controls.append(control);
        }
        tab.insert("controls", controls); tabs.append(tab);
    }
    return { { "tabs", tabs }, { "state", m_state }, { "revision", QString::fromLatin1(m_stateRevision.toHex()) }, { "variantGroups", m_variantGroups },
        { "displayName", m_panelDisplayName }, { "languageId", m_languageId }, { "errors", m_errors } };
}
QJsonValue CustomUiRuntime::bridge(const QString& name, const QJsonArray& arguments, bool& ok)
{
    ok = false;
    if (!m_instance || m_instance->isRunning() || !m_jsContext || arguments.size() > 16) return {};
    const QStringList known{ "setModEnabled", "activateVariant", "getModState", "isModEnabled", "listMods", "getLanguageId",
        "setState", "getState", "saveState", "getInstanceSetting", "setInstanceSetting", "openFolder",
        "fs.exists", "fs.readFile", "fs.writeFile", "fs.readdir", "fs.mkdir", "fs.rm" };
    if (!known.contains(name)) return {};
    auto global = JS_GetGlobalObject(m_jsContext);
    auto object = JS_GetPropertyStr(m_jsContext, global, "launcher");
    JS_FreeValue(m_jsContext, global);
    if (name.startsWith("fs.")) {
        auto fs = JS_GetPropertyStr(m_jsContext, object, "fs");
        JS_FreeValue(m_jsContext, object); object = fs;
    }
    auto function = JS_GetPropertyStr(m_jsContext, object, name.section('.', -1).toUtf8().constData());
    QList<JSValue> args;
    for (const auto& argument : arguments) args.append(jsonValueToJs(argument));
    JSValue result = JS_UNDEFINED;
    ok = callJsWithTimeout(result, function, object, args.size(), args.data(), 700);
    const auto value = ok ? jsValueToJson(result) : QJsonValue();
    JS_FreeValue(m_jsContext, result);
    for (const auto& argument : args) JS_FreeValue(m_jsContext, argument);
    JS_FreeValue(m_jsContext, function); JS_FreeValue(m_jsContext, object);
    return value;
}
bool CustomUiRuntime::trigger(const QJsonObject& control, const QJsonValue& value)
{
    const auto id = control.value("id").toString();
    if (!m_instance || m_instance->isRunning()) return false;
    const auto type = control.value("type").toString().trimmed().toLower();
    if (type == "accountselect" || type == "account-select") {
        if (!value.isString()) return false;
        const auto profile = value.toString();
        bool found = profile.isEmpty();
        for (int i = 0; i < APPLICATION->accounts()->count(); ++i) found |= APPLICATION->accounts()->at(i)->profileId() == profile;
        if (!found) return false;
        m_instance->settings()->set("UseAccountForInstance", !profile.isEmpty());
        m_instance->settings()->set("InstanceAccountId", profile);
        return true;
    }
    if ((type == "toggle" || type == "checkbox" || type == "button") && !value.isBool()) return false;
    if ((type == "input" || type == "lineedit" || type == "textarea") && !value.isString()) return false;
    if (type == "number" || type == "spinbox") {
        if (!value.isDouble() || value.toDouble() != value.toInt() || value.toInt() < control.value("minimum").toInt(0) ||
            value.toInt() > control.value("maximum").toInt(1024 * 1024)) return false;
    }
    if (type == "select" || type == "combo") {
        bool found = false;
        for (const auto& option : control.value("options").toArray()) found |= (option.isObject() ? option.toObject().value("value") : option) == value;
        if (!found) return false;
    }
    if (!QStringList{ "toggle", "checkbox", "button", "input", "lineedit", "textarea", "number", "spinbox", "select", "combo" }.contains(type)) return false;
    if (type != "button" && !id.isEmpty() && control.value("sourceSetting").toString().isEmpty()) m_state.insert(id, value);
    bool ok = true;
    const auto action = control.value("action");
    if (!action.isUndefined() && !action.isNull()) ok = executeActionValue(action, id, value);
    else {
        if (control.contains("mod")) ok = setModEnabledByName(control.value("mod").toString(), valueToBool(value, false)) && ok;
        if (control.contains("variantGroup")) ok = activateVariant(control.value("variantGroup").toString(), valueToString(value)) && ok;
    }
    auto handler = control.value("onChange").toString();
    if (handler.isEmpty()) handler = control.value("onClick").toString();
    if (!handler.isEmpty()) ok = invokeHandler(handler, id, value) && ok;
    if (control.value("saveOnChange").toBool()) ok = saveState() && ok;
    return ok;
}
