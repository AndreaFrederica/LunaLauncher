// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QSet>
#include <QTimer>
#include <QUrl>
#include <QWebSocket>
#include <functional>

namespace Net {

struct Aria2DownloadInfo {
    QString gid;
    QString url;
    QString path;
    QString status;
    QString errorCode;
    QString errorMessage;
    qint64 completedLength = 0;
    qint64 totalLength = -1;
    int downloadSpeed = 0;
};

class Aria2Manager : public QObject {
    Q_OBJECT

   public:
    using AddCallback = std::function<void(bool, const QString&, const QString&)>;

    static Aria2Manager* instance();

    bool isEnabledBySettings() const;
    bool fallbackToQtEnabled() const;
    bool followLauncherDownloadLimits() const;
    int maxConcurrentDownloads() const;
    QString findExecutable() const;
    QString bundledExecutablePath() const;
    bool ensureStarted(QString* error = nullptr);
    bool isRunning() const;
    QString statusText() const;
    int rpcPort() const { return m_port; }

    void addDownload(const QUrl& url,
                     const QString& dir,
                     const QString& out,
                     const QJsonObject& extraOptions,
                     QObject* context,
                     AddCallback callback);
    void removeDownload(const QString& gid);
    QList<Aria2DownloadInfo> downloads() const;
    void clearFinished();
    void shutdown();

   signals:
    void statusChanged();
    void downloadsChanged();
    void downloadChanged(const QString& gid, const Net::Aria2DownloadInfo& info);
    void downloadGidChanged(const QString& oldGid, const QString& newGid, const Net::Aria2DownloadInfo& info);

   private slots:
    void poll();
    void processFinished(int exitCode, QProcess::ExitStatus exitStatus);

   private:
    explicit Aria2Manager(QObject* parent = nullptr);

    struct AddRequest {
        QUrl url;
        QString dir;
        QString out;
        QJsonObject extraOptions;
    };

    using RpcCallback = std::function<void(bool, const QJsonValue&, const QString&)>;

    void rpc(const QString& method, const QJsonArray& params, QObject* context, RpcCallback callback);
    void connectWebSocket();
    void handleWebSocketTextMessage(const QString& message);
    void refreshDownload(const QString& gid);
    void restartActiveDownloads(const QString& reason);
    void readdDownload(const QString& oldGid, const AddRequest& request);
    void failActiveDownloads(const QString& reason);
    QJsonObject downloadOptions(const QJsonObject& extraOptions) const;
    QJsonObject proxyOptions() const;
    void updateDownloadFromJson(const QString& gid, const QJsonObject& object);
    QString makeProxyUri(const QString& type, const QString& host, int port, const QString& user, const QString& pass) const;
    QStringList splitExtraArgs(const QString& raw) const;

   private:
    QProcess* m_process = nullptr;
    QNetworkAccessManager m_network;
    QWebSocket m_webSocket;
    QTimer m_pollTimer;
    QString m_secret;
    int m_port = 0;
    QString m_status;
    QHash<QString, Aria2DownloadInfo> m_downloads;
    QHash<QString, AddRequest> m_requests;
    QSet<QString> m_activeGids;
    bool m_shutdownRequested = false;
    int m_restartAttempts = 0;
};

}  // namespace Net
