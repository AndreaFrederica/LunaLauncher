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

    auto buttonBar = new QHBoxLayout();
    buttonBar->setContentsMargins(6, 6, 6, 0);
    auto clearBtn = new QPushButton(tr("Clear"), this);
    clearBtn->setIcon(QIcon::fromTheme("edit-clear"));
    buttonBar->addStretch();
    buttonBar->addWidget(clearBtn);
    layout->addLayout(buttonBar);

    m_console = new ServerConsoleWidget(this);
    layout->addWidget(m_console);

    // Connect to running status changes
    connect(m_instance, &ServerInstance::runningStatusChanged, this, &ServerConsolePage::onRunningStatusChanged);

    // Connect console I/O
    connect(m_console, &ServerConsoleWidget::sendData, this, &ServerConsolePage::onSendData);
    connect(m_console, &ServerConsoleWidget::termSizeChange, this, &ServerConsolePage::onTermSizeChange);
    connect(clearBtn, &QPushButton::clicked, m_console, &ServerConsoleWidget::clear);
}

ServerConsolePage::~ServerConsolePage()
{
    disconnectConsole();
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
    // Re-check running status when page opens
    onRunningStatusChanged(m_instance->isRunning());
}

void ServerConsolePage::closedImpl()
{
    // Keep connection alive when page is closed
}

void ServerConsolePage::onRunningStatusChanged(bool running)
{
    qDebug() << "ServerConsolePage::onRunningStatusChanged - running:" << running;

    if (running) {
        // Connect to the current task
        connectToTask(dynamic_cast<ServerLaunchTask*>(m_instance->launchTask()));
    } else {
        // Disconnect from any task
        disconnectConsole();
    }
}

void ServerConsolePage::connectToTask(ServerLaunchTask* task)
{
    if (!task) {
        qWarning() << "ServerConsolePage::connectToTask - task is null";
        return;
    }

    // Disconnect from previous task first
    disconnectConsole();

    m_currentTask = task;
    qDebug() << "ServerConsolePage::connectToTask - connected to task";

    // Connect to console output
    connect(task, &ServerLaunchTask::readyRead, this, &ServerConsolePage::onReadyRead);

    // Immediately sync terminal size to prevent cursor positioning issues
    // This must happen BEFORE any output is displayed
    int cols = m_console->columns();
    int rows = m_console->lines();
    qDebug() << "ServerConsolePage::connectToTask - syncing terminal size:" << cols << "x" << rows;
    task->resizePty(cols, rows);
}

void ServerConsolePage::disconnectConsole()
{
    if (m_currentTask) {
        qDebug() << "ServerConsolePage::disconnectConsole - disconnecting from task";
        disconnect(m_currentTask, &ServerLaunchTask::readyRead, this, &ServerConsolePage::onReadyRead);
        m_currentTask = nullptr;
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
