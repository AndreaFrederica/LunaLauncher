/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
*/
#include "ServerLaunchTask.h"
#include "ServerInstance.h"
#include "FileSystem.h"
#include "minecraft/auth/AuthlibInjector.h"
#include <QDir>
#include <QCoreApplication>
#include <QEventLoop>
#include <QProcessEnvironment>
#include <QDebug>
#include <QProcess>
#include <QFileInfo>
#include <QThread>
#include <QUrl>
#include <QDateTime>
#include <cerrno>
#include <utility>
#ifndef Q_OS_WIN
#include <unistd.h>
#include <signal.h>
#endif

ServerLaunchTask::ServerLaunchTask(ServerInstance *instance)
    : LaunchTask(instance), m_instance(instance)
{
}

ServerLaunchTask::~ServerLaunchTask()
{
    cleanup();
}

void ServerLaunchTask::executeTask()
{
    qDebug() << "ServerLaunchTask::executeTask() - Beginning server start";

    // Set running state
    m_instance->setRunning(true);
    setStatus("Starting server...");

    // Create PTY process
    m_ptyProcess.reset(PtyQt::createPtyProcess());
    if (!m_ptyProcess) {
        qWarning() << "ServerLaunchTask: Failed to create PTY process";
        emitFailed("Failed to create PTY process");
        return;
    }

    // Connect PTY signals
    connect(m_ptyProcess->notifier(), &QIODevice::readyRead, this, &ServerLaunchTask::onPtyRead);
    connect(m_ptyProcess->notifier(), &QIODevice::readChannelFinished, this, &ServerLaunchTask::onPtyExit);

    // Prepare command line
    QString fullCmd = m_instance->executablePath();
    QStringList extraArgs = m_instance->arguments();

    // Java & Memory Replacements
    auto settings = m_instance->settings();
    QString javaPath = "java";
    if (settings->get("OverrideJavaLocation").toBool()) {
        javaPath = settings->get("JavaPath").toString();
    }

    // Force java instead of javaw for servers
    if (javaPath.endsWith("javaw.exe", Qt::CaseInsensitive)) {
        javaPath.chop(9);
        javaPath.append("java.exe");
    } else if (javaPath.endsWith("javaw", Qt::CaseInsensitive)) {
        javaPath.chop(5);
        javaPath.append("java");
    }

    QString minMem = settings->get("MinMemAlloc").toString();
    QString maxMem = settings->get("MaxMemAlloc").toString();
    QString permMem = settings->get("PermGen").toString();

    auto doReplace = [&](QString &str) {
        str.replace("$java", javaPath);
        str.replace("$min_memory", minMem);
        str.replace("$max_memory", maxMem);
        str.replace("$perm_memory", permMem);
    };

    doReplace(fullCmd);
    for (auto &arg : extraArgs) {
        doReplace(arg);
    }

    QString workDir = m_instance->workingDir();
    if (workDir.isEmpty()) {
        workDir = m_instance->instanceRoot();
    }

    // Parse command line
    QString cmd;
    QStringList args;
    parseCommandLine(fullCmd, workDir, cmd, args);
    args.append(extraArgs);

    // Server instances do not have a Minecraft account session, so their
    // authlib-injector settings are applied directly to the Java command.
    // The injector jar is shared with the normal launcher cache.
    if (m_instance->authlibInjectorEnabled()) {
        const auto apiUrl = m_instance->authlibInjectorApiUrl().trimmed();
        const QUrl api(apiUrl);
        if (apiUrl.isEmpty() || !api.isValid() || (api.scheme() != "http" && api.scheme() != "https") || api.host().isEmpty()) {
            emitFailed("Authlib Injector is enabled, but its API URL is invalid.");
            return;
        }
        auto injectorPath = AuthlibInjector::instance().getLocalPath();
        if (!AuthlibInjector::instance().checkCache())
            injectorPath = AuthlibInjector::instance().downloadLatest();
        if (injectorPath.isEmpty() || !QFileInfo::exists(injectorPath)) {
            emitFailed("Authlib Injector is enabled, but authlib-injector.jar is unavailable.");
            return;
        }

        QStringList injectorArgs;
        injectorArgs << QString("-javaagent:%1=%2").arg(injectorPath, apiUrl);
        const auto metadata = AuthlibInjector::instance().getYggdrasilMeta(apiUrl);
        if (!metadata.isEmpty())
            injectorArgs << QString("-Dauthlibinjector.yggdrasil.prefetched=%1").arg(metadata);

        const bool javaCommand = cmd.contains("java", Qt::CaseInsensitive);
        if (javaCommand) {
            int jarPos = args.indexOf("-jar");
            if (jarPos < 0)
                jarPos = 0;
            for (int i = injectorArgs.size() - 1; i >= 0; --i)
                args.insert(jarPos, injectorArgs.at(i));
        } else {
            emitFailed("Authlib Injector requires a Java server executable.");
            return;
        }
    }

    // Add JVM properties
    if (cmd.contains("java", Qt::CaseInsensitive) && args.contains("-jar", Qt::CaseInsensitive)) {
        int jarPos = args.indexOf("-jar");
        args.insert(jarPos, "-Dterminal.ansi=true");
        args.insert(jarPos, "-Dterminal.jline=false");
    }

    // Resolve relative path
    QFileInfo cmdInfo(cmd);
    if (cmdInfo.isRelative()) {
        QString absPath = FS::PathCombine(workDir, cmd);
        if (QFileInfo::exists(absPath)) {
            cmd = absPath;
        }
    }

    // Windows: wrap batch files
#ifdef Q_OS_WIN
    wrapBatchFile(cmd, args);
#endif

    QStringList envList = QProcessEnvironment::systemEnvironment().toStringList();

    // Use reasonable default terminal size that matches typical console windows
    // These will be updated via resizePty() when the console widget connects
    const int defaultCols = 120;  // More standard for modern terminals
    const int defaultRows = 30;   // Better than 24 for visibility

    qDebug() << "ServerLaunchTask: Starting PTY process:" << cmd;
    qDebug() << "ServerLaunchTask: Initial terminal size:" << defaultCols << "x" << defaultRows;
    bool success = m_ptyProcess->startProcess(cmd, args, workDir, envList, defaultCols, defaultRows);
    if (!success) {
        QString err = m_ptyProcess->lastError();
        qCritical() << "ServerLaunchTask: Failed to start PTY process:" << err;
        emitFailed("Failed to start server process: " + err);
        return;
    }

    qDebug() << "ServerLaunchTask: PTY started, PID:" << m_ptyProcess->pid();

    // Start exit check timer
    m_exitCheckTimer = std::make_unique<QTimer>(this);
    connect(m_exitCheckTimer.get(), &QTimer::timeout, this, &ServerLaunchTask::checkProcessExit);
    m_exitCheckTimer->start(500);

    setStatus("Server running");
    m_finished = false;
    qDebug() << "ServerLaunchTask::executeTask() - Server started successfully";
}

