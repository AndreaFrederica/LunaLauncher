/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
*/
#pragma once
#include "launch/LaunchTask.h"
#include <ptyqt/ptyqt.h>
#include <memory>

class ServerInstance;

class ServerLaunchTask : public LaunchTask
{
    Q_OBJECT
public:
    explicit ServerLaunchTask(ServerInstance *instance);
    virtual ~ServerLaunchTask();

    void executeTask() override;

public slots:
    void writeToStdin(const QByteArray &data);
    void resizePty(int cols, int rows);

signals:
    void readyRead(const QByteArray &data);

private slots:
    void onPtyRead();
    void onPtyExit();

private:
    ServerInstance *m_instance;
    std::unique_ptr<IPtyProcess> m_ptyProcess;
};
