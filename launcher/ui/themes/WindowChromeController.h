// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QAbstractNativeEventFilter>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QPointer>

class QWidget;
class WindowChromeController final : public QObject, public QAbstractNativeEventFilter {
   public:
    explicit WindowChromeController(QObject* parent = nullptr);
    ~WindowChromeController() override;
    static bool supported();
    static bool effectsSupported();
    // Apply renderer-independent window material hints extracted from a
    // theme. This is intentionally separate from the Luna title bar: KDE
    // themes can request blur/transparency while retaining native decoration.
    void setEffects(const QJsonObject& effects);
    // Switch the Luna decoration on or off and update its configuration for
    // windows that are already visible. This is safe to call repeatedly when
    // a theme combination is applied at runtime.
    void initialize(bool enabled, const QJsonObject& configuration = {});
    // Reapply the stored title-bar margin after a style or font change. Qt can
    // otherwise leave the content layout at its pre-theme geometry until a
    // native resize occurs.
    void relayoutWindows();
    bool eventFilter(QObject* watched, QEvent* event) override;
    bool nativeEventFilter(const QByteArray&, void* message, qintptr* result) override;

   private:
    void ensureInstalled();
    void attach(QWidget* window);
    void detach(QWidget* window);
    void applyEffects(QWidget* window);
    void relayoutWindow(QWidget* window);
    bool m_enabled = false;
    bool m_installed = false;
    bool m_applyingEffects = false;
    QJsonObject m_config;
    QJsonObject m_effects;
    QHash<QWidget*, QPointer<QWidget>> m_bars;
};
