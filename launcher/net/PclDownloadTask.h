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

#include "net/Download.h"

namespace Net {

/**
 * Download request backed by the PCL.Download NativeAOT library.
 * Provides multi-chunk parallel download, resume and multi-URL failover
 * without an external process (unlike Aria2Download). Falls back to the
 * plain Qt download path when the library is unavailable.
 */
class PclDownloadTask : public Download {
    Q_OBJECT

   public:
    using Ptr = shared_qobject_ptr<PclDownloadTask>;

    explicit PclDownloadTask(QString path);

    static bool shouldUseFor(const QUrl& url);
    static Download::Ptr makeCached(QUrl url, MetaEntryPtr entry, Options options = Option::NoOptions);
    static Download::Ptr makeFile(QUrl url, QString path, Options options = Option::NoOptions);

    bool abort() override;
    int replyStatusCode() const override { return m_statusCode; }
    QNetworkReply::NetworkError error() const override { return m_error; }
    QString errorString() const override { return m_errorString; }

   protected slots:
    void executeTask() override;

   private slots:
    void onTaskEvent(int taskId, int eventType, qint64 downloaded, qint64 total, qint64 speed, int threads);

   private:
    void startQtFallback(const QString& reason);
    void finishDownload();
    void failOrAbort(const QString& reason);

   private:
    QString m_targetPath;
    int m_taskId = -1;
    MetaEntryPtr m_cacheEntry;
    bool m_isEternal = false;
    bool m_usingPcl = false;
    bool m_finished = false;
    int m_statusCode = -1;
    QNetworkReply::NetworkError m_error = QNetworkReply::NoError;
    QString m_errorString;
};

}  // namespace Net
