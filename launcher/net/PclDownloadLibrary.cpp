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

#include "PclDownloadLibrary.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

#include <cstring>

#include "net/Logging.h"

#if defined(LAUNCHER_APPLICATION)
#include "Application.h"
#include "settings/SettingsObject.h"
#endif

PclDownloadLibrary& PclDownloadLibrary::instance()
{
    static PclDownloadLibrary inst;
    return inst;
}

void PclDownloadBridge::enqueueTaskEvent(int taskId,
                                         int eventType,
                                         qint64 downloaded,
                                         qint64 total,
                                         qint64 speed,
                                         int threads)
{
    bool scheduleFlush = false;
    {
        QMutexLocker lock(&m_eventMutex);
        const TaskEvent event{ taskId, eventType, downloaded, total, speed, threads };
        if (eventType == static_cast<int>(PclDownloadEvent::Progress)) {
            if (!m_pendingTerminalTaskIds.contains(taskId)) {
                // Only the newest progress sample matters before the event loop
                // gets a chance to process it.
                m_pendingProgressEvents.insert(taskId, event);
            }
        } else if (!m_pendingTerminalTaskIds.contains(taskId)) {
            m_pendingProgressEvents.remove(taskId);
            m_pendingTerminalEvents.append(event);
            m_pendingTerminalTaskIds.insert(taskId);
        }

        if (!m_eventFlushScheduled && (!m_pendingProgressEvents.isEmpty() || !m_pendingTerminalEvents.isEmpty())) {
            m_eventFlushScheduled = true;
            scheduleFlush = true;
        }
    }

    if (scheduleFlush) {
        QMetaObject::invokeMethod(this, [this] { flushTaskEvents(); }, Qt::QueuedConnection);
    }
}

void PclDownloadBridge::flushTaskEvents()
{
    QHash<int, TaskEvent> progressEvents;
    QVector<TaskEvent> terminalEvents;
    {
        QMutexLocker lock(&m_eventMutex);
        progressEvents.swap(m_pendingProgressEvents);
        terminalEvents.swap(m_pendingTerminalEvents);
        m_pendingTerminalTaskIds.clear();
        m_eventFlushScheduled = false;
    }

    for (const auto& event : progressEvents) {
        emit taskEvent(event.taskId, event.eventType, event.downloaded, event.total, event.speed, event.threads);
    }
    for (const auto& event : terminalEvents) {
        emit taskEvent(event.taskId, event.eventType, event.downloaded, event.total, event.speed, event.threads);
    }
}

PclDownloadLibrary::~PclDownloadLibrary()
{
    if (m_loaded && pcl_download_shutdown) {
        pcl_download_shutdown();
    }
}

template <typename Fn>
bool PclDownloadLibrary::resolve(Fn& out, const char* name)
{
    out = reinterpret_cast<Fn>(m_lib.resolve(name));
    if (!out) {
        m_loadError = QStringLiteral("Failed to resolve entry point '%1': %2").arg(QString::fromLatin1(name), m_lib.errorString());
        return false;
    }
    return true;
}

