// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QTemporaryDir>
#include <QVector>

struct CurseForgeDownloadPage {
    QString url;
    QString fileName;
};

class CurseForgeDownloadPageService final : public QObject {
    Q_OBJECT

   public:
    explicit CurseForgeDownloadPageService(QObject* parent = nullptr);
    ~CurseForgeDownloadPageService() override;

    static bool isSupportedUrl(const QString& url);
    static QString helperPath();
    static bool isAvailable();

    bool open(const QVector<CurseForgeDownloadPage>& pages);
    QString acceptDownloadedFile(const QString& filePath);
    bool isRunning() const;
    QString downloadDirectory() const;
    QString errorString() const;

   signals:
    void failed(const QString& reason);
    void downloadStarted(const QString& fileName, int fileIndex, int fileCount);
    void downloadProgress(const QString& fileName, int fileIndex, int fileCount, qint64 bytesReceived, qint64 bytesPerSecond);
    void downloadFailed(const QString& fileName, int fileIndex, int fileCount);
    void retrying(const QString& fileName, int fileIndex, int fileCount);
    void downloadFinished(const QString& fileName, int fileIndex, int fileCount);
    void completed();

   private:
    bool writeRequest(const QVector<CurseForgeDownloadPage>& pages, int startIndex, int totalItems);
    bool startHelper();
    bool restartCurrentDownload();
    void handleOutputLine(const QByteArray& line);
    void reportFailure(const QString& reason);

    QProcess m_process;
    QTemporaryDir m_downloadDirectory;
    QString m_requestPath;
    QString m_error;
    QByteArray m_stdoutBuffer;
    QVector<CurseForgeDownloadPage> m_pages;
    int m_currentIndex = 1;
    bool m_restartRequested = false;
    bool m_failureReported = false;
};
