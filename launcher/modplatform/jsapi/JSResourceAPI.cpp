// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

// QuickJS 浣跨敤 C99 澶嶅悎瀛楅潰閲忥紝鍦?C++ 涓細浜х敓 pedantic 璀﹀憡
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

#include "JSResourceAPI.h"
#include "Application.h"
#include "net/NetJob.h"
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>

#pragma GCC diagnostic pop

// ============================================================================
// JavaScript 缁戝畾杈呭姪鍑芥暟
// ============================================================================

// HTTP GET 璇锋眰
static JSValue js_http_get(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
{
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "http.get() requires at least 1 argument");

    const char* url = JS_ToCString(ctx, argv[0]);
    if (!url)
        return JS_EXCEPTION;

    // 鍒涘缓涓€涓?Promise (鏆傛椂杩斿洖绌哄璞★紝瀹為檯搴旇寮傛澶勭悊)
    // TODO: 瀹炵幇鐪熸鐨勫紓姝ヨ姹?
    JSValue result = JS_NewObject(ctx);
    JS_FreeCString(ctx, url);

    return result;
}

// JSON 瑙ｆ瀽
static JSValue js_json_parse(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
{
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "JSON.parse() requires 1 argument");

    const char* jsonStr = JS_ToCString(ctx, argv[0]);
    if (!jsonStr)
        return JS_EXCEPTION;

    JSValue result = JS_ParseJSON(ctx, jsonStr, strlen(jsonStr), "<parse>");
    JS_FreeCString(ctx, jsonStr);

    return result;
}

// JSON 搴忓垪鍖?
static JSValue js_json_stringify(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
{
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "JSON.stringify() requires 1 argument");

    JSValue str = JS_JSONStringify(ctx, argv[0], JS_UNDEFINED, JS_UNDEFINED);
    return str;
}

// 鏃ュ織杈撳嚭
static JSValue js_console_log(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
{
    for (int i = 0; i < argc; i++) {
        const char* str = JS_ToCString(ctx, argv[i]);
        if (str) {
            qDebug() << "[JSResourceAPI]" << str;
            JS_FreeCString(ctx, str);
        }
    }
    return JS_UNDEFINED;
}

// ============================================================================
// JSResourceAPI 瀹炵幇
// ============================================================================

// 闈欐€佹垚鍛樺垵濮嬪寲
QList<std::shared_ptr<JSResourceAPI>> JSResourceAPI::s_registeredAPIs;

JSResourceAPI::JSResourceAPI(const QString& apiName) : m_apiName(apiName)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
    m_apiObject = JS_UNDEFINED;
#pragma GCC diagnostic pop
}

JSResourceAPI::~JSResourceAPI()
{
    cleanup();
}

std::shared_ptr<JSResourceAPI> JSResourceAPI::fromFile(const QString& jsFilePath, const QString& apiName)
{
    QFile file(jsFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[JSResourceAPI] Failed to open JS file:" << jsFilePath;
        return nullptr;
    }

    QString jsCode = QString::fromUtf8(file.readAll());
    return fromString(jsCode, apiName);
}

std::shared_ptr<JSResourceAPI> JSResourceAPI::fromString(const QString& jsCode, const QString& apiName)
{
    auto api = std::unique_ptr<JSResourceAPI>(new JSResourceAPI(apiName));

    if (!api->initialize(jsCode)) {
        qWarning() << "[JSResourceAPI] Failed to initialize" << apiName;
        return nullptr;
    }

    // 鍔犺浇鍏冩暟鎹?
    if (!api->loadMetadata()) {
        qWarning() << "[JSResourceAPI] Failed to load metadata for" << apiName;
        return nullptr;
    }

    // 娉ㄥ唽鍒板叏灞€鍒楄〃
    auto sharedApi = std::shared_ptr<JSResourceAPI>(api.release());
    s_registeredAPIs.append(sharedApi);

    qDebug() << "[JSResourceAPI] Registered" << apiName
             << "- Provider:" << sharedApi->m_metadata.provider
             << "Types:" << sharedApi->m_metadata.supportedTypes;

    return sharedApi;
}

