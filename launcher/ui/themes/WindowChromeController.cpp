// SPDX-License-Identifier: GPL-3.0-only
#include "WindowChromeController.h"
#include <QApplication>
#include <QCoreApplication>
#include <QDialog>
#include <QLayout>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QWindow>
#include <QScopedValueRollback>
#include <QWizard>
#include <QIcon>
#include <cstring>
#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#endif

namespace {
#ifdef Q_OS_WIN
template <typename Function>
Function resolveWindowsFunction(HMODULE module, const char* name)
{
    // GetProcAddress returns FARPROC, whose type is intentionally generic on
    // MinGW. Copy the address into the requested function-pointer type rather
    // than relying on an incompatible function-pointer cast.
    Function result{};
    const auto address = module ? GetProcAddress(module, name) : nullptr;
    static_assert(sizeof(Function) == sizeof(address));
    if (address)
        std::memcpy(&result, &address, sizeof(result));
    return result;
}
#endif

class CaptionButton final : public QToolButton {
   public:
    CaptionButton(int action, QWidget* parent, const QJsonObject& config) : QToolButton(parent), action(action), m_config(config)
    {
        setAutoRaise(true);
    }
    int action;
    QJsonObject m_config;

   protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        const auto active = window()->isActiveWindow();
        QColor fg(m_config.value(active ? "activeForeground" : "inactiveForeground").toString());
        if (!fg.isValid())
            fg = QApplication::palette().color(active ? QPalette::Active : QPalette::Inactive, QPalette::WindowText);
        QColor background(m_config.value(active ? "activeBackground" : "inactiveBackground").toString());
        if (!background.isValid())
            background = QApplication::palette().color(active ? QPalette::Active : QPalette::Inactive, QPalette::Window);
        if (underMouse() || isDown())
            painter.fillRect(rect(), action == 2 ? QColor(210, 42, 52) : QColor(fg.red(), fg.green(), fg.blue(), isDown() ? 55 : 28));
        painter.setPen(QPen(underMouse() && action == 2 ? Qt::white : fg, 1.25));
        const QRectF r(width() / 2. - 4.5, height() / 2. - 4.5, 9, 9);
        if (action == 0)
            painter.drawLine(r.bottomLeft(), r.bottomRight());
        else if (action == 1) {
            if (window()->isMaximized()) {
                painter.drawRect(r.translated(2, -2));
                painter.fillRect(r, background);
            }
            painter.drawRect(r);
        } else {
            painter.drawLine(r.topLeft(), r.bottomRight());
            painter.drawLine(r.topRight(), r.bottomLeft());
        }
    }
};

class CaptionBar final : public QWidget {
   public:
    CaptionBar(QWidget* window, const QJsonObject& config) : QWidget(window), m_window(window), m_config(config)
    {
        setObjectName("lunaWindowCaption");
        setAccessibleName(tr("Window title bar"));
        setMouseTracking(true);
        setFixedHeight(qBound(28, config.value("height").toInt(36), 64));
        for (int i = 0; i < 3; ++i) {
            buttons[i] = new CaptionButton(i, this, config);
            buttons[i]->setObjectName(QString("lunaCaptionButton%1").arg(i));
            buttons[i]->setAccessibleName(i == 0 ? tr("Minimize") : i == 1 ? tr("Maximize or restore") : tr("Close"));
            buttons[i]->setToolTip(buttons[i]->accessibleName());
        }
        connect(buttons[0], &QToolButton::clicked, window, &QWidget::showMinimized);
        connect(buttons[1], &QToolButton::clicked, this, [this] { toggleMaximize(); });
        connect(buttons[2], &QToolButton::clicked, window, &QWidget::close);
        connect(window, &QWidget::windowTitleChanged, this, qOverload<>(&QWidget::update));
        connect(window, &QWidget::windowIconChanged, this, qOverload<>(&QWidget::update));
    }
    CaptionButton* buttons[3];
    void setConfig(const QJsonObject& config)
    {
        m_config = config;
        setFixedHeight(qBound(28, config.value("height").toInt(36), 64));
        sync();
        update();
    }
    void toggleMaximize()
    {
        if (buttons[1]->isVisible())
            m_window->isMaximized() ? m_window->showNormal() : m_window->showMaximized();
    }
    void sync()
    {
        buttons[0]->setVisible(m_window->windowFlags().testFlag(Qt::WindowMinimizeButtonHint));
        buttons[1]->setVisible(m_window->windowFlags().testFlag(Qt::WindowMaximizeButtonHint) &&
                               m_window->minimumSize() != m_window->maximumSize());
        buttons[2]->setVisible(m_window->windowFlags().testFlag(Qt::WindowCloseButtonHint));
        int right = width();
        for (int i = 2; i >= 0; --i)
            if (!buttons[i]->isHidden()) {
                right -= 44;
                buttons[i]->setGeometry(right, 0, 44, height());
                buttons[i]->update();
            }
        update();
    }

