// SPDX-License-Identifier: GPL-3.0-only
/*
 *  LunaLauncher - Minecraft Launcher
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
 */

#pragma once

#include <cstdint>

#include <QByteArray>
#include <QHash>
#include <QLibrary>
#include <QMutex>
#include <QObject>
#include <QSet>
#include <QString>
#include <QUrl>
#include <QVector>

/// Event types pushed by pcl_download_set_event_callback.
enum class PclDownloadEvent : int { Progress = 0, Finished = 1, Failed = 2, Aborted = 3 };

/**
 * Qt-side bridge that re-emits native download events on the main thread.
 * The native callback runs on a .NET threadpool thread; it is marshalled
 * here via QMetaObject::invokeMethod (queued) before being emitted.
 */
class PclDownloadBridge : public QObject {
    Q_OBJECT
   public:
    using QObject::QObject;

    void enqueueTaskEvent(int taskId, int eventType, qint64 downloaded, qint64 total, qint64 speed, int threads);

   signals:
    void taskEvent(int taskId, int eventType, qint64 downloaded, qint64 total, qint64 speed, int threads);

   private:
    struct TaskEvent {
        int taskId;
        int eventType;
        qint64 downloaded;
        qint64 total;
        qint64 speed;
        int threads;
    };

    void flushTaskEvents();

    QMutex m_eventMutex;
    QHash<int, TaskEvent> m_pendingProgressEvents;
    QVector<TaskEvent> m_pendingTerminalEvents;
    QSet<int> m_pendingTerminalTaskIds;
    bool m_eventFlushScheduled = false;
};

/**
 * Loader for the PCL.Download NativeAOT shared library
 * (PCL.Download.dll / libPCL.Download.so / libPCL.Download.dylib).
 *
 * Resolves the C API exported via [UnmanagedCallersOnly] in Exports.cs
 * and exposes a thin QString/QUrl friendly wrapper on top of the raw
 * function pointers. All functions are safe to call when the library is
 * not loaded — they return error values in that case.
 */
class PclDownloadLibrary {
   public:
    static PclDownloadLibrary& instance();

    /// Load the shared library and resolve all entry points. Returns true on success.
    bool load();
    bool isLoaded() const { return m_loaded; }
    QString errorString() const { return m_loadError; }

    /// Bridge emitting download events (pushed by the native library) on the main thread.
    /// Null until load() succeeds.
    PclDownloadBridge* bridge() const { return m_bridge; }

    // ---- Qt-friendly wrappers ----

    /// Start a single-file download. Returns task id (> 0) or -1 on error.
    int startFileDownload(const QUrl& url, const QString& localPath, const QString& hash = QString(), qint64 expectedSize = -1);
    /// Start a multi-source single-file download (URLs tried in order). Returns task id (> 0) or -1 on error.
    int startFileDownload(const QStringList& urls, const QString& localPath, const QString& hash = QString(), qint64 expectedSize = -1);
    void abortDownload(int taskId);
    void releaseTask(int taskId);

    /// 0=running, 1=finished, 2=failed, 3=aborted, -1=unknown/unavailable.
    int taskState(int taskId);
    /// Aggregate progress, 0.0 ~ 1.0.
    double taskProgress(int taskId);
    /// Per-file progress detail as a JSON document, empty on error.
    QByteArray taskProgressDetailJson(int taskId);
    /// Error message of a task, empty if none.
    QString taskErrorString(int taskId);

    /// Global download speed in bytes/sec across all active downloads.
    qint64 globalSpeed();
    /// Total number of active download threads/chunks.
    int globalActiveThreads();

    void setThreadLimit(int limit);
    /// Speed limit in bytes/sec. <= 0 means unlimited.
    void setSpeedLimit(qint64 bytesPerSec);
    void setProxy(const QString& type, const QString& host, int port, const QString& user, const QString& password);

    // ---- Raw C function pointers (resolved from the shared library) ----
    int (*pcl_download_init)() = nullptr;
    void (*pcl_download_shutdown)() = nullptr;
    int (*pcl_download_file)(const char*, const char*, const char*, int64_t) = nullptr;
    int (*pcl_download_file_wait)(int) = nullptr;
    void (*pcl_download_file_abort)(int) = nullptr;
    void (*pcl_download_release)(int) = nullptr;
    int (*pcl_download_batch_start)(const char*) = nullptr;
    int (*pcl_download_batch_get_state)(int) = nullptr;
    double (*pcl_download_batch_get_progress)(int) = nullptr;
    const char* (*pcl_download_batch_get_progress_detail)(int) = nullptr;
    int (*pcl_download_batch_wait)(int) = nullptr;
    void (*pcl_download_batch_abort)(int) = nullptr;
    int (*pcl_download_get_state)(int) = nullptr;
    double (*pcl_download_get_progress)(int) = nullptr;
    const char* (*pcl_download_get_progress_detail)(int) = nullptr;
    long long (*pcl_download_get_speed)() = nullptr;
    int (*pcl_download_get_active_threads)() = nullptr;
    const char* (*pcl_download_get_error)(int) = nullptr;
    void (*pcl_free_string)(const char*) = nullptr;
    void (*pcl_download_set_thread_limit)(int) = nullptr;
    void (*pcl_download_set_speed_limit)(long long) = nullptr;
    void (*pcl_download_set_proxy)(const char*, const char*, int, const char*, const char*) = nullptr;
    void (*pcl_download_set_event_callback)(void*) = nullptr;

   private:
    PclDownloadLibrary() = default;
    ~PclDownloadLibrary();

    template <typename Fn>
    bool resolve(Fn& out, const char* name);

    static void nativeEventCallback(int taskId, int eventType, long long downloaded, long long total, long long speed, int threads);

    QLibrary m_lib;
    PclDownloadBridge* m_bridge = nullptr;
    bool m_loaded = false;
    QString m_loadError;
};
