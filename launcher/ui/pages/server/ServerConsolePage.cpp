/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
*/
#include "ServerConsolePage.h"
#include "ServerConsoleWidget.h"
#include "server/ServerInstance.h"
#include "server/ServerLaunchTask.h"
#include <QVBoxLayout>
#include <QDebug>

ServerConsolePage::ServerConsolePage(ServerInstance *instance, QWidget *parent)
    : QWidget(parent), m_instance(instance)
{
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_console = new ServerConsoleWidget(this);

    layout->addWidget(m_console);

    connect(m_instance, &ServerInstance::runningStatusChanged, this, &ServerConsolePage::onRunningStatusChanged);

    // Connect console output
    connect(m_console, &ServerConsoleWidget::sendData, this, &ServerConsolePage::onSendData);
    connect(m_console, &ServerConsoleWidget::termSizeChange, this, &ServerConsolePage::onTermSizeChange);
}

ServerConsolePage::~ServerConsolePage()
{
}

QString ServerConsolePage::displayName() const
{
    return tr("Console");
}

QIcon ServerConsolePage::icon() const
{
    return QIcon::fromTheme("utilities-terminal");
}

QString ServerConsolePage::id() const
{
    return "server-console";
}

QString ServerConsolePage::helpPage() const
{
    return "Server Console";
}

bool ServerConsolePage::shouldDisplay() const
{
    return true;
}

void ServerConsolePage::openedImpl()
{
    // Re-check running status
    onRunningStatusChanged(m_instance->isRunning());
}

void ServerConsolePage::closedImpl()
{
}

void ServerConsolePage::onRunningStatusChanged(bool running)
{
    // Disconnect previous task if any
    if (m_currentTask) {
         disconnect(m_currentTask.get(), &ServerLaunchTask::readyRead, this, &ServerConsolePage::onReadyRead);
    }

    if (running) {
        m_currentTask = m_instance->launchTask().dynamicCast<ServerLaunchTask>();

        if (m_currentTask) {
            connect(m_currentTask.get(), &ServerLaunchTask::readyRead, this, &ServerConsolePage::onReadyRead);
            // Sync initial size
            m_currentTask->resizePty(m_console->columns(), m_console->lines());
        }
    } else {
        m_currentTask.reset();
    }
}

void ServerConsolePage::onReadyRead(const QByteArray &data)
{
    if (m_console) {
        m_console->writeData(data);
    }
}

void ServerConsolePage::onSendData(const char *data, int size)
{
    if (m_currentTask) {
        m_currentTask->writeToStdin(QByteArray(data, size));
    }
}

void ServerConsolePage::onTermSizeChange(int lines, int columns)
{
    if (m_currentTask) {
        m_currentTask->resizePty(columns, lines);
    }
}
