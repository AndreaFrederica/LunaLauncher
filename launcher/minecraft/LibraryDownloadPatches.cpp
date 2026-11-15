// SPDX-License-Identifier: GPL-3.0-only
#include "LibraryDownloadPatches.h"

#include "Application.h"
#include "DesktopServices.h"
#include "LauncherResources.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMutex>
#include <QMutexLocker>
#include <QSaveFile>
#include <QSet>
#include <QUrl>
#include <algorithm>
#include <cstring>
#include "quickjs.h"

namespace {
constexpr auto fileName = "library-download-patches.json5";
constexpr auto overlayName = "library-download-patches.overlay.json5";
constexpr const char* json5Parser =
#include "../../libraries/json5/parser.inc"
;

QJsonObject parse(const QByteArray& bytes, const QString& path, QString& error)
{
    auto* runtime = JS_NewRuntime();
    if (!runtime) { error = "Could not create JSON5 parser."; return {}; }
    JS_SetMemoryLimit(runtime, 32 * 1024 * 1024);
    JS_SetMaxStackSize(runtime, 512 * 1024);
    QElapsedTimer timer; timer.start();
    JS_SetInterruptHandler(runtime, [](JSRuntime*, void* data) -> int { return static_cast<QElapsedTimer*>(data)->elapsed() > 1000; }, &timer);
    auto* context = JS_NewContext(runtime);
    if (!context) { JS_FreeRuntime(runtime); error = "Could not create JSON5 parser context."; return {}; }
    // Only the shipped JSON5 parser is executable. Configuration is passed as
    // a string to JSON5.parse, never evaluated as JavaScript.
    auto loaded = JS_Eval(context, json5Parser, strlen(json5Parser), "json5-2.2.3", JS_EVAL_TYPE_GLOBAL);
    auto global = JS_GetGlobalObject(context);
    auto parser = JS_GetPropertyStr(context, global, "JSON5");
    auto function = JS_GetPropertyStr(context, parser, "parse");
    auto argument = JS_NewStringLen(context, bytes.constData(), bytes.size());
    auto value = JS_IsException(loaded) ? JS_EXCEPTION : JS_Call(context, function, parser, 1, &argument);
    QJsonObject object;
    if (JS_IsException(value)) {
        auto exception = JS_GetException(context);
        if (const auto* text = JS_ToCString(context, exception)) { error = QString::fromUtf8(text); JS_FreeCString(context, text); }
        JS_FreeValue(context, exception);
    } else {
        auto json = JS_JSONStringify(context, value, JS_UNDEFINED, JS_UNDEFINED);
        if (const auto* text = JS_ToCString(context, json)) {
            const auto doc = QJsonDocument::fromJson(QByteArray(text));
            if (doc.isObject()) object = doc.object();
            else error = "The root must be an object.";
            JS_FreeCString(context, text);
        } else error = "Could not serialize JSON5 data.";
        JS_FreeValue(context, json);
    }
    JS_FreeValue(context, value); JS_FreeValue(context, argument);
    JS_FreeValue(context, function); JS_FreeValue(context, parser); JS_FreeValue(context, global); JS_FreeValue(context, loaded);
    JS_FreeContext(context); JS_FreeRuntime(runtime);
    if (!error.isEmpty()) error = path + ": " + error;
    return object;
}

bool validUrl(const QString& text)
{
    const QUrl url(text, QUrl::StrictMode);
    return url.isValid() && (url.scheme() == "https" || url.scheme() == "http") && !url.host().isEmpty() && url.userInfo().isEmpty();
}

bool readRules(const QString& path, QVector<LibraryDownloadPatches::Rule>& output, QString& error)
{
    QFile file(path);
    if (!file.exists()) return true;
    if (!file.open(QIODevice::ReadOnly)) { error = path + ": " + file.errorString(); return false; }
    if (file.size() > 1024 * 1024) { error = path + ": JSON5 file exceeds 1 MiB."; return false; }
    const auto object = parse(file.readAll(), path, error);
    if (!error.isEmpty()) return false;
    if (object.value("formatVersion").toInt(-1) != 1 || !object.value("rules").isArray()) {
        error = path + ": Expected formatVersion: 1 and a rules array."; return false;
    }
    QSet<QString> ids;
    for (const auto& item : object.value("rules").toArray()) {
        const auto entry = item.toObject();
        LibraryDownloadPatches::Rule rule;
        rule.id = entry.value("id").toString();
        if (rule.id.isEmpty() || ids.contains(rule.id) || (entry.contains("enabled") && !entry.value("enabled").isBool())) {
            error = path + ": Invalid/duplicate rule ID or enabled flag: " + rule.id; return false;
        }
        ids.insert(rule.id);
        rule.enabled = entry.value("enabled").toBool(true);
        if (rule.enabled) {
            const auto match = entry.value("match").toObject();
            for (auto it = match.begin(); it != match.end(); ++it) {
                if ((it.key() != "library" && it.key() != "classifier" && it.key() != "url")
                    || !it.value().isString()) {
                    error = path + ": Unknown match key or non-string value in " + rule.id; return false;
                }
            }
            if (entry.contains("mode") && !entry.value("mode").isString()) {
                error = path + ": mode must be a string in " + rule.id; return false;
            }
            rule.library = match.value("library").toString();
            rule.classifier = match.value("classifier").toString();
            rule.originalUrl = match.value("url").toString();
            rule.mode = entry.value("mode").toString("fallback");
            for (const auto& url : entry.value("urls").toArray()) {
                if (!url.isString() || !validUrl(url.toString())) { error = path + ": Invalid URL in " + rule.id; return false; }
                if (!rule.urls.contains(url.toString())) rule.urls.append(url.toString());
            }
            // Exact matching avoids accidentally redirecting unrelated versions
            // or native architectures. Each enabled rule must identify a library
            // or an original URL; both constraints apply when both are provided.
            if ((rule.library.isEmpty() && rule.originalUrl.isEmpty()) || rule.urls.isEmpty()
                || (rule.mode != "replace" && rule.mode != "fallback")
                || (!rule.originalUrl.isEmpty() && !validUrl(rule.originalUrl))) {
                error = path + ": Invalid match, mode or URLs in " + rule.id; return false;
            }
        }
        output.append(rule);
    }
    return true;
}
}