bool ServerLaunchTask::stop(bool force)
{
    qDebug() << "ServerLaunchTask::stop() - Stopping server";

    if (m_finished || !m_ptyProcess) {
        if (m_instance && m_instance->isRunning())
            m_instance->setRunning(false);
        return true;
    }

    if (m_stopping)
        return false;
    m_stopping = true;

    const qint64 pid = m_ptyProcess->pid();
    bool exited = false;

    // A normal stop is an actual server-console command. It intentionally
    // does not kill the process when the command is empty or times out.
    if (!force) {
        const auto command = m_instance ? m_instance->stopCommand().trimmed() : QString();
        if (!command.isEmpty()) {
            auto data = command.toUtf8();
            if (!data.endsWith('\n'))
                data.append('\n');
            writeToStdin(data);
            exited = waitForProcessExit(pid, 10000);
        }
        if (!exited) {
            m_stopping = false;
            return false;
        }
        m_stopping = false;
        return true;
    }

    exited = terminateProcessTree();
    if (!exited) {
        m_stopping = false;
        qWarning() << "ServerLaunchTask::stop() - Process tree could not be terminated";
        return false;
    }

    // Stop timer first
    if (m_exitCheckTimer) {
        m_exitCheckTimer->stop();
        m_exitCheckTimer->blockSignals(true);
    }

    // Release the PTY after the OS process tree has been terminated. The PTY
    // implementations close handles, but do not provide process-tree kill
    // semantics themselves.
    if (m_ptyProcess) {
        m_ptyProcess->kill();
        m_ptyProcess.reset();
    }

    m_finished = true;

    // Clear running state
    if (m_instance && m_instance->isRunning()) {
        m_instance->setRunning(false);
    }

    // A forced stop is still a terminal Task result. This wakes controller
    // and stream users that otherwise would retain a permanently-running task.
    if (isRunning())
        emitAborted();

    m_stopping = false;
    qDebug() << "ServerLaunchTask::stop() - Server stopped, process tree exited:" << exited;
    return exited;
}

