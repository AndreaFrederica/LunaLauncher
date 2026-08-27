// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QObject>
#include <QProcess>
#include <QSet>
#include <QString>
#include <QTemporaryDir>
#include <QVector>

#include "modplatform/flame/CurseForgeExternalTool.h"

struct CurseForgeDownloadPage {
    QString url;
    QString fileName;
    QString hashAlgorithm;
    QString hash;
};

class CurseForgeDownloadPageService final : public QObject {
    Q_OBJECT

   public:
    enum class Provider { Embedded, External };

    explicit CurseForgeDownloadPageService(Provider provider = Provider::Embedded, QObject* parent = nullptr);
    ~CurseForgeDownloadPageService() override;

    static constexpr int ProtocolVersion = CurseForgeExternalTool::ProtocolVersion;

    static bool isSupportedUrl(const QString& url);
    static QString helperPath();
    static bool isAvailable();
    static QString resolveExternalToolPath(const QString& configuredPath);
    static QString externalToolPath();
    static bool isAvailable(Provider provider);
    static bool probeExternalTool(const QString& configuredPath, QString* error, bool* supportsHeadless = nullptr);

    bool open(const QVector<CurseForgeDownloadPage>& pages, const QString& clientMode = "gui");
    QString acceptDownloadedFile(const QString& filePath, const QString& verifiedHash);
    bool isRunning() const;
    QString downloadDirectory() const;
    QString errorString() const;
    Provider provider() const { return m_provider; }

   signals:
    void failed(const QString& reason);
    void downloadStarted(const QString& fileName, int fileIndex, int fileCount);
    void downloadProgress(const QString& fileName, int fileIndex, int fileCount, qint64 bytesReceived, qint64 bytesPerSecond);
    void downloadFailed(const QString& fileName, int fileIndex, int fileCount);
    void fileReady(const QString& fileName, int fileIndex, int fileCount);
    void retrying(const QString& fileName, int fileIndex, int fileCount);
    void downloadFinished(const QString& fileName, int fileIndex, int fileCount);
    void completed();

   private:
    bool writeRequest(const QVector<CurseForgeDownloadPage>& pages, int startIndex, int totalItems);
    bool startHelper();
    bool restartCurrentDownload();
    void handleOutputLine(const QByteArray& line);
    void reportFailure(const QString& reason);
    QString programPath() const;
    QString providerDisplayName() const;

    Provider m_provider;
    QProcess m_process;
    QTemporaryDir m_downloadDirectory;
    QString m_requestPath;
    QString m_requestId;
    QString m_clientMode;
    QString m_error;
    QByteArray m_stdoutBuffer;
    QVector<CurseForgeDownloadPage> m_pages;
    QSet<int> m_acceptedIndexes;
    int m_currentIndex = 1;
    bool m_restartRequested = false;
    bool m_failureReported = false;
};
