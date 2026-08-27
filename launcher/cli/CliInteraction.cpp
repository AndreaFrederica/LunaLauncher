// SPDX-License-Identifier: GPL-3.0-only

#include "CliInteraction.h"

#include <QJsonObject>
#include <QTextStream>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

CliInteraction::CliInteraction(bool nonInteractive, bool passwordStdin) : m_nonInteractive(nonInteractive), m_passwordStdin(passwordStdin)
{}

void CliInteraction::status(const QString& message)
{
    QTextStream(stderr) << message << Qt::endl;
}

void CliInteraction::deviceCode(const QString& url, const QString& code, int expiresIn)
{
    QTextStream(stderr) << "Open " << url << " and enter code " << code << " (expires in " << expiresIn << " seconds)." << Qt::endl;
}

QString CliInteraction::readLine(bool secret)
{
    QTextStream in(stdin);
    if (!secret)
        return in.readLine();

#ifdef Q_OS_WIN
    const HANDLE handle = GetStdHandle(STD_INPUT_HANDLE);
    DWORD oldMode = 0;
    const bool terminal = handle != INVALID_HANDLE_VALUE && GetConsoleMode(handle, &oldMode);
    if (terminal)
        SetConsoleMode(handle, oldMode & ~ENABLE_ECHO_INPUT);
    const auto value = in.readLine();
    if (terminal) {
        SetConsoleMode(handle, oldMode);
        QTextStream(stderr) << Qt::endl;
    }
#else
    termios oldState{};
    const bool terminal = tcgetattr(STDIN_FILENO, &oldState) == 0;
    if (terminal) {
        auto state = oldState;
        state.c_lflag &= ~ECHO;
        tcsetattr(STDIN_FILENO, TCSANOW, &state);
    }
    const auto value = in.readLine();
    if (terminal) {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldState);
        QTextStream(stderr) << Qt::endl;
    }
#endif
    return value;
}

std::optional<QString> CliInteraction::input(const QString& prompt, bool secret)
{
    if (secret && m_passwordStdin && !m_passwordConsumed) {
        m_passwordConsumed = true;
        const auto value = readLine(false);
        return value.isNull() ? std::nullopt : std::optional<QString>(value);
    }
    if (m_nonInteractive)
        return std::nullopt;

    QTextStream(stderr) << prompt << (secret ? ": " : ": ") << Qt::flush;
    const auto value = readLine(secret);
    return value.isNull() ? std::nullopt : std::optional<QString>(value);
}

std::optional<int> CliInteraction::select(const QString& prompt, const QJsonArray& choices)
{
    if (m_nonInteractive)
        return std::nullopt;

    auto err = QTextStream(stderr);
    err << prompt << Qt::endl;
    for (int i = 0; i < choices.size(); ++i) {
        const auto choice = choices.at(i).toObject();
        err << "  " << (i + 1) << ". " << choice.value("name").toString() << " [" << choice.value("id").toString() << "]" << Qt::endl;
    }
    err << "Selection: " << Qt::flush;
    bool ok = false;
    const int selection = readLine(false).toInt(&ok);
    if (!ok || selection < 1 || selection > choices.size())
        return std::nullopt;
    return selection - 1;
}
