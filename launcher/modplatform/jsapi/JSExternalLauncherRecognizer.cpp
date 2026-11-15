// SPDX-License-Identifier: GPL-3.0-only
#include "JSExternalLauncherRecognizer.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include "quickjs.h"
#pragma GCC diagnostic pop

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include "ExternalInstanceProvenance.h"

namespace {

QString stringProperty(JSContext* context, JSValue object, const char* name)
{
    auto value = JS_GetPropertyStr(context, object, name);
    QString result;
    if (!JS_IsUndefined(value) && !JS_IsNull(value)) {
        if (const auto* text = JS_ToCString(context, value)) {
            result = QString::fromUtf8(text);
            JS_FreeCString(context, text);
        }
    }
    JS_FreeValue(context, value);
    return result;
}

int intProperty(JSContext* context, JSValue object, const char* name, int fallback)
{
    auto value = JS_GetPropertyStr(context, object, name);
    int32_t result = fallback;
    JS_ToInt32(context, &result, value);
    JS_FreeValue(context, value);
    return result;
}

QJsonObject callRecognizer(const QString& source, const QString& fileName, const QJsonObject& contextObject)
{
    auto* runtime = JS_NewRuntime();
    auto* context = runtime ? JS_NewContext(runtime) : nullptr;
    if (!context) {
        if (runtime) JS_FreeRuntime(runtime);
        return {};
    }
    const auto code = source.toUtf8();
    auto evaluated = JS_Eval(context, code.constData(), code.size(), fileName.toUtf8().constData(), JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(evaluated)) {
        JS_FreeValue(context, evaluated);
        JS_FreeContext(context);
        JS_FreeRuntime(runtime);
        return {};
    }
    JS_FreeValue(context, evaluated);
    auto global = JS_GetGlobalObject(context);
    auto recognizer = JS_GetPropertyStr(context, global, "recognizer");
    auto detect = JS_GetPropertyStr(context, recognizer, "detect");
    const auto contextJson = QJsonDocument(contextObject).toJson(QJsonDocument::Compact);
    auto contextValue = JS_ParseJSON(context, contextJson.constData(), contextJson.size(), "<context>");
    QJsonObject result;
    if (JS_IsFunction(context, detect) && !JS_IsException(contextValue)) {
        auto returned = JS_Call(context, detect, recognizer, 1, &contextValue);
        if (!JS_IsException(returned)) {
            auto json = JS_JSONStringify(context, returned, JS_UNDEFINED, JS_UNDEFINED);
            if (const auto* text = JS_ToCString(context, json)) {
                QJsonParseError error;
                const auto document = QJsonDocument::fromJson(QByteArray(text), &error);
                if (error.error == QJsonParseError::NoError && document.isObject()) result = document.object();
                JS_FreeCString(context, text);
            }
            JS_FreeValue(context, json);
        }
        JS_FreeValue(context, returned);
    }
    JS_FreeValue(context, contextValue);
    JS_FreeValue(context, detect);
    JS_FreeValue(context, recognizer);
    JS_FreeValue(context, global);
    JS_FreeContext(context);
    JS_FreeRuntime(runtime);
    return result;
}

ExternalLauncherEvidence probe(const ExternalLauncherRecognizer& metadata, const QString& source, const QString& fileName,
                               const ExternalLauncherProbeContext& context)
{
    ExternalLauncherEvidence evidence;
    evidence.providerId = metadata.id;
    evidence.displayName = metadata.displayName;
    evidence.iconId = metadata.iconId;
    QJsonObject input{ { "root", context.root }, { "version", context.version },
                       { "layout", QJsonObject{ { "gameRoot", context.layout.gameRoot }, { "versionJson", context.layout.versionJson },
                                                 { "isolated", context.layout.isolated } } } };
    const auto result = callRecognizer(source, fileName, input);
    evidence.version = result.value("version").toString();
    evidence.confidence = qBound(0, result.value("confidence").toInt(), 100);
    for (const auto& path : result.value("matchedPaths").toArray()) if (path.isString()) evidence.matchedPaths.append(path.toString());
    for (const auto& field : result.value("matchedFields").toArray()) if (field.isString()) evidence.matchedFields.append(field.toString());
    const auto capabilities = result.value("capabilities").toObject();
    evidence.canReadInstanceSettings = capabilities.value("read").toBool(false);
    evidence.canEditCanonicalMetadata = capabilities.value("editCanonicalMetadata").toBool(false);
    return evidence;
}

void loadFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024) return;
    const auto source = QString::fromUtf8(file.readAll());
    // Metadata is evaluated once to avoid accepting scripts without the
    // standard recognizer descriptor. Detection itself gets a fresh runtime.
    auto* runtime = JS_NewRuntime();
    auto* context = runtime ? JS_NewContext(runtime) : nullptr;
    if (!context) { if (runtime) JS_FreeRuntime(runtime); return; }
    const auto code = source.toUtf8();
    auto evaluated = JS_Eval(context, code.constData(), code.size(), path.toUtf8().constData(), JS_EVAL_TYPE_GLOBAL);
    auto global = JS_GetGlobalObject(context);
    auto object = JS_GetPropertyStr(context, global, "recognizer");
    ExternalLauncherRecognizer metadata;
    metadata.id = stringProperty(context, object, "id");
    metadata.displayName = stringProperty(context, object, "displayName");
    metadata.iconId = stringProperty(context, object, "iconId");
    metadata.priority = intProperty(context, object, "priority", 0);
    metadata.builtIn = false;
    const bool valid = !JS_IsException(evaluated) && !metadata.id.isEmpty() && !metadata.displayName.isEmpty();
    JS_FreeValue(context, object);
    JS_FreeValue(context, global);
    JS_FreeValue(context, evaluated);
    JS_FreeContext(context);
    JS_FreeRuntime(runtime);
    if (!valid) return;
    metadata.detect = [source, path, metadata](const ExternalLauncherProbeContext& context) {
        return probe(metadata, source, path, context);
    };
    ExternalLauncherRecognizerRegistry::instance().registerRecognizer(std::move(metadata));
}

} // namespace

void JSExternalLauncherRecognizerRuntime::initialize(const QStringList& directories)
{
    for (const auto& directory : directories) {
        if (!QDir(directory).exists()) continue;
        QDirIterator iterator(directory, { QStringLiteral("*.js") }, QDir::Files, QDirIterator::Subdirectories);
        while (iterator.hasNext()) loadFile(iterator.next());
    }
}