bool JSResourceAPI::initialize(const QString& jsCode)
{
    // 鍒涘缓 QuickJS 杩愯鏃跺拰涓婁笅鏂?
    m_runtime = JS_NewRuntime();
    if (!m_runtime) {
        m_lastError = "Failed to create JS runtime";
        return false;
    }

    m_context = JS_NewContext(m_runtime);
    if (!m_context) {
        m_lastError = "Failed to create JS context";
        JS_FreeRuntime(m_runtime);
        m_runtime = nullptr;
        return false;
    }

    // 璁剧疆 JavaScript 缁戝畾
    setupJSBindings();

    // 鎵ц JavaScript 浠ｇ爜
    QByteArray codeUtf8 = jsCode.toUtf8();
    JSValue result = JS_Eval(m_context, codeUtf8.constData(), codeUtf8.size(),
                              m_apiName.toUtf8().constData(), JS_EVAL_TYPE_GLOBAL);

    if (JS_IsException(result)) {
        JSValue exception = JS_GetException(m_context);
        const char* error = JS_ToCString(m_context, exception);
        m_lastError = QString("JavaScript error: %1").arg(error);
        qWarning() << "[JSResourceAPI]" << m_lastError;
        JS_FreeCString(m_context, error);
        JS_FreeValue(m_context, exception);
        JS_FreeValue(m_context, result);
        return false;
    }

    JS_FreeValue(m_context, result);

    // 鑾峰彇 API 瀵硅薄 (鏈熸湜 JS 浠ｇ爜瀵煎嚭 'api' 瀵硅薄)
    JSValue global = JS_GetGlobalObject(m_context);
    m_apiObject = JS_GetPropertyStr(m_context, global, "api");
    JS_FreeValue(m_context, global);

    if (JS_IsUndefined(m_apiObject)) {
        m_lastError = "JavaScript code must export an 'api' object";
        qWarning() << "[JSResourceAPI]" << m_lastError;
        return false;
    }

    qDebug() << "[JSResourceAPI] Successfully initialized" << m_apiName;
    return true;
}

bool JSResourceAPI::loadMetadata()
{
    if (JS_IsUndefined(m_apiObject)) {
        m_lastError = "API object is undefined";
        return false;
    }

    // 鑾峰彇 metadata 瀵硅薄
    JSValue metadataObj = JS_GetPropertyStr(m_context, m_apiObject, "metadata");
    if (JS_IsUndefined(metadataObj)) {
        m_lastError = "API object must have a 'metadata' property";
        qWarning() << "[JSResourceAPI]" << m_lastError;
        JS_FreeValue(m_context, metadataObj);
        return false;
    }

    // 璇诲彇鍚勪釜瀛楁
    auto getStringProp = [this, &metadataObj](const char* key) -> QString {
        JSValue val = JS_GetPropertyStr(m_context, metadataObj, key);
        QString result;
        if (!JS_IsUndefined(val) && !JS_IsNull(val)) {
            const char* str = JS_ToCString(m_context, val);
            if (str) {
                result = QString::fromUtf8(str);
                JS_FreeCString(m_context, str);
            }
        }
        JS_FreeValue(m_context, val);
        return result;
    };

    auto getBoolProp = [this, &metadataObj](const char* key, bool defaultValue) -> bool {
        JSValue val = JS_GetPropertyStr(m_context, metadataObj, key);
        bool result = defaultValue;
        if (JS_IsBool(val)) {
            result = JS_ToBool(m_context, val) > 0;
        }
        JS_FreeValue(m_context, val);
        return result;
    };

    auto getIntProp = [this, &metadataObj](const char* key, int defaultValue) -> int {
        JSValue val = JS_GetPropertyStr(m_context, metadataObj, key);
        int result = defaultValue;
        if (!JS_IsUndefined(val)) {
            int32_t intVal;
            if (JS_ToInt32(m_context, &intVal, val) == 0) {
                result = intVal;
            }
        }
        JS_FreeValue(m_context, val);
        return result;
    };

    m_metadata.id = getStringProp("id");
    m_metadata.displayName = getStringProp("displayName");
    m_metadata.version = getStringProp("version");
    m_metadata.provider = getStringProp("provider");
    m_metadata.icon = getStringProp("icon");
    m_metadata.description = getStringProp("description");
    m_metadata.homepage = getStringProp("homepage");
    m_metadata.enabled = getBoolProp("enabled", true);
    m_metadata.priority = getIntProp("priority", 0);

    // 璇诲彇 supportedTypes 鏁扮粍
    JSValue typesArray = JS_GetPropertyStr(m_context, metadataObj, "supportedTypes");
    if (JS_IsArray(typesArray)) {
        JSValue lengthVal = JS_GetPropertyStr(m_context, typesArray, "length");
        uint32_t length;
        if (JS_ToUint32(m_context, &length, lengthVal) == 0) {
            for (uint32_t i = 0; i < length; i++) {
                JSValue item = JS_GetPropertyUint32(m_context, typesArray, i);
                int32_t typeVal;
                if (JS_ToInt32(m_context, &typeVal, item) == 0) {
                    m_metadata.supportedTypes.append(typeVal);
                }
                JS_FreeValue(m_context, item);
            }
        }
        JS_FreeValue(m_context, lengthVal);
    }
    JS_FreeValue(m_context, typesArray);
    JS_FreeValue(m_context, metadataObj);

    // 楠岃瘉蹇呴渶瀛楁
    if (m_metadata.id.isEmpty()) {
        m_lastError = "Metadata 'id' is required";
        qWarning() << "[JSResourceAPI]" << m_lastError;
        return false;
    }

    return true;
}