QString LibraryDownloadPatches::basePath() { return QDir(QCoreApplication::applicationDirPath()).filePath(fileName); }
QString LibraryDownloadPatches::overlayPath() { return QDir(LauncherResources::userRoot()).filePath(overlayName); }

LibraryDownloadPatches LibraryDownloadPatches::load(const QString& base, const QString& overlay)
{
    LibraryDownloadPatches result;
    for (const auto& path : {base, overlay}) {
        QVector<Rule> layer;
        QString error;
        if (!readRules(path, layer, error)) { result.errors.append(error); continue; }
        for (const auto& rule : layer) {
            auto existing = std::find_if(result.rules.begin(), result.rules.end(), [&](const Rule& value) { return value.id == rule.id; });
            if (existing == result.rules.end()) result.rules.append(rule);
            else *existing = rule;
        }
    }
    return result;
}

LibraryDownloadPatches LibraryDownloadPatches::current()
{
    static QMutex mutex;
    const QMutexLocker lock(&mutex);
    static QString stamp;
    static LibraryDownloadPatches cache;
    QString next;
    for (const auto& path : {basePath(), overlayPath()}) {
        const QFileInfo info(path);
        next += path + ':' + QString::number(info.lastModified().toMSecsSinceEpoch()) + ':' + QString::number(info.size()) + '\n';
    }
    if (stamp != next) {
        cache = load(basePath(), overlayPath()); stamp = next;
        for (const auto& error : cache.errors) qWarning().noquote() << "Library download patches:" << error;
    }
    return cache;
}

QStringList LibraryDownloadPatches::apply(const QString& library, const QString& classifier, const QString& originalUrl,
                                        const QStringList& candidates, QStringList* matched) const
{
    auto result = candidates;
    result.removeAll(QString());
    for (const auto& rule : rules) {
        if (!rule.enabled || (!rule.library.isEmpty() && rule.library != library)
            || (!rule.classifier.isEmpty() && rule.classifier != classifier)
            || (!rule.originalUrl.isEmpty() && rule.originalUrl != originalUrl)) continue;
        if (matched) matched->append(rule.id);
        if (rule.mode == "replace") result.clear();
        for (const auto& url : rule.urls) if (!result.contains(url)) result.append(url);
    }
    return result;
}

QJsonObject LibraryDownloadPatches::status()
{
    const auto config = current();
    QJsonArray rules;
    for (const auto& rule : config.rules)
        rules.append(QJsonObject{{"id", rule.id}, {"enabled", rule.enabled}, {"library", rule.library},
                                 {"classifier", rule.classifier}, {"url", rule.originalUrl}, {"mode", rule.mode}, {"urls", QJsonArray::fromStringList(rule.urls)}});
    return {{"basePath", basePath()}, {"overlayPath", overlayPath()}, {"rules", rules}, {"errors", QJsonArray::fromStringList(config.errors)}};
}

bool LibraryDownloadPatches::openFile(bool overlay, QString& error)
{
    const auto path = overlay ? overlayPath() : basePath();
    if (!QFileInfo::exists(path) && overlay) {
        QSaveFile file(path);
        const QByteArray initial = "// User rules replace shipped rules with the same id.\n{formatVersion: 1, rules: [],}\n";
        if (!QDir().mkpath(QFileInfo(path).absolutePath()) || !file.open(QIODevice::WriteOnly)
            || file.write(initial) != initial.size() || !file.commit()) { error = "Could not create " + path; return false; }
    }
    if (!QFileInfo(path).isFile() || !DesktopServices::openUrl(QUrl::fromLocalFile(path))) { error = "Could not open " + path; return false; }
    return true;
}
