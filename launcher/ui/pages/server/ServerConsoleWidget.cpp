/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
*/
#include "ServerConsoleWidget.h"
#include <qtermwidget.h>
#include <QVBoxLayout>
#include <QFont>
#include "Application.h"

ServerConsoleWidget::ServerConsoleWidget(QWidget *parent) : QWidget(parent)
{
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_console = new QTermWidget(0, this);

    auto settings = APPLICATION->settings();
    QString family = settings->get("ConsoleFont").toString();
    bool ok = true;
    int size = settings->get("ConsoleFontSize").toInt(&ok);
    if (!ok || size <= 0) {
        size = 12;
    }
    if (family.isEmpty()) {
        family = "Monospace";
    }
    QFont font(family, size);
    font.setStyleHint(QFont::TypeWriter, QFont::PreferAntialias);
    m_console->setTerminalFont(font);
    QString scheme = settings->get("ConsoleColorScheme").toString();
    if (scheme.isEmpty()) {
        scheme = "Builtin Dark";
    }
    const auto schemes = QTermWidget::availableColorSchemes();
    if (!schemes.contains(scheme)) {
        scheme = "Builtin Dark";
    }
    m_console->setColorScheme(scheme);
    m_console->setScrollBarPosition(QTermWidget::ScrollBarRight);

    layout->addWidget(m_console);

    connect(m_console, &QTermWidget::sendData, this, &ServerConsoleWidget::sendData);
    connect(m_console, &QTermWidget::termSizeChange, this, &ServerConsoleWidget::termSizeChange);
}

ServerConsoleWidget::~ServerConsoleWidget() {}

void ServerConsoleWidget::writeData(const QByteArray &data)
{
    m_console->recvData(data.constData(), data.size());
}

int ServerConsoleWidget::columns() const { return m_console->screenColumnsCount(); }
int ServerConsoleWidget::lines() const { return m_console->screenLinesCount(); }
