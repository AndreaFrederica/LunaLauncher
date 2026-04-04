/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
*/
#include "ServerConsoleWidget.h"
#include <qtermwidget.h>
#include <QVBoxLayout>
#include <QFont>
#include "Application.h"
#include "settings/SettingsObject.h"

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

    // Validate and fallback font family
    static const QStringList monospaceFonts = {
        "Consolas",        // Windows 7+
        "Courier New",     // Windows 默认
        "Lucida Console",  // Windows
        "Microsoft YaHei Mono",
        "Cascadia Mono",
        "DejaVu Sans Mono",
        "Ubuntu Mono",
        "Monaco",
        "Menlo",
        "Source Code Pro",
        "Fira Code",
        "JetBrains Mono",
        "Monospace"
    };

    if (family.isEmpty()) {
        family = monospaceFonts.first();
        qDebug() << "ServerConsoleWidget: No font configured, using:" << family;
    } else {
        // Validate the configured font exists and is fixed pitch
        QFont testFont(family, size);
        QFontInfo info(testFont);
        qDebug() << "ServerConsoleWidget: Requested font:" << family << "size:" << size;
        qDebug() << "ServerConsoleWidget: QFontInfo family:" << info.family() << "fixedPitch:" << info.fixedPitch();
        // If font doesn't exist or isn't fixed pitch, try to find a valid one
        if (info.family() != family || !info.fixedPitch()) {
            qDebug() << "ServerConsoleWidget: Font validation failed, searching for fallback";
            QString validFont;
            for (const QString &fontName : monospaceFonts) {
                QFont f(fontName, size);
                QFontInfo fi(f);
                qDebug() << "ServerConsoleWidget: Checking font:" << fontName << "- family:" << fi.family() << "fixedPitch:" << fi.fixedPitch();
                if (fi.fixedPitch() && fi.family() == fontName) {
                    validFont = fontName;
                    break;
                }
            }
            if (!validFont.isEmpty()) {
                family = validFont;
                qDebug() << "ServerConsoleWidget: Found valid font:" << family;
            } else {
                family = monospaceFonts.first();
                qDebug() << "ServerConsoleWidget: No valid font found, using:" << family;
            }
        } else {
            qDebug() << "ServerConsoleWidget: Font validation passed, using:" << family;
        }
    }
    QFont font(family, size);
    font.setStyleHint(QFont::TypeWriter, QFont::PreferAntialias);
    qDebug() << "ServerConsoleWidget: Setting terminal font to:" << font.family() << "pointSize:" << font.pointSize();
    m_console->setTerminalFont(font);

    // Try to set color scheme, with fallback for resource loading issues
    QString scheme = settings->get("ConsoleColorScheme").toString();
    if (scheme.isEmpty()) {
        scheme = "Builtin Dark";
    }
    const auto schemes = QTermWidget::availableColorSchemes();
    // Only use setColorScheme if we have schemes available
    // Otherwise use direct color setting for dark theme
    if (schemes.contains(scheme)) {
        m_console->setColorScheme(scheme);
    } else if (!schemes.isEmpty()) {
        // Try to find a dark scheme from available ones
        QString darkScheme;
        for (const QString &s : schemes) {
            if (s.contains("Dark", Qt::CaseInsensitive) || s.contains("Night", Qt::CaseInsensitive)) {
                darkScheme = s;
                break;
            }
        }
        m_console->setColorScheme(darkScheme.isEmpty() ? schemes.first() : darkScheme);
    } else {
        // Fallback: direct color setting when resource loading fails
        // Only set background and foreground colors, avoid ANSI color palette issues
        m_console->setBackgroundColor(QColor("#1a1a1a"));
        m_console->setForegroundColor(QColor("#e0e0e0"));
    }
    m_console->setScrollBarPosition(QTermWidget::ScrollBarRight);

    layout->addWidget(m_console);

    connect(m_console, &QTermWidget::sendData, this, &ServerConsoleWidget::sendData);
    connect(m_console, &QTermWidget::termSizeChange, this, &ServerConsoleWidget::termSizeChange);

    // Log initial terminal size for debugging
    qDebug() << "ServerConsoleWidget: Initial terminal size:" << m_console->screenColumnsCount() << "x" << m_console->screenLinesCount();
}

ServerConsoleWidget::~ServerConsoleWidget() {}

void ServerConsoleWidget::writeData(const QByteArray &data)
{
    m_console->recvData(data.constData(), data.size());
}

int ServerConsoleWidget::columns() const { return m_console->screenColumnsCount(); }
int ServerConsoleWidget::lines() const { return m_console->screenLinesCount(); }

void ServerConsoleWidget::clear()
{
    int old = m_console->historySize();
    m_console->setHistorySize(0);
    m_console->setHistorySize(old);
    const char seq[] = "\x1B[2J\x1B[H";
    m_console->recvData(seq, sizeof(seq) - 1);
}
