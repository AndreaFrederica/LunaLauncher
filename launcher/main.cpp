// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *      Copyright 2013-2021 MultiMC Contributors
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#include "Application.h"

#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QTextStream>
#include <QDebug>
#include <csignal>
#include <cstdlib>
#include <cstdio>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dbghelp.h>
#ifdef _MSC_VER
#pragma comment(lib, "dbghelp.lib")
#endif
#endif

static QString crashLogPath;

void writeCrashLog(const QString& message)
{
    QFile file(crashLogPath);
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        out << message << "\n";
        file.close();
    }
}

#ifdef Q_OS_WIN

// Helper function to capture stack trace with detailed symbols
static void captureStackTrace(QStringList& frames, const CONTEXT* sourceContext)
{
    if (!sourceContext) {
        return;
    }

    HANDLE hProcess = GetCurrentProcess();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
    if (!SymInitialize(hProcess, nullptr, TRUE)) {
        return;
    }

    CONTEXT context = *sourceContext;

    STACKFRAME64 stackFrame;
    memset(&stackFrame, 0, sizeof(stackFrame));

#ifdef _M_AMD64
    DWORD machineType = IMAGE_FILE_MACHINE_AMD64;
    stackFrame.AddrPC.Offset = context.Rip;
    stackFrame.AddrFrame.Offset = context.Rbp;
    stackFrame.AddrStack.Offset = context.Rsp;
#elif _M_IX86
    DWORD machineType = IMAGE_FILE_MACHINE_I386;
    stackFrame.AddrPC.Offset = context.Eip;
    stackFrame.AddrFrame.Offset = context.Ebp;
    stackFrame.AddrStack.Offset = context.Esp;
#elif _M_ARM64
    DWORD machineType = IMAGE_FILE_MACHINE_ARM64;
    stackFrame.AddrPC.Offset = context.Pc;
    stackFrame.AddrFrame.Offset = context.Fp;
    stackFrame.AddrStack.Offset = context.Sp;
#endif

    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrStack.Mode = AddrModeFlat;

    auto* symbol = reinterpret_cast<SYMBOL_INFO*>(::operator new(sizeof(SYMBOL_INFO) + 512 * sizeof(char)));
    symbol->MaxNameLen = 511;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

    IMAGEHLP_LINE64 line;
    line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

    for (int i = 0; i < 64; i++) {
        if (!StackWalk64(machineType, hProcess, GetCurrentThread(), &stackFrame, &context, nullptr,
                         SymFunctionTableAccess64, SymGetModuleBase64, nullptr)) {
            break;
        }

        QString frame = QString("  #%1: ").arg(i);

        // Get function name
        if (SymFromAddr(hProcess, stackFrame.AddrPC.Offset, nullptr, symbol)) {
            frame += QString::fromLocal8Bit(symbol->Name);
        } else {
            frame += QString("0x%1").arg(stackFrame.AddrPC.Offset, 0, 16);
        }

        // Get file and line
        DWORD displacement = 0;
        if (SymGetLineFromAddr64(hProcess, stackFrame.AddrPC.Offset, &displacement, &line)) {
            QString filePath = QString::fromLocal8Bit(line.FileName);
            frame += QString(" (%1:%2)").arg(filePath.mid(filePath.lastIndexOf('\\') + 1)).arg(line.LineNumber);
        }

        frames.append(frame);

        if (stackFrame.AddrReturn.Offset == 0) break;
    }

    ::operator delete(symbol);
    SymCleanup(hProcess);
}

LONG WINAPI windowsExceptionHandler(EXCEPTION_POINTERS* exceptionInfo)
{
    static LONG inHandler = 0;
    if (InterlockedCompareExchange(&inHandler, 1, 0) != 0) {
        return EXCEPTION_EXECUTE_HANDLER;
    }

    // Set up crash log path first
    crashLogPath = QDir::current().filePath("crash.log");

    DWORD code = 0;
    const CONTEXT* contextRecord = nullptr;
    if (exceptionInfo && exceptionInfo->ExceptionRecord) {
        code = exceptionInfo->ExceptionRecord->ExceptionCode;
    }
    if (exceptionInfo && exceptionInfo->ContextRecord) {
        contextRecord = exceptionInfo->ContextRecord;
    }

    QString message = QString("[CRASH] %1 - Windows Exception Code: 0x%2")
                          .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs))
                          .arg(code, 8, 16, QChar('0'));

    // Output to console/debugger
    OutputDebugStringA(message.toLocal8Bit().constData());
    OutputDebugStringA("\n");
    fprintf(stderr, "%s\n", message.toLocal8Bit().constData());

    writeCrashLog(message);

    // Capture detailed stack trace
    QStringList frames;
    captureStackTrace(frames, contextRecord);

    writeCrashLog("[CRASH] Stack Trace:");
    if (frames.isEmpty()) {
        writeCrashLog("  <unavailable>");
    } else {
        for (const QString& frame : frames) {
            fprintf(stderr, "%s\n", frame.toLocal8Bit().constData());
            writeCrashLog(frame);
        }
    }

    // Try to write minidump
    QString dumpPath = crashLogPath;
    dumpPath.replace(".log", ".dmp");
    HANDLE hDumpFile = CreateFile(reinterpret_cast<LPCWSTR>(dumpPath.utf16()), GENERIC_WRITE, FILE_SHARE_WRITE, nullptr,
                                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hDumpFile != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mdei;
        MINIDUMP_EXCEPTION_INFORMATION* mdeiPtr = nullptr;
        if (exceptionInfo) {
            mdei.ThreadId = GetCurrentThreadId();
            mdei.ExceptionPointers = exceptionInfo;
            mdei.ClientPointers = FALSE;
            mdeiPtr = &mdei;
        }

        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hDumpFile, MiniDumpNormal, mdeiPtr, nullptr,
                          nullptr);
        CloseHandle(hDumpFile);
        writeCrashLog(QString("[CRASH] Minidump written to: %1").arg(dumpPath));
    }

    // Show crash dialog before exiting
    MessageBoxW(nullptr, reinterpret_cast<LPCWSTR>(message.utf16()), L"Prism Launcher Crashed", MB_OK | MB_ICONERROR);

    return EXCEPTION_EXECUTE_HANDLER;
}
#else
#include <signal.h>
#include <execinfo.h>
#include <cstring>