void JSResourceAPI::setupJSBindings()
{
    JSValue global = JS_GetGlobalObject(m_context);

    // 鍒涘缓 console 瀵硅薄
    JSValue console = JS_NewObject(m_context);
    JS_SetPropertyStr(m_context, console, "log", JS_NewCFunction(m_context, js_console_log, "log", 0));
    JS_SetPropertyStr(m_context, global, "console", console);

    // 鍒涘缓 http 瀵硅薄
    JSValue http = JS_NewObject(m_context);
    JS_SetPropertyStr(m_context, http, "get", JS_NewCFunction(m_context, js_http_get, "get", 1));
    JS_SetPropertyStr(m_context, global, "http", http);

    // 澧炲己 JSON 瀵硅薄
    JSValue json = JS_GetPropertyStr(m_context, global, "JSON");
    if (JS_IsUndefined(json)) {
        json = JS_NewObject(m_context);
        JS_SetPropertyStr(m_context, global, "JSON", json);
    }
    JS_SetPropertyStr(m_context, json, "parse", JS_NewCFunction(m_context, js_json_parse, "parse", 1));
    JS_SetPropertyStr(m_context, json, "stringify", JS_NewCFunction(m_context, js_json_stringify, "stringify", 1));
    JS_FreeValue(m_context, json);

    JS_FreeValue(m_context, global);
}

void JSResourceAPI::cleanup()
{
    if (!JS_IsUndefined(m_apiObject)) {
        JS_FreeValue(m_context, m_apiObject);
        m_apiObject = JS_UNDEFINED;
    }

    if (m_context) {
        JS_FreeContext(m_context);
        m_context = nullptr;
    }

    if (m_runtime) {
        JS_FreeRuntime(m_runtime);
        m_runtime = nullptr;
    }
}

// ============================================================================
// JavaScript 璋冪敤杈呭姪鍑芥暟
// ============================================================================

JSValue JSResourceAPI::callJSFunction(const char* funcName, int argc, JSValueConst* argv) const
{
    if (JS_IsUndefined(m_apiObject)) {
        qWarning() << "[JSResourceAPI] API object is undefined";
        return JS_UNDEFINED;
    }

    JSValue func = JS_GetPropertyStr(m_context, m_apiObject, funcName);
    if (!JS_IsFunction(m_context, func)) {
        qWarning() << "[JSResourceAPI] Function" << funcName << "not found or not a function";
        JS_FreeValue(m_context, func);
        return JS_UNDEFINED;
    }

    JSValue result = JS_Call(m_context, func, m_apiObject, argc, argv);
    JS_FreeValue(m_context, func);

    if (JS_IsException(result)) {
        JSValue exception = JS_GetException(m_context);
        const char* error = JS_ToCString(m_context, exception);
        qWarning() << "[JSResourceAPI] Error calling" << funcName << ":" << error;
        JS_FreeCString(m_context, error);
        JS_FreeValue(m_context, exception);
        return JS_UNDEFINED;
    }

    return result;
}

