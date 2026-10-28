// SPDX-License-Identifier: GPL-3.0-only
// Script dialect adapter for CustomUIPanelPage. Keep bridge semantics in sync with that page.
#pragma once
#include <QObject>
#include <QPointer>
#include <QJsonArray>
#include <QJsonObject>
#include "minecraft/MinecraftInstance.h"
extern "C" {
#include "quickjs.h"
}
class CustomUiRuntime : public QObject {
public:
    explicit CustomUiRuntime(MinecraftInstance* instance) : m_instance(instance) {}
    bool valid() const { return !m_instance.isNull(); }
    MinecraftInstance* instance() const { return m_instance; }
    ~CustomUiRuntime() override { cleanupJsRuntime(); }
    bool load();
    QJsonObject snapshot() const;
    bool trigger(const QJsonObject& control, const QJsonValue& value);
    QJsonValue bridge(const QString& name, const QJsonArray& arguments, bool& ok);
    bool activate(const QString& group, const QString& option) { return activateVariant(group, option); }
    bool persist() const { return saveState(); }
private:
    void rebuildTabs() { load(); }
    void loadState();
    bool saveState() const;

    bool initJsRuntime();
    void cleanupJsRuntime();
    bool loadTabsFromJsonFile(const QString& filePath);
    bool loadTabsFromJsFile(const QString& filePath);
    void appendTabsFromValue(const QJsonValue& value, const QString& sourceTag);
    void appendSingleTab(const QJsonObject& tab, const QString& sourceTag);
    void mergeVariantGroups(const QJsonObject& groupsObj);

    bool executeActionValue(const QJsonValue& action, const QString& controlId, const QJsonValue& value);
    bool executeActionObject(const QJsonObject& action, const QString& controlId, const QJsonValue& value);
    bool invokeHandler(const QString& handlerName, const QString& controlId, const QJsonValue& value);

    bool setModEnabledByName(const QString& modName, bool enabled);
    bool activateVariant(const QString& groupName, const QString& optionName);

    static int jsInterruptHandler(JSRuntime* rt, void* opaque);
    static JSValue jsLog(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv);
    static JSValue jsSetModEnabled(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv);
    static JSValue jsActivateVariant(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv);
    static JSValue jsGetModState(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv);
    static JSValue jsIsModEnabled(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv);
    static JSValue jsListMods(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv);
    static JSValue jsGetLanguageId(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv);
    static JSValue jsSetState(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv);
    static JSValue jsGetState(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv);
    static JSValue jsSaveState(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv);
    static JSValue jsGetInstanceSetting(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv);
    static JSValue jsSetInstanceSetting(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv);
    static JSValue jsOpenFolder(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv);
    static JSValue jsFsExists(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv);
    static JSValue jsFsReadFile(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv);
    static JSValue jsFsWriteFile(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv);
    static JSValue jsFsReaddir(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv);
    static JSValue jsFsMkdir(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv);
    static JSValue jsFsRm(JSContext* ctx, JSValueConst thisVal, int argc, JSValueConst* argv);

    QJsonValue jsValueToJson(JSValue value) const;
    JSValue jsonValueToJs(const QJsonValue& value) const;
    bool callJsWithTimeout(JSValue& outResult, JSValue function, JSValue thisObj, int argc, JSValue* argv, int timeoutMs);
    bool evalJsWithTimeout(JSValue& outResult, const QByteArray& code, const QByteArray& fileName, int timeoutMs);

    QString customUiDir() const;
    QString stateFilePath() const;
    QString currentLanguageId() const;
    QString resolveLocalizedValue(const QJsonValue& value, const QString& fallback = QString()) const;
    QString resolveLocalizedField(const QJsonObject& obj, const QString& key, const QString& fallback = QString()) const;
    void applyPanelMetadata(const QJsonObject& obj);
    QString normalizeModIdentity(QString modName) const;
    QStringList toStringList(const QJsonValue& value) const;
    bool findModFile(const QString& modName, QString& pathOut, bool& enabledOut) const;
    QJsonObject getModState(const QString& modName) const;
    QJsonArray listManagedMods(const QString& filter) const;
    bool resolveFsPath(const QString& userPath, QString& absolutePathOut) const;
    bool openInstanceFolder(const QString& userPath, bool create);


    struct JsDeadline { qint64 deadlineMs = 0; };
    QPointer<MinecraftInstance> m_instance;
    QString m_panelDisplayName;
    QString m_languageId;
    QJsonObject m_state;
    QJsonObject m_variantGroups;
    QJsonArray m_tabs;
    QJsonArray m_errors;
    mutable QByteArray m_stateRevision;
    int m_actionDepth = 0;
    int m_jsDepth = 0;
    JSRuntime* m_jsRuntime = nullptr;
    JSContext* m_jsContext = nullptr;
    mutable JsDeadline m_jsDeadline;
};
