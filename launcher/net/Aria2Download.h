// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "net/Aria2Manager.h"
#include "net/Download.h"

namespace Net {

class Aria2Download : public Download {
    Q_OBJECT

   public:
    using Ptr = shared_qobject_ptr<Aria2Download>;

    explicit Aria2Download(QString path);

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
    void aria2DownloadChanged(const QString& gid, const Net::Aria2DownloadInfo& info);
    void aria2DownloadGidChanged(const QString& oldGid, const QString& newGid, const Net::Aria2DownloadInfo& info);

   private:
    void startQtFallback(const QString& reason);
    void finishFromTempFile();
    QJsonObject buildAria2Options(QNetworkRequest& request) const;

   private:
    QString m_targetPath;
    QString m_tempPath;
    QString m_gid;
    bool m_finished = false;
    int m_statusCode = -1;
    QNetworkReply::NetworkError m_error = QNetworkReply::NoError;
    QString m_errorString;
};

}  // namespace Net