QJsonObject JSResourceAPI::jsValueToJsonObject(JSValue val) const
{
    JSValue jsonStr = JS_JSONStringify(m_context, val, JS_UNDEFINED, JS_UNDEFINED);
    const char* str = JS_ToCString(m_context, jsonStr);

    QJsonDocument doc = QJsonDocument::fromJson(QByteArray(str));
    QJsonObject obj = doc.object();

    JS_FreeCString(m_context, str);
    JS_FreeValue(m_context, jsonStr);

    return obj;
}

QJsonArray JSResourceAPI::jsValueToJsonArray(JSValue val) const
{
    JSValue jsonStr = JS_JSONStringify(m_context, val, JS_UNDEFINED, JS_UNDEFINED);
    const char* str = JS_ToCString(m_context, jsonStr);

    QJsonDocument doc = QJsonDocument::fromJson(QByteArray(str));
    QJsonArray arr = doc.array();

    JS_FreeCString(m_context, str);
    JS_FreeValue(m_context, jsonStr);

    return arr;
}

QString JSResourceAPI::jsValueToString(JSValue val) const
{
    const char* str = JS_ToCString(m_context, val);
    QString result = QString::fromUtf8(str);
    JS_FreeCString(m_context, str);
    return result;
}

JSValue JSResourceAPI::jsonObjectToJSValue(const QJsonObject& obj) const
{
    QJsonDocument doc(obj);
    QByteArray json = doc.toJson(QJsonDocument::Compact);
    return JS_ParseJSON(m_context, json.constData(), json.size(), "<json>");
}

JSValue JSResourceAPI::jsonArrayToJSValue(const QJsonArray& arr) const
{
    QJsonDocument doc(arr);
    QByteArray json = doc.toJson(QJsonDocument::Compact);
    return JS_ParseJSON(m_context, json.constData(), json.size(), "<json>");
}

// ============================================================================
// ResourceAPI 鎺ュ彛瀹炵幇
// ============================================================================

Task::Ptr JSResourceAPI::getProjects(QStringList addonIds, std::shared_ptr<QByteArray> response) const
{
    // TODO: 璋冪敤 JS 鐨?getProjects 鏂规硶
    qWarning() << "[JSResourceAPI] getProjects not implemented yet";
    return Task::Ptr();
}

void JSResourceAPI::loadIndexedPack(ModPlatform::IndexedPack& pack, QJsonObject& obj) const
{
    JSValue jsObj = jsonObjectToJSValue(obj);
    JSValue argv[] = { jsObj };
    JSValue result = callJSFunction("loadIndexedPack", 1, argv);

    if (!JS_IsUndefined(result)) {
        QJsonObject resultObj = jsValueToJsonObject(result);

        // 鏇存柊 pack 瀵硅薄
        if (resultObj.contains("name"))
            pack.name = resultObj["name"].toString();
        if (resultObj.contains("slug"))
            pack.slug = resultObj["slug"].toString();
        if (resultObj.contains("description"))
            pack.description = resultObj["description"].toString();
        if (resultObj.contains("addonId"))
            pack.addonId = resultObj["addonId"].toVariant();
        if (resultObj.contains("websiteUrl"))
            pack.websiteUrl = resultObj["websiteUrl"].toString();
        if (resultObj.contains("logoUrl"))
            pack.logoUrl = resultObj["logoUrl"].toString();

        if (resultObj.contains("logoName")) {
            pack.logoName = resultObj["logoName"].toString();
        } else if (!pack.logoUrl.isEmpty() && !pack.slug.isEmpty()) {
            pack.logoName = QString("%1.%2").arg(pack.slug, QFileInfo(QUrl(pack.logoUrl).fileName()).suffix());
        }

        // 浣滆€呬俊鎭?
        if (resultObj.contains("authors")) {
            QJsonArray authorsArr = resultObj["authors"].toArray();
            for (const auto& authorVal : authorsArr) {
                QJsonObject authorObj = authorVal.toObject();
                ModPlatform::ModpackAuthor author;
                author.name = authorObj["name"].toString();
                author.url = authorObj["url"].toString();
                pack.authors.append(author);
            }
        }

        // Side handling
        QString clientSide = resultObj["clientSide"].toString();
        QString serverSide = resultObj["serverSide"].toString();

        bool client = (clientSide == "required" || clientSide == "optional");
        bool server = (serverSide == "required" || serverSide == "optional");

        if (server && client) {
            pack.side = ModPlatform::Side::UniversalSide;
        } else if (server) {
            pack.side = ModPlatform::Side::ServerSide;
        } else if (client) {
            pack.side = ModPlatform::Side::ClientSide;
        }

        // 鏍囪闇€瑕佸姞杞介澶栨暟鎹?
        pack.extraDataLoaded = false;

        // 涓嬭浇閲忓拰鍏虫敞鏁?(ExtraData)
        // 娉ㄦ剰: IndexedPack 缁撴瀯浣撲腑娌℃湁鐩存帴瀛樺偍 downloads/follows 鐨勫瓧娈碉紝
        // 瀹冧滑閫氬父瀛樺偍鍦?extraData 涓垨鑰呬綔涓?UI 鏄剧ず鐨勪竴閮ㄥ垎銆?
        // 杩欓噷鎴戜滑鍋囪 JS API 杩斿洖鐨勫璞＄粨鏋勪笌 IndexedPack 鍏煎銆?

        JS_FreeValue(m_context, result);
    }

    JS_FreeValue(m_context, jsObj);
}