   protected:
    void resizeEvent(QResizeEvent*) override { sync(); }
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        const bool active = m_window->isActiveWindow();
        QColor background(m_config.value(active ? "activeBackground" : "inactiveBackground").toString());
        QColor foreground(m_config.value(active ? "activeForeground" : "inactiveForeground").toString());
        const auto effects = m_window->property("_lunaWindowEffects").toJsonObject();
        if (!effects.isEmpty() && (effects.value("translucentWindows").toBool() || effects.value("backdrop").toString() != "none")) {
            const auto reduction = qBound(0, effects.value("reduceWindowOpacity").toInt(), 100);
            const auto opacity = qBound(10, effects.value("windowOpacity").toInt(100), 100);
            if (background.isValid())
                background.setAlpha(qBound(48, qMin(235, qMax(96, opacity * 235 / 100 - reduction * 2)), 235));
        }
        painter.fillRect(rect(), background.isValid()
                                     ? background
                                     : QApplication::palette().color(active ? QPalette::Active : QPalette::Inactive, QPalette::Window));
        painter.setPen(foreground.isValid()
                           ? foreground
                           : QApplication::palette().color(active ? QPalette::Active : QPalette::Inactive, QPalette::WindowText));
        // A number of Qt dialogs (notably QWizard) do not set a per-window
        // icon. The native Windows frame falls back to the application icon,
        // so the custom frame must do the same or the icon disappears when
        // Luna decoration is enabled.
        auto icon = m_window->windowIcon();
        if (icon.isNull() && qApp)
            icon = qApp->windowIcon();
        if (!icon.isNull())
            icon.paint(&painter, QRect(10, (height() - 20) / 2, 20, 20));
        int right = width();
        for (auto button : buttons)
            if (!button->isHidden())
                right -= 44;
        auto font = painter.font();
        font.setBold(m_config.value("boldTitle").toBool(false));
        painter.setFont(font);
        painter.drawText(QRect(38, 0, qMax(0, right - 48), height()), Qt::AlignVCenter,
                         painter.fontMetrics().elidedText(m_window->windowTitle(), Qt::ElideRight, qMax(0, right - 48)));
    }
    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && m_window->windowHandle())
            m_window->windowHandle()->startSystemMove();
#ifdef Q_OS_WIN
        if (event->button() == Qt::RightButton) {
            auto hwnd = reinterpret_cast<HWND>(m_window->winId());
            POINT point;
            GetCursorPos(&point);
            const auto command =
                TrackPopupMenu(GetSystemMenu(hwnd, FALSE), TPM_RETURNCMD | TPM_RIGHTBUTTON, point.x, point.y, 0, hwnd, nullptr);
            if (command)
                PostMessage(hwnd, WM_SYSCOMMAND, command, 0);
        }
#endif
    }
    void mouseDoubleClickEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton)
            toggleMaximize();
    }

   private:
    QWidget* m_window;
    QJsonObject m_config;
};
}  // namespace

