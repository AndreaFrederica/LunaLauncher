/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
*/
#pragma once
#include "ui/pages/BasePage.h"
#include <QWidget>
#include <memory>
#include "QObjectPtr.h"

class QTermWidget;
class ServerConsoleWidget;
class ServerInstance;
class ServerLaunchTask;

class ServerConsolePage : public QWidget, public BasePage
{
    Q_OBJECT

public:
    explicit ServerConsolePage(ServerInstance *instance, QWidget *parent = nullptr);
    virtual ~ServerConsolePage() override;

    QString displayName() const override;
    QIcon icon() const override;
    QString id() const override;
    QString helpPage() const override;
    bool shouldDisplay() const override;

protected:
    void openedImpl() override;
    void closedImpl() override;

private slots:
    void onRunningStatusChanged(bool running);
    void onReadyRead(const QByteArray &data);
    void onSendData(const char *data, int size);
    void onTermSizeChange(int lines, int columns);

private:
    void connectToTask(ServerLaunchTask* task);
    void disconnectConsole();

private:
    ServerInstance *m_instance;
    ServerConsoleWidget *m_console = nullptr;
    ServerLaunchTask* m_currentTask = nullptr;
};
