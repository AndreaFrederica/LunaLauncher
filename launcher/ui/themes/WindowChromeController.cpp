// SPDX-License-Identifier: GPL-3.0-only
#include "WindowChromeController.h"
#include <QApplication>
#include <QDialog>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QWindow>
#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#endif

namespace {
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
        painter.fillRect(rect(), background.isValid()
                                     ? background
                                     : QApplication::palette().color(active ? QPalette::Active : QPalette::Inactive, QPalette::Window));
        painter.setPen(foreground.isValid()
                           ? foreground
                           : QApplication::palette().color(active ? QPalette::Active : QPalette::Inactive, QPalette::WindowText));
        if (!m_window->windowIcon().isNull())
            m_window->windowIcon().paint(&painter, QRect(10, (height() - 20) / 2, 20, 20));
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
    if (qApp) {
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
void WindowChromeController::initialize(bool enabled, const QJsonObject& configuration)
{
    if (m_enabled || !enabled || !supported())
        return;
    m_enabled = true;
    m_config = configuration;
    qApp->installEventFilter(this);
    qApp->installNativeEventFilter(this);
    for (auto window : QApplication::topLevelWidgets())
        attach(window);
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
    if (visible)
        window->show();
}
bool WindowChromeController::eventFilter(QObject* watched, QEvent* event)
{
    auto window = qobject_cast<QWidget*>(watched);
    if (!window)
        return false;
    if (event->type() == QEvent::Polish && window->isWindow())
        attach(window);
    auto bar = m_bars.value(window);
    if (!bar)
        return false;
    if (event->type() == QEvent::Resize || event->type() == QEvent::WindowStateChange || event->type() == QEvent::Show) {
        bar->setGeometry(0, 0, window->width(), bar->height());
        bar->setVisible(!window->isFullScreen());
        auto margins = window->property("_lunaChromeMargins").value<QMargins>();
        margins.setTop(margins.top() + (window->isFullScreen() ? 0 : bar->height()));
        window->setContentsMargins(margins);
        static_cast<CaptionBar*>(bar.data())->sync();
        bar->raise();
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