static void unixSignalHandler(int sig)
{
    // Set up crash log path first
    crashLogPath = QDir::current().filePath("crash.log");

    QString message = QString("[CRASH] %1 - Unix Signal: %2 (%3)")
                          .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs))
                          .arg(strsignal(sig))
                          .arg(sig);

    fprintf(stderr, "%s\n", message.toLocal8Bit().constData());
    writeCrashLog(message);

    // Try to get backtrace
    void* buffer[100];
    int nframes = backtrace(buffer, 100);
    char** symbols = backtrace_symbols(buffer, nframes);

    if (symbols) {
        writeCrashLog("[CRASH] Backtrace:");
        for (int i = 0; i < nframes; i++) {
            QString frame = QString("  #%1: %2").arg(i).arg(symbols[i]);
            fprintf(stderr, "%s\n", frame.toLocal8Bit().constData());
            writeCrashLog(frame);
        }
        free(symbols);
    }

    // Re-raise signal for default handler
    signal(sig, SIG_DFL);
    raise(sig);
}

static void setupUnixSignalHandlers()
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = unixSignalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGABRT, &sa, nullptr);
    sigaction(SIGBUS, &sa, nullptr);
    sigaction(SIGILL, &sa, nullptr);
    sigaction(SIGFPE, &sa, nullptr);
}
#endif

void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    static thread_local bool inMessageHandler = false;
    if (inMessageHandler) {
        return;
    }
    struct ScopedFlagReset {
        bool& ref;
        ~ScopedFlagReset() { ref = false; }
    };
    inMessageHandler = true;
    ScopedFlagReset reset{ inMessageHandler };

    auto file = context.file ? context.file : "unknown";
    auto function = context.function ? context.function : "unknown";

    QString timestamp = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    QString level;
    switch (type) {
        case QtDebugMsg: level = "DEBUG"; break;
        case QtInfoMsg: level = "INFO"; break;
        case QtWarningMsg: level = "WARNING"; break;
        case QtCriticalMsg: level = "CRITICAL"; break;
        case QtFatalMsg: level = "FATAL"; break;
    }

    QString message = QString("[%1] [%2] %3 (%4:%5, %6)")
                          .arg(timestamp)
                          .arg(level)
                          .arg(msg)
                          .arg(file)
                          .arg(context.line)
                          .arg(function);

    auto encoded = message.toLocal8Bit();
    fprintf(stderr, "%s\n", encoded.constData());
    fflush(stderr);

    // Also output via OutputDebugString on Windows for IDE/debugger
#ifdef Q_OS_WIN
    OutputDebugStringA(encoded.constData());
    OutputDebugStringA("\n");
#endif

    // For fatal messages, also trigger crash handler
    if (type == QtFatalMsg) {
#ifdef Q_OS_WIN
        // Raise exception to trigger our handler
        RaiseException(0xE0000001, EXCEPTION_NONCONTINUABLE, 0, nullptr);
        TerminateProcess(GetCurrentProcess(), 1);
#else
        abort();
#endif
    }
}

int main(int argc, char* argv[])
{
    // try to set the utf-8 locale for the libarchive
    for (auto name : { ".UTF-8", "en_US.UTF-8", "C.UTF-8" }) {
        if (std::setlocale(LC_CTYPE, name)) {
            break;
        }
    }

#ifdef Q_OS_WIN
    // Set up Windows exception handler
    SetUnhandledExceptionFilter(windowsExceptionHandler);
#else
    // Set up Unix signal handlers
    setupUnixSignalHandlers();
#endif

    // Install message handler
    qInstallMessageHandler(messageHandler);
    // initialize Qt
    Application app(argc, argv);

    switch (app.status()) {
        case Application::StartingUp:
        case Application::Initialized: {
            Q_INIT_RESOURCE(multimc);
            Q_INIT_RESOURCE(backgrounds);
            Q_INIT_RESOURCE(documents);
            Q_INIT_RESOURCE(lunalauncher);

            Q_INIT_RESOURCE(pe_dark);
            Q_INIT_RESOURCE(pe_light);
            Q_INIT_RESOURCE(pe_blue);
            Q_INIT_RESOURCE(pe_colored);
            Q_INIT_RESOURCE(breeze_dark);
            Q_INIT_RESOURCE(breeze_light);
            Q_INIT_RESOURCE(OSX);
            Q_INIT_RESOURCE(iOS);
            Q_INIT_RESOURCE(flat);
            Q_INIT_RESOURCE(flat_white);

            Q_INIT_RESOURCE(shaders);
            return app.exec();
        }
        case Application::Failed:
            return 1;
        case Application::Succeeded:
            return 0;
        default:
            return -1;
    }
}