WindowChromeController::WindowChromeController(QObject* parent) : QObject(parent) {}
WindowChromeController::~WindowChromeController()
{
    if (qApp && m_installed) {
        qApp->removeEventFilter(this);
        qApp->removeNativeEventFilter(this);
    }
}
bool WindowChromeController::supported()
{
#ifdef Q_OS_MACOS
    return false;  // Preserve native traffic lights, tabs and fullscreen behavior.
#else
    return true;
#endif
}
bool WindowChromeController::effectsSupported()
{
    // Qt can at least preserve opacity/translucent background on every
    // desktop platform. Native material support is selected at runtime.
    return true;
}
void WindowChromeController::ensureInstalled()
{
    if (m_installed || !qApp)
        return;
    m_installed = true;
    qApp->installEventFilter(this);
    qApp->installNativeEventFilter(this);
}
void WindowChromeController::initialize(bool enabled, const QJsonObject& configuration)
{
    if (enabled && !supported())
        return;
    if (!enabled && m_enabled) {
        // Restore the native frame and the exact margins captured by attach().
        // This makes decoration changes reversible without restarting Qt.
        const auto windows = m_bars.keys();
        for (auto window : windows)
            detach(window);
    }
    m_enabled = enabled;
    m_config = configuration;
    if (enabled) {
        ensureInstalled();
        for (auto window : QApplication::topLevelWidgets())
            attach(window);
        for (auto it = m_bars.cbegin(); it != m_bars.cend(); ++it) {
            if (auto bar = it.value()) {
                static_cast<CaptionBar*>(bar.data())->setConfig(m_config);
                relayoutWindow(it.key());
            }
        }
    }
}
void WindowChromeController::setEffects(const QJsonObject& effects)
{
    m_effects = effects;
    if (!effectsSupported())
        return;
    if (!m_effects.isEmpty()) {
        ensureInstalled();
        for (auto window : QApplication::topLevelWidgets()) {
            applyEffects(window);
            relayoutWindow(window);
        }
    } else {
        // Clear effects on windows that were already materialized. Keep the
        // event filter installed so newly-created windows are handled too if
        // a subsequent theme asks for effects.
        for (auto window : QApplication::topLevelWidgets()) {
            applyEffects(window);
            relayoutWindow(window);
        }
    }
}
void WindowChromeController::relayoutWindows()
{
    for (auto window : QApplication::topLevelWidgets())
        relayoutWindow(window);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
    for (auto window : QApplication::topLevelWidgets())
        if (auto layout = window->layout())
            layout->activate();
}
void WindowChromeController::attach(QWidget* window)
{
    if (!window || m_bars.contains(window) || (!qobject_cast<QDialog*>(window) && !qobject_cast<QMainWindow*>(window)))
        return;
    if (!window->isWindow() || window->windowType() == Qt::Popup || window->windowType() == Qt::Tool ||
        window->windowFlags().testFlag(Qt::FramelessWindowHint))
        return;
    if (window->objectName().contains("neo", Qt::CaseInsensitive) || window->property("lunaNativeDecoration").toBool())
        return;
    // Reserve space in the existing layout; its widgets and ownership stay intact.
    auto bar = new CaptionBar(window, m_config);
    m_bars.insert(window, bar);
    connect(window, &QObject::destroyed, this, [this, window] { m_bars.remove(window); });
    const bool visible = window->isVisible();
    auto margins = window->contentsMargins();
    window->setProperty("_lunaChromeMargins", QVariant::fromValue(margins));
    margins.setTop(margins.top() + bar->height());
    window->setContentsMargins(margins);
    window->setWindowFlag(Qt::FramelessWindowHint, true);
    if (window->minimumWidth() < window->maximumWidth())
        window->setMinimumWidth(qMin(window->maximumWidth(), qMax(window->minimumWidth(), 220)));
    // QWizard lays out its page header and button box from its private layout
    // and can ignore the top-level QWidget contents rect. Keep a matching
    // layout margin for wizard windows so the subtitle starts below the Luna
    // caption instead of being painted underneath it.
    if (qobject_cast<QWizard*>(window)) {
        if (auto layout = window->layout()) {
            window->setProperty("_lunaChromeLayoutMargins", QVariant::fromValue(layout->contentsMargins()));
            auto layoutMargins = layout->contentsMargins();
            layoutMargins.setTop(layoutMargins.top() + bar->height());
            layout->setContentsMargins(layoutMargins);
        }
    }
    bar->setGeometry(0, 0, window->width(), bar->height());
    bar->show();
    bar->raise();
#ifdef Q_OS_WIN
    const auto hwnd = reinterpret_cast<HWND>(window->winId());
    auto flags = GetWindowLongPtr(hwnd, GWL_STYLE) | WS_THICKFRAME | WS_CAPTION | WS_SYSMENU;
    if (window->windowFlags().testFlag(Qt::WindowMinimizeButtonHint))
        flags |= WS_MINIMIZEBOX;
    if (window->windowFlags().testFlag(Qt::WindowMaximizeButtonHint))
        flags |= WS_MAXIMIZEBOX;
    SetWindowLongPtr(hwnd, GWL_STYLE, flags);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
#endif
    applyEffects(window);
    relayoutWindow(window);
    if (visible)
        window->show();
}
void WindowChromeController::detach(QWidget* window)
{
    if (!window)
        return;
    auto bar = m_bars.take(window);
    if (!bar)
        return;
    const bool visible = window->isVisible();
    const auto baseMargins = window->property("_lunaChromeMargins");
    if (baseMargins.isValid())
        window->setContentsMargins(baseMargins.value<QMargins>());
    window->setProperty("_lunaChromeMargins", QVariant());
    if (qobject_cast<QWizard*>(window)) {
        if (auto layout = window->layout()) {
            const auto baseLayout = window->property("_lunaChromeLayoutMargins");
            if (baseLayout.isValid())
                layout->setContentsMargins(baseLayout.value<QMargins>());
        }
        window->setProperty("_lunaChromeLayoutMargins", QVariant());
    }
    delete bar.data();
    window->setWindowFlag(Qt::FramelessWindowHint, false);
#ifdef Q_OS_WIN
    const auto hwnd = reinterpret_cast<HWND>(window->winId());
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
#endif
    window->updateGeometry();
    if (auto layout = window->layout()) {
        layout->invalidate();
        layout->activate();
    }
    if (visible)
        window->show();
}
void WindowChromeController::relayoutWindow(QWidget* window)
{
    if (!window)
        return;
    auto bar = m_bars.value(window);
    if (!bar)
        return;
    bar->setGeometry(0, 0, window->width(), bar->height());
    bar->setVisible(!window->isFullScreen());
    const auto base = window->property("_lunaChromeMargins");
    if (base.isValid()) {
        auto margins = base.value<QMargins>();
        margins.setTop(margins.top() + (window->isFullScreen() ? 0 : bar->height()));
        if (window->contentsMargins() != margins)
            window->setContentsMargins(margins);
    }
    if (qobject_cast<QWizard*>(window)) {
        if (auto layout = window->layout()) {
            auto baseLayout = window->property("_lunaChromeLayoutMargins");
            // QWizard creates its private layout after Polish on some Qt
            // versions. Capture its original margins at the first relayout;
            // otherwise the wizard header is painted underneath the caption.
            if (!baseLayout.isValid()) {
                baseLayout = QVariant::fromValue(layout->contentsMargins());
                window->setProperty("_lunaChromeLayoutMargins", baseLayout);
            }
            auto layoutMargins = baseLayout.value<QMargins>();
            layoutMargins.setTop(layoutMargins.top() + (window->isFullScreen() ? 0 : bar->height()));
            if (layout->contentsMargins() != layoutMargins)
                layout->setContentsMargins(layoutMargins);
        }
    }
    static_cast<CaptionBar*>(bar.data())->sync();
    bar->raise();
    window->updateGeometry();
    if (auto layout = window->layout()) {
        layout->invalidate();
        layout->activate();
    }
}
void WindowChromeController::applyEffects(QWidget* window)
{
    if (!window || !window->isWindow() || window->windowType() == Qt::Popup || window->windowType() == Qt::Tool)
        return;
    if (m_applyingEffects)
        return;
    QScopedValueRollback<bool> applying(m_applyingEffects, true);

    const bool enabled = !m_effects.isEmpty() && (!m_effects.contains("composite") || m_effects.value("composite").toBool());
    const bool translucent = enabled && m_effects.value("translucentWindows").toBool();
    window->setProperty("_lunaWindowEffects", enabled ? m_effects : QJsonObject());
    if (!window->property("_lunaBaseWindowOpacity").isValid())
        window->setProperty("_lunaBaseWindowOpacity", window->windowOpacity());
    const auto baseOpacity = window->property("_lunaBaseWindowOpacity").toDouble();
    const int reduction = enabled ? qBound(0, m_effects.value("reduceWindowOpacity").toInt(), 100) : 0;
    const bool hasUserOpacity = enabled && m_effects.contains("windowOpacity");
    const int userOpacity = hasUserOpacity ? qBound(10, m_effects.value("windowOpacity").toInt(100), 100) : 100;
    const double requestedOpacity = hasUserOpacity ? userOpacity / 100.0 : (1.0 - reduction / 100.0);
    const auto effectiveOpacity = qBound(0.05, baseOpacity * requestedOpacity, 1.0);
    const int effectBlur = enabled ? qBound(0, m_effects.value("blurStrength").toInt(100), 100) : 0;
    const bool nativeBackdropHint = enabled && effectBlur > 0 && m_effects.value("backdrop").toString().compare("none", Qt::CaseInsensitive) != 0;
    // Avoid creating a layered window for the normal 100% case. Layered
    // windows prevent DWM's backdrop from being visible on several Windows
    // 10/11 builds. Restore the original opacity when effects are cleared.
    // Native DWM materials and Qt layered opacity do not compose reliably:
    // keep the window unlayered while Mica/Acrylic is active and pass the
    // requested opacity through the native tint below.
    if ((effectiveOpacity < 0.999 && !nativeBackdropHint) || (!enabled && qAbs(window->windowOpacity() - baseOpacity) > 0.001))
        window->setWindowOpacity(effectiveOpacity);
    else if (qAbs(window->windowOpacity() - baseOpacity) > 0.001)
        window->setWindowOpacity(baseOpacity);

    const auto requestedBackdrop = enabled ? m_effects.value("backdrop").toString().toLower() : QStringLiteral("none");
    const int blur = effectBlur;
    // A zero blur value explicitly disables the theme's desktop material.
    // Native DWM does not expose a blur radius; this gives the slider a
    // meaningful endpoint while positive values select the requested material.
    const auto backdrop = requestedBackdrop != "none" && blur == 0 ? QStringLiteral("none") : requestedBackdrop;
    const bool rounded = enabled && m_effects.value("roundedCorners").toBool(true);
    const bool glass = enabled && (translucent || backdrop != "none" || (hasUserOpacity && userOpacity < 100));
    // Qt documents WA_TranslucentBackground as an early, creation-time
    // attribute: toggling it after a window is shown is not uniformly
    // supported. Only prepare it for windows that actually need a translucent
    // surface. Preparing every window (including the normal opaque case)
    // changes Qt's backing-store clearing behavior and can leave stale page
    // pixels behind after a stacked-page switch.
    if (glass && !window->property("_lunaTranslucentBackgroundPrepared").isValid()) {
        if (!window->isVisible())
            window->setAttribute(Qt::WA_TranslucentBackground, true);
        window->setProperty("_lunaTranslucentBackgroundPrepared", true);
    }

#ifdef Q_OS_WIN
    // The constants below are available in newer Windows SDKs only. Keeping
    // numeric fallbacks lets the same binary use DWM materials on older SDKs.
    constexpr DWORD DWMWA_USE_IMMERSIVE_DARK_MODE_COMPAT = 20;
    constexpr DWORD DWMWA_SYSTEMBACKDROP_TYPE_COMPAT = 38;
    constexpr DWORD DWMWA_WINDOW_CORNER_PREFERENCE_COMPAT = 33;
    constexpr DWORD DWMWA_MICA_EFFECT_COMPAT = 1029;  // Win11 22000 fallback
    constexpr int DWMWCP_DEFAULT_COMPAT = 0;
    constexpr int DWMWCP_ROUND_COMPAT = 2;
    constexpr int DWMSBT_NONE_COMPAT = 1;
    constexpr int DWMSBT_MAINWINDOW_COMPAT = 2;  // Mica
    constexpr int DWMSBT_TRANSIENTWINDOW_COMPAT = 3;  // Acrylic
    using DwmSetWindowAttributeFn = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
    struct MarginsCompat {
        int cxLeftWidth;
        int cxRightWidth;
        int cyTopHeight;
        int cyBottomHeight;
    };
    using DwmExtendFrameIntoClientAreaFn = HRESULT(WINAPI*)(HWND, const MarginsCompat*);
    // Windows 10 has no SYSTEMBACKDROP_TYPE. Its supported equivalent is the
    // composition accent policy, loaded at runtime so Windows 7/8 still run.
    struct AccentPolicy {
        int state;
        int flags;
        int colour;
        int animation;
    };
    struct WindowCompositionAttributeData {
        int attribute;
        void* data;
        size_t size;
    };
    using SetWindowCompositionAttributeFn = BOOL(WINAPI*)(HWND, WindowCompositionAttributeData*);
    static auto dwm = [] {
        const auto module = LoadLibraryW(L"dwmapi.dll");
        return resolveWindowsFunction<DwmSetWindowAttributeFn>(module, "DwmSetWindowAttribute");
    }();
    static auto extendFrame = [] {
        const auto module = LoadLibraryW(L"dwmapi.dll");
        return resolveWindowsFunction<DwmExtendFrameIntoClientAreaFn>(module, "DwmExtendFrameIntoClientArea");
    }();
    static auto setComposition = [] {
        const auto module = LoadLibraryW(L"user32.dll");
        return resolveWindowsFunction<SetWindowCompositionAttributeFn>(module, "SetWindowCompositionAttribute");
    }();
    const auto hwnd = reinterpret_cast<HWND>(window->winId());
    if (dwm) {
        int backdropType = DWMSBT_NONE_COMPAT;
        if (backdrop == "mica")
            backdropType = DWMSBT_MAINWINDOW_COMPAT;
        else if (backdrop == "acrylic")
            backdropType = DWMSBT_TRANSIENTWINDOW_COMPAT;
        const BOOL dark = qApp->palette().window().color().lightness() < 128;
        const int corners = rounded ? DWMWCP_ROUND_COMPAT : DWMWCP_DEFAULT_COMPAT;
        dwm(hwnd, DWMWA_SYSTEMBACKDROP_TYPE_COMPAT, &backdropType, sizeof(backdropType));
        dwm(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE_COMPAT, &corners, sizeof(corners));
        dwm(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE_COMPAT, &dark, sizeof(dark));
        if (enabled && backdrop != "none") {
            const BOOL mica = backdrop == "mica";
            dwm(hwnd, DWMWA_MICA_EFFECT_COMPAT, &mica, sizeof(mica));
        } else {
            const BOOL mica = FALSE;
            dwm(hwnd, DWMWA_MICA_EFFECT_COMPAT, &mica, sizeof(mica));
        }
        if (extendFrame) {
            const MarginsCompat margins = enabled && backdrop != "none" ? MarginsCompat{ -1, -1, -1, -1 } : MarginsCompat{ 0, 0, 0, 0 };
            extendFrame(hwnd, &margins);
        }
    }
    if (setComposition) {
        AccentPolicy policy{};
        // ACCENT_ENABLE_ACRYLICBLURBEHIND (4) is the Windows 10 fallback.
        // A zero blur value intentionally falls back to the transparent
        // gradient state so the slider has an observable effect.
        policy.state = backdrop == "acrylic" ? (blur > 0 ? 4 : 2) : 0;
        policy.flags = 2;
        const auto colour = qApp->palette().window().color();
        // AccentPolicy's alpha is the tint opacity, not the whole window's
        // opacity. Keep it below opaque so the compositor's blur remains
        // visible even when the user's window opacity is 100%.
        const auto alpha = backdrop == "acrylic" ? qBound(24, 48 + userOpacity * (70 + blur) / 100, 230) : 0;
        policy.colour = (alpha << 24) | (colour.blue() << 16) | (colour.green() << 8) | colour.red();
        WindowCompositionAttributeData data{ 19, &policy, sizeof(policy) };
        setComposition(hwnd, &data);
    }
#else
    // Keep the translucent-background attribute stable on non-Windows too;
    // platform compositors consume the same Qt surface without DWM calls.
    Q_UNUSED(window);
#endif
}
bool WindowChromeController::eventFilter(QObject* watched, QEvent* event)
{
    auto window = qobject_cast<QWidget*>(watched);
    if (!window)
        return false;
    if (event->type() == QEvent::Polish && window->isWindow() && m_enabled)
        attach(window);
    if (window->isWindow() && (event->type() == QEvent::Polish || event->type() == QEvent::Show ||
                               event->type() == QEvent::WinIdChange || event->type() == QEvent::WindowStateChange))
        applyEffects(window);
    auto bar = m_bars.value(window);
    if (!bar)
        return false;
    if (event->type() == QEvent::Resize || event->type() == QEvent::WindowStateChange || event->type() == QEvent::Show ||
        event->type() == QEvent::StyleChange || event->type() == QEvent::FontChange || event->type() == QEvent::LayoutRequest) {
        relayoutWindow(window);
        if (event->type() == QEvent::Show) {
            // QDialog computes its layout minimum during the first show, after
            // Polish. Reserve title space once that calculation has completed.
            QTimer::singleShot(0, window, [window] {
                if (window->maximumWidth() >= 220 && window->minimumWidth() < 220)
                    window->setMinimumWidth(220);
            });
        }
    }
    if (event->type() == QEvent::ActivationChange || event->type() == QEvent::PaletteChange)
        bar->update();
#ifndef Q_OS_WIN
    if (event->type() == QEvent::MouseButtonPress && !window->isMaximized() && window->windowHandle()) {
        const auto mouse = static_cast<QMouseEvent*>(event);
        const auto point = mouse->position();
        Qt::Edges edges;
        if (point.x() < 6)
            edges |= Qt::LeftEdge;
        if (point.x() > window->width() - 6)
            edges |= Qt::RightEdge;
        if (point.y() < 6)
            edges |= Qt::TopEdge;
        if (point.y() > window->height() - 6)
            edges |= Qt::BottomEdge;
        if (edges && mouse->button() == Qt::LeftButton)
            return window->windowHandle()->startSystemResize(edges);
    }
#endif
    return false;
}
bool WindowChromeController::nativeEventFilter(const QByteArray&, void* message, qintptr* result)
{
#ifdef Q_OS_WIN
    auto msg = static_cast<MSG*>(message);
    QWidget* window = QWidget::find(reinterpret_cast<WId>(msg->hwnd));
    if (!window || !m_bars.contains(window))
        return false;
    if (msg->message == WM_NCCALCSIZE && msg->wParam) {
        if (IsZoomed(msg->hwnd)) {
            auto params = reinterpret_cast<NCCALCSIZE_PARAMS*>(msg->lParam);
            MONITORINFO monitor{ sizeof(MONITORINFO) };
            if (GetMonitorInfo(MonitorFromWindow(msg->hwnd, MONITOR_DEFAULTTONEAREST), &monitor))
                params->rgrc[0] = monitor.rcWork;
        }
        *result = 0;
        return true;
    }
    if (msg->message == WM_NCHITTEST) {
        RECT r;
        GetWindowRect(msg->hwnd, &r);
        const POINT p{ GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam) };
        const int edge = qRound(6 * window->devicePixelRatioF());
        if (!IsZoomed(msg->hwnd) && window->minimumSize() != window->maximumSize()) {
            const bool left = p.x < r.left + edge, right = p.x >= r.right - edge, top = p.y < r.top + edge, bottom = p.y >= r.bottom - edge;
            if (top || bottom || left || right) {
                *result = top      ? (left    ? HTTOPLEFT
                                      : right ? HTTOPRIGHT
                                              : HTTOP)
                          : bottom ? (left    ? HTBOTTOMLEFT
                                      : right ? HTBOTTOMRIGHT
                                              : HTBOTTOM)
                          : left   ? HTLEFT
                                   : HTRIGHT;
                return true;
            }
        }
        auto bar = static_cast<CaptionBar*>(m_bars.value(window).data());
        POINT local = p;
        ScreenToClient(msg->hwnd, &local);
        if (bar && bar->buttons[1]->isVisible() &&
            bar->buttons[1]->rect().contains(bar->buttons[1]->mapFrom(window, QPoint(local.x, local.y) / window->devicePixelRatioF()))) {
            *result = HTMAXBUTTON;
            return true;
        }
    }
    if ((msg->message == WM_NCLBUTTONUP || msg->message == WM_NCLBUTTONDOWN) && msg->wParam == HTMAXBUTTON) {
        if (msg->message == WM_NCLBUTTONUP)
            static_cast<CaptionBar*>(m_bars.value(window).data())->toggleMaximize();
        *result = 0;
        return true;
    }
#else
    Q_UNUSED(message);
    Q_UNUSED(result);
#endif
    return false;
}