void ServerLaunchTask::cleanup()
{
    qDebug() << "ServerLaunchTask::cleanup() - Cleaning up, this:" << this << "m_finished:" << m_finished;

    // Stop and block timer
    if (m_exitCheckTimer) {
        m_exitCheckTimer->stop();
        m_exitCheckTimer->blockSignals(true);
    }

    // Only kill PTY process if it hasn't finished normally
    // If m_finished is true, the process already exited and was cleaned up in onPtyExit
    if (m_ptyProcess) {
        if (!m_finished) {
            qDebug() << "ServerLaunchTask::cleanup() - Killing PTY process (abnormal termination)";
            terminateProcessTree();
            m_ptyProcess->kill();
        } else {
            qDebug() << "ServerLaunchTask::cleanup() - PTY process already exited normally, just releasing";
        }
        m_ptyProcess.reset();
    }

    // Disconnect PTY notifier signals
    // Note: We don't disconnect(this) as that breaks Qt's signal system

    // Delete timer
    if (m_exitCheckTimer) {
        delete m_exitCheckTimer.release();
    }

    // Only clear running state if this task is still the active task for the instance
    // This prevents an old task from clearing the running state of a new task
    if (m_instance) {
        auto currentTask = dynamic_cast<ServerLaunchTask*>(m_instance->launchTask());
        qDebug() << "ServerLaunchTask::cleanup() - currentTask:" << currentTask << "this:" << this;
        if (currentTask == this && m_instance->isRunning()) {
            qDebug() << "ServerLaunchTask::cleanup() - Clearing running state";
            m_instance->setRunning(false);
        } else {
            qDebug() << "ServerLaunchTask::cleanup() - Not clearing running state (task mismatch or not running)";
        }
    }

    qDebug() << "ServerLaunchTask::cleanup() - Cleanup complete";
}

void ServerLaunchTask::parseCommandLine(const QString &fullCmd, const QString &workDir, QString &cmd, QStringList &args)
{
    Q_UNUSED(workDir)

    bool directFileExists = QFileInfo::exists(fullCmd) ||
                           (QFileInfo(fullCmd).isRelative() && QFileInfo::exists(FS::PathCombine(workDir, fullCmd)));

    if (directFileExists) {
        cmd = fullCmd;
        return;
    }

    // Parse command line
    QStringList parts = QProcess::splitCommand(fullCmd);
    if (parts.isEmpty()) {
        cmd = fullCmd;
        return;
    }

    auto normalize = [](QString s) {
        s = s.trimmed();
        if (s.startsWith('"') && s.endsWith('"') && s.size() >= 2)
            s = s.mid(1, s.size() - 2);
        return QDir::toNativeSeparators(s);
    };

    auto existsPath = [&](QString p) {
        p = normalize(p);
        return QFileInfo::exists(p) || QFileInfo::exists(FS::PathCombine(workDir, p));
    };

    // Find executable
    int exeIdx = -1;
    for (int i = 0; i < parts.size(); ++i) {
        QString tl = parts[i].toLower();
        if (tl.endsWith(".exe") || tl.endsWith(".bat") || tl.endsWith(".cmd")) {
            exeIdx = i;
            break;
        }
    }
    if (exeIdx < 0) exeIdx = 0;

    // Build command
    QString candidate = parts[0];
    for (int i = 1; i <= exeIdx && i < parts.size(); ++i) {
        candidate += " " + parts[i];
    }

    if (existsPath(candidate)) {
        cmd = normalize(candidate);
        for (int i = exeIdx + 1; i < parts.size(); ++i) {
            args.append(parts[i]);
        }
    } else {
        // Fallback: incremental join
        candidate = parts[0];
        int consumed = 1;
        while (consumed < parts.size() && !existsPath(candidate)) {
            candidate += " " + parts[consumed];
            consumed++;
        }
        if (existsPath(candidate)) {
            cmd = normalize(candidate);
            for (int i = consumed; i < parts.size(); ++i) {
                args.append(parts[i]);
            }
        } else {
            cmd = normalize(parts.takeFirst());
            args = parts;
        }
    }
}

