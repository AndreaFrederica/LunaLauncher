/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
*/
#pragma once
#include "launch/LaunchTask.h"
#include <ptyqt/ptyqt.h>
#include <memory>
#include <QTimer>
#include <QElapsedTimer>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class ServerInstance;

class ServerLaunchTask : public LaunchTask
{
    Q_OBJECT
public:
    explicit ServerLaunchTask(ServerInstance *instance);
    virtual ~ServerLaunchTask();

    // Core API: start/stop the server
    void executeTask() override;
    bool stop(bool force = false);
    void cleanup();

    // Override to match LaunchTask behavior
    void emitSucceeded() override;
    void emitFailed(QString reason) override;

    // Console I/O
    void writeToStdin(const QByteArray &data);
    void resizePty(int cols, int rows);

    // Status queries
    bool canStop() const;
    bool stopServer(bool force = false);  // Returns true if stopped successfully

signals:
    void readyRead(const QByteArray &data);

private slots:
    void onPtyRead();
    void onPtyExit();
    void checkProcessExit();

private:
    bool isProcessRunning(qint64 pid);
    bool waitForProcessExit(qint64 pid, int timeoutMs);
    bool terminateProcessTree();
    void collectProcessTree(const IPtyProcess::pidTree_t &tree, QList<qint64> &pids);
    void parseCommandLine(const QString &fullCmd, const QString &workDir, QString &cmd, QStringList &args);
    void wrapBatchFile(QString &cmd, QStringList &args);

private:
    ServerInstance *m_instance;
    std::unique_ptr<IPtyProcess> m_ptyProcess;
    std::unique_ptr<QTimer> m_exitCheckTimer;
    bool m_finished = false;
    bool m_stopping = false;
};