ModPlatform::IndexedVersion JSResourceAPI::loadIndexedPackVersion(QJsonObject& obj, ModPlatform::ResourceType type) const
{
    ModPlatform::IndexedVersion version;

    JSValue jsObj = jsonObjectToJSValue(obj);
    JSValue typeVal = JS_NewInt32(m_context, static_cast<int>(type));
    JSValue argv[] = { jsObj, typeVal };
    JSValue result = callJSFunction("loadIndexedPackVersion", 2, argv);

    if (!JS_IsUndefined(result)) {
        QJsonObject resultObj = jsValueToJsonObject(result);

        // 瑙ｆ瀽鐗堟湰淇℃伅
        if (resultObj.contains("version"))
            version.version = resultObj["version"].toString();
        if (resultObj.contains("versionId"))
            version.fileId = resultObj["versionId"].toVariant();
        if (resultObj.contains("versionNumber"))
            version.version_number = resultObj["versionNumber"].toString();
        if (resultObj.contains("versionType"))
            version.version_type = ModPlatform::IndexedVersionType::fromString(resultObj["versionType"].toString());

        if (resultObj.contains("downloadUrl"))
            version.downloadUrl = resultObj["downloadUrl"].toString();
        if (resultObj.contains("fileName"))
            version.fileName = resultObj["fileName"].toString();
        if (resultObj.contains("date"))
            version.date = resultObj["date"].toString();
        if (resultObj.contains("hash"))
            version.hash = resultObj["hash"].toString();
        if (resultObj.contains("hashType"))
            version.hash_type = resultObj["hashType"].toString();

        if (resultObj.contains("gameVersions")) {
             QJsonArray arr = resultObj["gameVersions"].toArray();
             for(const auto& v : arr) version.mcVersion.append(v.toString());
        }

        if (resultObj.contains("loaders")) {
             QJsonArray arr = resultObj["loaders"].toArray();
             for(const auto& v : arr) {
                 QString loaderStr = v.toString();
                 auto modLoader = ModPlatform::getModLoaderFromString(loaderStr);
                 if (modLoader) {
                     version.loaders |= modLoader;
                 }

                 if (loaderStr == "paper") version.pluginLoaders |= ModPlatform::PluginLoaderType::Paper;
                 else if (loaderStr == "spigot") version.pluginLoaders |= ModPlatform::PluginLoaderType::Spigot;
                 else if (loaderStr == "bukkit") version.pluginLoaders |= ModPlatform::PluginLoaderType::Bukkit;
                 else if (loaderStr == "purpur") version.pluginLoaders |= ModPlatform::PluginLoaderType::Purpur;
                 else if (loaderStr == "sponge") version.pluginLoaders |= ModPlatform::PluginLoaderType::Sponge;
                 else if (loaderStr == "velocity") version.pluginLoaders |= ModPlatform::PluginLoaderType::Velocity;
                 else if (loaderStr == "waterfall") version.pluginLoaders |= ModPlatform::PluginLoaderType::Waterfall;
                 else if (loaderStr == "bungeecord") version.pluginLoaders |= ModPlatform::PluginLoaderType::BungeeCord;
             }
        }

        if (resultObj.contains("dependencies")) {
            QJsonArray deps = resultObj["dependencies"].toArray();
            for (const auto& d : deps) {
                QJsonObject depObj = d.toObject();
                ModPlatform::Dependency dep;
                if (depObj.contains("projectId")) dep.addonId = depObj["projectId"].toVariant();
                if (depObj.contains("versionId")) dep.version = depObj["versionId"].toString();

                QString typeStr = depObj["dependencyType"].toString();
                if (typeStr == "required") dep.type = ModPlatform::DependencyType::Required;
                else if (typeStr == "optional") dep.type = ModPlatform::DependencyType::Optional;
                else if (typeStr == "incompatible") dep.type = ModPlatform::DependencyType::Incompatible;
                else if (typeStr == "embedded") dep.type = ModPlatform::DependencyType::Embedded;
                else dep.type = ModPlatform::DependencyType::Unknown;

                version.dependencies.append(dep);
            }
        }

        JS_FreeValue(m_context, result);
    }

    JS_FreeValue(m_context, typeVal);
    JS_FreeValue(m_context, jsObj);

    return version;
}