bool PclDownloadLibrary::load()
{
    if (m_loaded)
        return true;

    if (auto* app = QCoreApplication::instance()) {
        // Prefer the directory of the running executable (portable layout).
        m_lib.setFileName(app->applicationDirPath() + QStringLiteral("/PCL.Download"));
        if (!m_lib.load()) {
            // Fall back to the system library search path.
            m_lib.setFileName(QStringLiteral("PCL.Download"));
            if (!m_lib.load()) {
                m_loadError = m_lib.errorString();
                qCWarning(taskDownloadLogC) << "PclDownloadLibrary: failed to load PCL.Download:" << m_loadError;
                return false;
            }
        }
    } else {
        m_lib.setFileName(QStringLiteral("PCL.Download"));
        if (!m_lib.load()) {
            m_loadError = m_lib.errorString();
            qCWarning(taskDownloadLogC) << "PclDownloadLibrary: failed to load PCL.Download:" << m_loadError;
            return false;
        }
    }

    // clang-format off
    if (!resolve(pcl_download_init,                      "pcl_download_init"))                      return false;
    if (!resolve(pcl_download_shutdown,                  "pcl_download_shutdown"))                  return false;
    if (!resolve(pcl_download_file,                      "pcl_download_file"))                      return false;
    if (!resolve(pcl_download_file_wait,                 "pcl_download_file_wait"))                 return false;
    if (!resolve(pcl_download_file_abort,                "pcl_download_file_abort"))                return false;
    if (!resolve(pcl_download_release,                   "pcl_download_release"))                   return false;
    if (!resolve(pcl_download_batch_start,               "pcl_download_batch_start"))               return false;
    if (!resolve(pcl_download_batch_get_state,           "pcl_download_batch_get_state"))           return false;
    if (!resolve(pcl_download_batch_get_progress,        "pcl_download_batch_get_progress"))        return false;
    if (!resolve(pcl_download_batch_get_progress_detail, "pcl_download_batch_get_progress_detail")) return false;
    if (!resolve(pcl_download_batch_wait,                "pcl_download_batch_wait"))                return false;
    if (!resolve(pcl_download_batch_abort,               "pcl_download_batch_abort"))               return false;
    if (!resolve(pcl_download_get_state,                 "pcl_download_get_state"))                 return false;
    if (!resolve(pcl_download_get_progress,              "pcl_download_get_progress"))              return false;
    if (!resolve(pcl_download_get_progress_detail,       "pcl_download_get_progress_detail"))       return false;
    if (!resolve(pcl_download_get_speed,                 "pcl_download_get_speed"))                 return false;
    if (!resolve(pcl_download_get_active_threads,        "pcl_download_get_active_threads"))        return false;
    if (!resolve(pcl_download_get_error,                 "pcl_download_get_error"))                 return false;
    if (!resolve(pcl_free_string,                        "pcl_free_string"))                        return false;
    if (!resolve(pcl_download_set_thread_limit,          "pcl_download_set_thread_limit"))          return false;
    if (!resolve(pcl_download_set_speed_limit,           "pcl_download_set_speed_limit"))           return false;
    if (!resolve(pcl_download_set_proxy,                 "pcl_download_set_proxy"))                 return false;
    if (!resolve(pcl_download_set_event_callback,        "pcl_download_set_event_callback"))        return false;
    // clang-format on

    if (pcl_download_init() != 0) {
        m_loadError = QStringLiteral("pcl_download_init() failed");
        return false;
    }

    m_bridge = new PclDownloadBridge;
    pcl_download_set_event_callback(reinterpret_cast<void*>(&PclDownloadLibrary::nativeEventCallback));

#if defined(LAUNCHER_APPLICATION)
    // Apply configured transfer limits.
    if (APPLICATION_DYN) {
        auto s = APPLICATION->settings();
        pcl_download_set_thread_limit(s->get("PclDownloadThreadLimit").toInt());
        pcl_download_set_speed_limit(static_cast<long long>(s->get("PclDownloadSpeedLimitKBps").toInt()) * 1024);
        const auto proxyType = s->get("ProxyType").toString().toUtf8();
        const auto proxyHost = s->get("ProxyAddr").toString().toUtf8();
        const auto proxyUser = s->get("ProxyUser").toString().toUtf8();
        const auto proxyPassword = s->get("ProxyPass").toString().toUtf8();
        pcl_download_set_proxy(proxyType.constData(), proxyHost.constData(), s->get("ProxyPort").toInt(), proxyUser.constData(),
                               proxyPassword.constData());
    }
#endif

    m_loaded = true;
    qCDebug(taskDownloadLogC) << "PclDownloadLibrary: PCL.Download loaded successfully";
    return true;
}

void PclDownloadLibrary::nativeEventCallback(int taskId,
                                             int eventType,
                                             long long downloaded,
                                             long long total,
                                             long long speed,
                                             int threads)
{
    // Called on a .NET threadpool thread — marshal to the Qt main thread.
    auto* bridge = instance().m_bridge;
    if (!bridge)
        return;
    bridge->enqueueTaskEvent(taskId, eventType, downloaded, total, speed, threads);
}

