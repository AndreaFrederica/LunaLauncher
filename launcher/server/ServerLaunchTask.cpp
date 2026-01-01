/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
*/
#include "ServerLaunchTask.h"
#include "ServerInstance.h"
#include "FileSystem.h"
#include <QDir>
#include <QProcessEnvironment>
#include <QDebug>
#include <QProcess>
#include <QFileInfo>

ServerLaunchTask::ServerLaunchTask(ServerInstance *instance)
    : LaunchTask(instance->shared_from_this()), m_instance(instance)
{
}

ServerLaunchTask::~ServerLaunchTask()
{
    if (m_ptyProcess) {
        m_ptyProcess->kill();
    }
}

void ServerLaunchTask::executeTask()
{
    m_instance->setRunning(true);
    setStatus("Starting server...");

    // Create PTY process
    m_ptyProcess.reset(PtyQt::createPtyProcess());
    if (!m_ptyProcess) {
        emitFailed("Failed to create PTY process");
        return;
    }

    // Connect signals
    connect(m_ptyProcess->notifier(), &QIODevice::readyRead, this, &ServerLaunchTask::onPtyRead);
    connect(m_ptyProcess->notifier(), &QIODevice::readChannelFinished, this, &ServerLaunchTask::onPtyExit);

    // Prepare arguments
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

    // Do not add quotes here; startProcess expects a plain path

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

    // Attempt to split the command if it contains spaces and isn't an existing file
    QString cmd;
    QStringList args;

    bool directFileExists = false;
    if (QFileInfo::exists(fullCmd)) {
        directFileExists = true;
    } else {
        if (QFileInfo(fullCmd).isRelative()) {
            if (QFileInfo::exists(FS::PathCombine(workDir, fullCmd))) {
                directFileExists = true;
            }
        }
    }

    if (directFileExists) {
        cmd = fullCmd;
    } else {
        // Parse a full command line, handling unquoted paths with spaces robustly.
        QStringList parts = QProcess::splitCommand(fullCmd);
        if (parts.isEmpty()) {
            cmd = fullCmd;
        } else {
            auto normalize = [](QString s) {
                s = s.trimmed();
                if (s.startsWith('"') && s.endsWith('"') && s.size() >= 2)
                    s = s.mid(1, s.size() - 2);
                return QDir::toNativeSeparators(s);
            };
            auto existsPath = [&](QString p) {
                p = normalize(p);
                if (QFileInfo::exists(p))
                    return true;
                QString rel = FS::PathCombine(workDir, p);
                return QFileInfo::exists(rel);
            };
            // Heuristic: combine tokens up to the one that looks like an executable (*.exe, *.bat, *.cmd)
            int exeIdx = -1;
            for (int i = 0; i < parts.size(); ++i) {
                QString t = parts[i];
                QString tl = t.toLower();
                if (tl.endsWith(".exe") || tl.endsWith(".bat") || tl.endsWith(".cmd")) {
                    exeIdx = i;
                    break;
                }
            }
            if (exeIdx < 0)
                exeIdx = 0;
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
                // Fallback: incremental join until it exists
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
                    // Ultimate fallback
                    cmd = parts.takeFirst();
                    args = parts;
                }
            }
        }
    }

    // Append explicitly configured arguments
    args.append(extraArgs);

    // JVM properties go BEFORE '-jar'
    if (cmd.contains("java", Qt::CaseInsensitive) && args.contains("-jar", Qt::CaseInsensitive)) {
        int jarPos = -1;
        for (int i = 0; i < args.size(); ++i) {
            if (args[i].compare("-jar", Qt::CaseInsensitive) == 0) {
                jarPos = i;
                break;
            }
        }
        if (jarPos < 0) jarPos = 0;
        args.insert(jarPos, "-Dterminal.ansi=true");
        args.insert(jarPos, "-Dterminal.jline=false");
    }

    // Resolve relative executable path against working directory
    QFileInfo cmdInfo(cmd);
    if (cmdInfo.isRelative()) {
        QString absPath = FS::PathCombine(workDir, cmd);
        if (QFileInfo::exists(absPath)) {
            cmd = absPath;
        }
    }

#ifdef Q_OS_WIN
    // Windows: run batch files via cmd.exe
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
#endif

    QStringList envList = QProcessEnvironment::systemEnvironment().toStringList();

    qDebug() << "Starting PTY process:" << cmd << "args:" << args << "workDir:" << workDir;
    bool success = m_ptyProcess->startProcess(cmd, args, workDir, envList, 80, 24);
    if (!success) {
        QString err = m_ptyProcess->lastError();
        qCritical() << "Failed to start PTY process:" << err;
        emitFailed("Failed to start server process: " + err);
        return;
    }

    setStatus("Server running");
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
        m_ptyProcess->resize(cols, rows);
    }
}

void ServerLaunchTask::onPtyRead()
{
    QByteArray data = m_ptyProcess->readAll();
    if (!data.isEmpty()) {
        emit readyRead(data);
    }
}

void ServerLaunchTask::onPtyExit()
{
    emitSucceeded();
}
