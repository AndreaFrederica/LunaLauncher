// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Luna Launcher - Minecraft Launcher
 */
#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QWidget>

#include "minecraft/MinecraftInstance.h"
#include "ui/pages/BasePage.h"

// QuickJS uses C99 compound literals in public headers, which can trigger pedantic warnings in C++ mode.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
extern "C" {
#include "quickjs.h"
}
#pragma GCC diagnostic pop

class QLabel;
class QTabWidget;
class QVBoxLayout;

class CustomUIPanelPage : public QWidget, public BasePage {
    Q_OBJECT

   public:
    explicit CustomUIPanelPage(MinecraftInstancePtr instance, QWidget* parent = nullptr);
    explicit CustomUIPanelPage(MinecraftInstance* instance, QWidget* parent = nullptr)
        : CustomUIPanelPage(MinecraftInstancePtr(instance, [](MinecraftInstance*) {}), parent)
    {}
    ~CustomUIPanelPage() override;

    QString displayName() const override { return m_panelDisplayName; }
    QIcon icon() const override;
    QString id() const override { return "custom-ui"; }
    bool apply() override;
    bool shouldDisplay() const override { return m_hasCustomTabs; }
    QString helpPage() const override { return QString(); }
    void openedImpl() override;
    void retranslate() override;

   private:
    void rebuildTabs();
    void clearTabs();
    void loadState();
    bool saveState() const;

    bool initJsRuntime();
    void cleanupJsRuntime();
    bool loadTabsFromJsonFile(const QString& filePath);
    bool loadTabsFromJsFile(const QString& filePath);
    void appendTabsFromValue(const QJsonValue& value, const QString& sourceTag);
    void appendSingleTab(const QJsonObject& tab, const QString& sourceTag);
    void mergeVariantGroups(const QJsonObject& groupsObj);

    QWidget* createTabContent(const QJsonObject& tabObj);
    void addControl(QWidget* parent, QVBoxLayout* layout, const QJsonObject& control);
    void onControlTriggered(const QJsonObject& control, const QJsonValue& value);

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

   private:
    struct JsDeadline {
        qint64 deadlineMs = 0;
    };

    MinecraftInstancePtr m_instance;
    QTabWidget* m_tabWidget = nullptr;
    QLabel* m_emptyLabel = nullptr;
    bool m_hasCustomTabs = false;
    QString m_panelDisplayName;
    QString m_languageId;

    QJsonObject m_state;
    QJsonObject m_variantGroups;

    JSRuntime* m_jsRuntime = nullptr;
    JSContext* m_jsContext = nullptr;
    JsDeadline m_jsDeadline;
};