void ServerLaunchTask::wrapBatchFile(QString &cmd, QStringList &args)
{
    QFileInfo finalInfo(cmd);
    const QString suffix = finalInfo.suffix().toLower();
    if (suffix == "bat" || suffix == "cmd") {
        QString quotedCmd = cmd;
        if (quotedCmd.contains(' ')) {
            quotedCmd = "\"" + quotedCmd + "\"";
        }
        QStringList newArgs;
        newArgs << "/C" << quotedCmd;
        newArgs.append(args);
        cmd = "cmd.exe";
        args = newArgs;
    }
}

void ServerLaunchTask::writeToStdin(const QByteArray &data)
{
    if (m_ptyProcess) {
        m_ptyProcess->write(data);
    }
}

void ServerLaunchTask::resizePty(int cols, int rows)
{
    if (m_ptyProcess) {
        qDebug() << "ServerLaunchTask::resizePty - resizing to:" << cols << "x" << rows;
        m_ptyProcess->resize(cols, rows);
    } else {
        qWarning() << "ServerLaunchTask::resizePty - cannot resize, no PTY process";
    }
}

bool ServerLaunchTask::canStop() const
{
    return m_ptyProcess != nullptr && !m_finished && !m_stopping;
}

bool ServerLaunchTask::stopServer(bool force)
{
    if (!m_ptyProcess || m_finished) {
        if (m_instance && !m_instance->isRunning())
            return true;
        return false;
    }
    return stop(force);
}

bool ServerLaunchTask::waitForProcessExit(qint64 pid, int timeoutMs)
{
    if (pid <= 0)
        return true;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        if (m_finished || !m_ptyProcess || !isProcessRunning(pid))
            return true;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
        QThread::msleep(25);
    }
    return !isProcessRunning(pid);
}

void ServerLaunchTask::collectProcessTree(const IPtyProcess::pidTree_t &tree, QList<qint64> &pids)
{
    for (const auto &child : tree.children)
        collectProcessTree(child, pids);
    if (tree.pidInfo.pid > 0 && !pids.contains(tree.pidInfo.pid))
        pids.append(tree.pidInfo.pid);
}

bool ServerLaunchTask::terminateProcessTree()
{
    if (!m_ptyProcess)
        return true;

    const qint64 rootPid = m_ptyProcess->pid();
    if (rootPid <= 0)
        return true;

    QList<qint64> pids;
    collectProcessTree(m_ptyProcess->processInfoTree(), pids);
    if (!pids.contains(rootPid))
        pids.append(rootPid);

#ifdef Q_OS_WIN
    // Children are terminated before their parent so cmd.exe/batch wrappers
    // cannot leave the Java process detached from the PTY.
    for (const auto pid : std::as_const(pids)) {
        HANDLE process = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
        if (!process)
            continue;
        TerminateProcess(process, 1);
        WaitForSingleObject(process, 2000);
        CloseHandle(process);
    }
#else
    for (const auto pid : std::as_const(pids))
        ::kill(static_cast<pid_t>(pid), SIGTERM);
    const auto deadline = QDateTime::currentMSecsSinceEpoch() + 2000;
    while (QDateTime::currentMSecsSinceEpoch() < deadline) {
        bool anyRunning = false;
        for (const auto pid : std::as_const(pids)) {
            if (isProcessRunning(pid)) {
                anyRunning = true;
                break;
            }
        }
        if (!anyRunning)
            return true;
        QThread::msleep(25);
    }
    for (const auto pid : std::as_const(pids))
        if (isProcessRunning(pid))
            ::kill(static_cast<pid_t>(pid), SIGKILL);
#endif

    bool running = false;
    for (const auto pid : std::as_const(pids)) {
        if (isProcessRunning(pid)) {
            running = true;
            break;
        }
    }
    return !running;
}