int PclDownloadLibrary::startFileDownload(const QUrl& url, const QString& localPath, const QString& hash, qint64 expectedSize)
{
    return startFileDownload(QStringList{ url.toString() }, localPath, hash, expectedSize);
}

int PclDownloadLibrary::startFileDownload(const QStringList& urls,
                                          const QString& localPath,
                                          const QString& hash,
                                          qint64 expectedSize)
{
    if (!isLoaded() || urls.isEmpty() || localPath.isEmpty())
        return -1;

    QJsonArray urlArray;
    for (const auto& url : urls) {
        if (!url.isEmpty())
            urlArray.append(url);
    }
    if (urlArray.isEmpty())
        return -1;

    const QByteArray urlsJson = QJsonDocument(urlArray).toJson(QJsonDocument::Compact);    const QByteArray pathUtf8 = localPath.toUtf8();
    const QByteArray hashUtf8 = hash.toUtf8();

    return pcl_download_file(urlsJson.constData(), pathUtf8.constData(), hashUtf8.isEmpty() ? nullptr : hashUtf8.constData(),
                             static_cast<int64_t>(expectedSize));
}

void PclDownloadLibrary::abortDownload(int taskId)
{
    if (isLoaded() && taskId >= 0)
        pcl_download_file_abort(taskId);
}

void PclDownloadLibrary::releaseTask(int taskId)
{
    if (isLoaded() && taskId >= 0)
        pcl_download_release(taskId);
}

int PclDownloadLibrary::taskState(int taskId)
{
    if (!isLoaded() || taskId < 0)
        return -1;
    return pcl_download_get_state(taskId);
}

double PclDownloadLibrary::taskProgress(int taskId)
{
    if (!isLoaded() || taskId < 0)
        return 0.0;
    return pcl_download_get_progress(taskId);
}

QByteArray PclDownloadLibrary::taskProgressDetailJson(int taskId)
{
    if (!isLoaded() || taskId < 0)
        return {};
    const char* ptr = pcl_download_get_progress_detail(taskId);
    if (!ptr)
        return {};
    // Deep copy before freeing the native string.
    QByteArray result(ptr, static_cast<qsizetype>(std::strlen(ptr)));
    pcl_free_string(ptr);
    return result;
}

QString PclDownloadLibrary::taskErrorString(int taskId)
{
    if (!isLoaded() || taskId < 0)
        return {};
    const char* ptr = pcl_download_get_error(taskId);
    if (!ptr)
        return {};
    QString result = QString::fromUtf8(ptr);
    pcl_free_string(ptr);
    return result;
}

qint64 PclDownloadLibrary::globalSpeed()
{
    return isLoaded() ? static_cast<qint64>(pcl_download_get_speed()) : 0;
}

int PclDownloadLibrary::globalActiveThreads()
{
    return isLoaded() ? pcl_download_get_active_threads() : 0;
}

void PclDownloadLibrary::setThreadLimit(int limit)
{
    if (isLoaded())
        pcl_download_set_thread_limit(limit);
}

void PclDownloadLibrary::setSpeedLimit(qint64 bytesPerSec)
{
    if (isLoaded())
        pcl_download_set_speed_limit(static_cast<long long>(bytesPerSec));
}

void PclDownloadLibrary::setProxy(const QString& type,
                                  const QString& host,
                                  int port,
                                  const QString& user,
                                  const QString& password)
{
    if (!isLoaded())
        return;

    const auto typeUtf8 = type.toUtf8();
    const auto hostUtf8 = host.toUtf8();
    const auto userUtf8 = user.toUtf8();
    const auto passwordUtf8 = password.toUtf8();
    pcl_download_set_proxy(typeUtf8.constData(), hostUtf8.constData(), port, userUtf8.constData(), passwordUtf8.constData());
}