QJsonArray JSResourceAPI::documentToArray(QJsonDocument& doc) const
{
    JSValue jsDoc = jsonObjectToJSValue(doc.object());
    JSValue argv[] = { jsDoc };
    JSValue result = callJSFunction("documentToArray", 1, argv);

    QJsonArray arr;
    if (!JS_IsUndefined(result)) {
        arr = jsValueToJsonArray(result);
        JS_FreeValue(m_context, result);
    }

    JS_FreeValue(m_context, jsDoc);
    return arr;
}

void JSResourceAPI::loadExtraPackInfo(ModPlatform::IndexedPack& pack, QJsonObject& obj) const
{
    JSValue packVal = JS_NewObject(m_context);
    JS_SetPropertyStr(m_context, packVal, "name", JS_NewString(m_context, pack.name.toUtf8().constData()));

    JSValue jsObj = jsonObjectToJSValue(obj);
    JSValue argv[] = { packVal, jsObj };
    JSValue result = callJSFunction("loadExtraPackInfo", 2, argv);

    if (!JS_IsUndefined(result)) {
        QJsonObject resultObj = jsValueToJsonObject(result);

        // 鏇存柊棰濆淇℃伅
        if (resultObj.contains("logoUrl"))
            pack.logoUrl = resultObj["logoUrl"].toString();
        if (resultObj.contains("body"))
            pack.extraData.body = resultObj["body"].toString().remove("<br>");

        if (resultObj.contains("issuesUrl"))
            pack.extraData.issuesUrl = resultObj["issuesUrl"].toString();
        if (resultObj.contains("sourceUrl"))
            pack.extraData.sourceUrl = resultObj["sourceUrl"].toString();
        if (resultObj.contains("wikiUrl"))
            pack.extraData.wikiUrl = resultObj["wikiUrl"].toString();
        if (resultObj.contains("discordUrl"))
            pack.extraData.discordUrl = resultObj["discordUrl"].toString();
        if (resultObj.contains("status"))
            pack.extraData.status = resultObj["status"].toString();

        if (resultObj.contains("donationUrls")) {
            QJsonArray donations = resultObj["donationUrls"].toArray();
            for (const auto& val : donations) {
                QJsonObject donationObj = val.toObject();
                ModPlatform::DonationData data;
                data.id = donationObj["id"].toString();
                data.platform = donationObj["platform"].toString();
                data.url = donationObj["url"].toString();
                pack.extraData.donate.append(data);
            }
        }

        pack.extraDataLoaded = true;

        JS_FreeValue(m_context, result);
    }

    JS_FreeValue(m_context, jsObj);
    JS_FreeValue(m_context, packVal);
}

auto JSResourceAPI::getSortingMethods() const -> QList<ResourceAPI::SortingMethod>
{
    QList<ResourceAPI::SortingMethod> methods;

    JSValue result = callJSFunction("getSortingMethods", 0, nullptr);
    if (!JS_IsUndefined(result) && JS_IsArray(result)) {
        // 瑙ｆ瀽鎺掑簭鏂规硶鏁扮粍
        // TODO: 瀹炵幇瀹屾暣鐨勬暟缁勮В鏋?
        JS_FreeValue(m_context, result);
    }

    return methods;
}