void ServerLaunchTask::onPtyRead()
{
    if (m_finished || !m_ptyProcess) {
        return;
    }

    QByteArray data = m_ptyProcess->readAll();
    if (!data.isEmpty()) {
        emit readyRead(data);
    } else if (m_ptyProcess->pid() > 0 && !isProcessRunning(m_ptyProcess->pid())) {
        onPtyExit();
    }
}

void ServerLaunchTask::onPtyExit()
{
    if (m_finished) {
        return;
    }
    m_finished = true;
    qDebug() << "ServerLaunchTask: Process exited normally";

    // Stop the exit check timer
    if (m_exitCheckTimer) {
        m_exitCheckTimer->stop();
        m_exitCheckTimer->blockSignals(true);
    }

    // Clear PTY process since it has exited normally - no need to kill
    m_ptyProcess.reset();

    qDebug() << "ServerLaunchTask: Process exited, emitting success";

    // Call emitSucceeded which will set running state and emit signals
    // This matches the behavior of normal LaunchTask
    emitSucceeded();
}

void ServerLaunchTask::checkProcessExit()
{
    if (m_finished || !m_ptyProcess) {
        if (m_exitCheckTimer) {
            m_exitCheckTimer->stop();
        }
        return;
    }

    qint64 pid = m_ptyProcess->pid();
    if (pid > 0 && !isProcessRunning(pid)) {
        m_exitCheckTimer->stop();
        onPtyExit();
    }
}

bool ServerLaunchTask::isProcessRunning(qint64 pid)
{
#ifdef Q_OS_WIN
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
    if (hProcess) {
        DWORD exitCode = 0;
        BOOL result = GetExitCodeProcess(hProcess, &exitCode);
        CloseHandle(hProcess);
        return result && exitCode == STILL_ACTIVE;
    }
    return false;
#else
    const int probe = kill((pid_t)pid, 0);
    if (probe < 0 && errno == ESRCH)
        return false;
    if (probe < 0 && errno != EPERM)
        return false;

    // Inspect /proc when available so a zombie is not reported as running.
    QString statPath = QString("/proc/%1/stat").arg(pid);
    QFile statFile(statPath);
    if (statFile.exists() && statFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray content = statFile.readAll();
        statFile.close();
        int commEnd = content.lastIndexOf(')');
        if (commEnd > 0)
            return content.size() <= commEnd + 2 || content[commEnd + 2] != 'Z';
    }
    return probe == 0 || errno == EPERM;
#endif
}

void ServerLaunchTask::emitSucceeded()
{
    qDebug() << "ServerLaunchTask::emitSucceeded() - Called";

    // Match LaunchTask behavior: set running state before emitting signals
    if (m_instance) {
        m_instance->setRunning(false);
        qDebug() << "ServerLaunchTask: Set instance running state to false";
    }

    qDebug() << "ServerLaunchTask::emitSucceeded() - Calling Task::emitSucceeded()";
    Task::emitSucceeded();
    qDebug() << "ServerLaunchTask::emitSucceeded() - Task::emitSucceeded() returned";
}

void ServerLaunchTask::emitFailed(QString reason)
{
    // Match LaunchTask behavior: set running state before emitting signals
    if (m_instance) {
        m_instance->setRunning(false);
        qDebug() << "ServerLaunchTask: Set instance running state to false (failed)";
    }
    Task::emitFailed(reason);
}
