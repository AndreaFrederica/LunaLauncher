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
    void initialize(bool enabled, const QJsonObject& configuration = {});
    bool eventFilter(QObject* watched, QEvent* event) override;
    bool nativeEventFilter(const QByteArray&, void* message, qintptr* result) override;

   private:
    void attach(QWidget* window);
    bool m_enabled = false;
    QJsonObject m_config;
    QHash<QWidget*, QPointer<QWidget>> m_bars;
};