auto JSResourceAPI::getSearchURL(SearchArgs const& args) const -> std::optional<QString>
{
    // 灏?SearchArgs 杞崲涓?JS 瀵硅薄
    JSValue jsArgs = JS_NewObject(m_context);
    JS_SetPropertyStr(m_context, jsArgs, "offset", JS_NewInt32(m_context, args.offset));

    if (args.search.has_value()) {
        JS_SetPropertyStr(m_context, jsArgs, "search",
                         JS_NewString(m_context, args.search.value().toUtf8().constData()));
    }

    JSValue argv[] = { jsArgs };
    JSValue result = callJSFunction("getSearchURL", 1, argv);

    std::optional<QString> url;
    if (!JS_IsUndefined(result) && !JS_IsNull(result)) {
        url = jsValueToString(result);
        JS_FreeValue(m_context, result);
    }

    JS_FreeValue(m_context, jsArgs);
    return url;
}

auto JSResourceAPI::getInfoURL(QString const& id) const -> std::optional<QString>
{
    JSValue jsId = JS_NewString(m_context, id.toUtf8().constData());
    JSValue argv[] = { jsId };
    JSValue result = callJSFunction("getInfoURL", 1, argv);

    std::optional<QString> url;
    if (!JS_IsUndefined(result) && !JS_IsNull(result)) {
        url = jsValueToString(result);
        JS_FreeValue(m_context, result);
    }

    JS_FreeValue(m_context, jsId);
    return url;
}

auto JSResourceAPI::getVersionsURL(VersionSearchArgs const& args) const -> std::optional<QString>
{
    JSValue jsArgs = JS_NewObject(m_context);

    if (args.pack && args.pack->addonId.isValid()) {
        QString addonId = args.pack->addonId.toString();
        JS_SetPropertyStr(m_context, jsArgs, "addonId",
                         JS_NewString(m_context, addonId.toUtf8().constData()));
    }

    if (args.mcVersions.has_value() && !args.mcVersions.value().empty()) {
        JSValue versionsArr = JS_NewArray(m_context);
        int i = 0;
        for (const auto& v : args.mcVersions.value()) {
            JS_SetPropertyUint32(m_context, versionsArr, i++,
                                JS_NewString(m_context, v.toString().toUtf8().constData()));
        }
        JS_SetPropertyStr(m_context, jsArgs, "versions", versionsArr);
    }

    if (args.loaders.has_value()) {
        JS_SetPropertyStr(m_context, jsArgs, "modLoaders", JS_NewInt32(m_context, static_cast<int>(args.loaders.value())));
    }

    if (args.pluginLoaders.has_value()) {
        JS_SetPropertyStr(m_context, jsArgs, "pluginLoaders", JS_NewInt32(m_context, static_cast<int>(args.pluginLoaders.value())));
    }

    JSValue argv[] = { jsArgs };
    JSValue result = callJSFunction("getVersionsURL", 1, argv);

    std::optional<QString> url;
    if (!JS_IsUndefined(result) && !JS_IsNull(result)) {
        url = jsValueToString(result);
        JS_FreeValue(m_context, result);
    }

    JS_FreeValue(m_context, jsArgs);
    return url;
}

auto JSResourceAPI::getDependencyURL(DependencySearchArgs const& args) const -> std::optional<QString>
{
    // Hangar 涓嶉渶瑕佸崟鐙殑渚濊禆 URL
    return {};
}

// ============================================================================
// 闈欐€佹柟娉?- API 娉ㄥ唽鍜屾煡璇?
// ============================================================================

QList<std::shared_ptr<JSResourceAPI>> JSResourceAPI::getRegisteredAPIs()
{
    return s_registeredAPIs;
}

QList<std::shared_ptr<JSResourceAPI>> JSResourceAPI::getAPIsForResourceType(ModPlatform::ResourceType type)
{
    QList<std::shared_ptr<JSResourceAPI>> result;
    int typeInt = static_cast<int>(type);

    for (const auto& api : s_registeredAPIs) {
        if (api->m_metadata.enabled && api->m_metadata.supportedTypes.contains(typeInt)) {
            result.append(api);
        }
    }

    // 鎸変紭鍏堢骇鎺掑簭锛堥檷搴忥級
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        return a->m_metadata.priority > b->m_metadata.priority;
    });

    return result;
}
