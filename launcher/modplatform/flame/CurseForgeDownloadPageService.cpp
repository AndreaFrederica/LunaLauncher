// SPDX-License-Identifier: GPL-3.0-only

#include "CurseForgeDownloadPageService.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUrl>

#include "BuildConfig.h"
#include "Application.h"
#include "settings/SettingsObject.h"

CurseForgeDownloadPageService::CurseForgeDownloadPageService(QObject* parent)
    : QObject(parent), m_downloadDirectory(QDir::tempPath() + "/luna-curseforge-download-XXXXXX")
{
    connect(&m_process, &QProcess::readyReadStandardOutput, this, [this] {
        m_stdoutBuffer += m_process.readAllStandardOutput();
        while (true) {
            const auto newline = m_stdoutBuffer.indexOf('\n');
            if (newline < 0)
                break;

            const auto line = m_stdoutBuffer.left(newline).trimmed();
            m_stdoutBuffer.remove(0, newline + 1);
            if (!line.isEmpty()) {
                handleOutputLine(line);
            }
        }
    });
    connect(&m_process, &QProcess::readyReadStandardError, this, [this] {
        const auto output = QString::fromUtf8(m_process.readAllStandardError()).trimmed();
        if (!output.isEmpty()) {
            qWarning().noquote() << "[CurseForge WebView]" << output;
        }
    });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart || error == QProcess::Crashed) {
            reportFailure(m_process.errorString());
        }
    });
    connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
                if (exitStatus == QProcess::NormalExit && exitCode == 3 && m_restartRequested) {
                    m_restartRequested = false;
                    if (!restartCurrentDownload()) {
                        reportFailure(m_error);
                    }
                    return;
                }
                if (exitStatus == QProcess::CrashExit || exitCode != 0) {
                    reportFailure(tr("The embedded CurseForge browser exited unexpectedly (code %1).").arg(exitCode));
                } else {
                    emit completed();
                }
            });
}

CurseForgeDownloadPageService::~CurseForgeDownloadPageService()
{
    if (m_process.state() == QProcess::NotRunning)
        return;

    m_process.terminate();
    if (!m_process.waitForFinished(1000)) {
        m_process.kill();
        m_process.waitForFinished(1000);
    }
}

bool CurseForgeDownloadPageService::isSupportedUrl(const QString& rawUrl)
{
    const QUrl url(rawUrl);
    const auto host = url.host().toLower();
    return url.isValid() && url.scheme() == "https" && (host == "curseforge.com" || host == "www.curseforge.com") &&
           url.path().contains("/download/");
}

QString CurseForgeDownloadPageService::helperPath()
{
#ifdef Q_OS_WIN
    const QString helperName = "luna-cf-webview.exe";
#else
    const QString helperName = "luna-cf-webview";
#endif

    const QDir applicationDir(QCoreApplication::applicationDirPath());
    const QStringList candidates = {
        applicationDir.filePath(helperName),
        applicationDir.filePath("tools/curseforge-webview/" + helperName),
        applicationDir.filePath("../tools/curseforge-webview/" + helperName),
    };
    for (const auto& candidate : candidates) {
        const QFileInfo file(candidate);
        if (file.isFile() && file.isExecutable()) {
            return file.absoluteFilePath();
        }
    }
    return {};
}

bool CurseForgeDownloadPageService::isAvailable()
{
    return BuildConfig.CURSEFORGE_WEBVIEW_ENABLED && !helperPath().isEmpty();
}

bool CurseForgeDownloadPageService::open(const QVector<CurseForgeDownloadPage>& pages)
{
    if (isRunning())
        return true;
    if (pages.isEmpty()) {
        m_error = tr("No CurseForge download pages were provided.");
        return false;
    }
    if (!m_downloadDirectory.isValid()) {
        m_error = tr("Could not create a temporary CurseForge download directory.");
        return false;
    }

    const auto executable = helperPath();
    if (executable.isEmpty()) {
        m_error = tr("The embedded CurseForge browser is not available.");
        return false;
    }

    for (const auto& page : pages) {
        if (!isSupportedUrl(page.url)) {
            m_error = tr("Unsupported CurseForge download URL: %1").arg(page.url);
            return false;
        }
        if (page.fileName.isEmpty() || QFileInfo(page.fileName).fileName() != page.fileName) {
            m_error = tr("Invalid CurseForge file name: %1").arg(page.fileName);
            return false;
        }
    }

    m_pages = pages;
    m_currentIndex = 1;
    m_restartRequested = false;
    m_requestPath = QDir(m_downloadDirectory.path()).filePath("request.json");
    if (!writeRequest(m_pages, 1, m_pages.size()))
        return false;

    return startHelper();
}

bool CurseForgeDownloadPageService::writeRequest(const QVector<CurseForgeDownloadPage>& pages, int startIndex, int totalItems)
{
    QJsonArray items;
    for (const auto& page : pages)
        items.append(QJsonObject{ { "url", page.url }, { "fileName", page.fileName } });

    QSaveFile requestFile(m_requestPath);
    if (!requestFile.open(QIODevice::WriteOnly)) {
        m_error = requestFile.errorString();
        return false;
    }
    auto settings = APPLICATION->settings();
    const QJsonObject proxy{ { "type", settings->get("ProxyType").toString() },
                             { "host", settings->get("ProxyAddr").toString() },
                             { "port", settings->get("ProxyPort").toInt() } };
    const QJsonObject request{ { "downloadDirectory", QDir::toNativeSeparators(m_downloadDirectory.path()) },
                               { "items", items },
                               { "startIndex", startIndex },
                               { "totalItems", totalItems },
                               { "proxy", proxy } };
    if (requestFile.write(QJsonDocument(request).toJson(QJsonDocument::Compact)) < 0 || !requestFile.commit()) {
        m_error = requestFile.errorString();
        return false;
    }

    return true;
}

bool CurseForgeDownloadPageService::startHelper()
{
    const auto executable = helperPath();
    if (executable.isEmpty()) {
        m_error = tr("The embedded CurseForge browser is not available.");
        return false;
    }

    m_error.clear();
    m_stdoutBuffer.clear();
    m_failureReported = false;
    m_process.setProgram(executable);
    m_process.setArguments({ "--request", m_requestPath });
    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    m_process.start();
    if (!m_process.waitForStarted(3000)) {
        m_error = m_process.errorString();
        return false;
    }
    return true;
}

bool CurseForgeDownloadPageService::restartCurrentDownload()
{
    if (m_currentIndex < 1 || m_currentIndex > m_pages.size()) {
        m_error = tr("The CurseForge download queue cannot be restarted at item %1.").arg(m_currentIndex);
        return false;
    }

    const auto partialPath = QDir(m_downloadDirectory.path()).filePath(m_pages.at(m_currentIndex - 1).fileName);
    if (QFileInfo::exists(partialPath) && !QFile::remove(partialPath)) {
        m_error = tr("Could not remove the incomplete download: %1").arg(partialPath);
        return false;
    }

    const auto remaining = m_pages.mid(m_currentIndex - 1);
    if (!writeRequest(remaining, m_currentIndex, m_pages.size()))
        return false;
    return startHelper();
}

bool CurseForgeDownloadPageService::isRunning() const
{
    return m_process.state() != QProcess::NotRunning;
}

QString CurseForgeDownloadPageService::acceptDownloadedFile(const QString& filePath)
{
    const QFileInfo source(filePath);
    if (!isRunning() || m_restartRequested || m_currentIndex < 1 || m_currentIndex > m_pages.size() ||
        source.fileName().compare(m_pages.at(m_currentIndex - 1).fileName, Qt::CaseInsensitive) != 0)
        return {};

    const QFileInfo expectedSource(QDir(m_downloadDirectory.path()).filePath(m_pages.at(m_currentIndex - 1).fileName));
    if (source.canonicalFilePath() != expectedSource.canonicalFilePath())
        return {};

    QDir verifiedDirectory(QDir(m_downloadDirectory.path()).filePath("verified"));
    if (!verifiedDirectory.exists() && !QDir().mkpath(verifiedDirectory.path())) {
        qWarning() << "[CurseForge WebView] Could not create verified download directory:" << verifiedDirectory.path();
        return {};
    }
    const auto verifiedPath = verifiedDirectory.filePath(m_pages.at(m_currentIndex - 1).fileName);
    if (QFileInfo::exists(verifiedPath) && !QFile::remove(verifiedPath)) {
        qWarning() << "[CurseForge WebView] Could not replace verified download:" << verifiedPath;
        return {};
    }
    if (!QFile::copy(source.absoluteFilePath(), verifiedPath)) {
        qWarning() << "[CurseForge WebView] Could not preserve verified download:" << source.absoluteFilePath();
        return {};
    }

    const QJsonObject command{ { "command", "acceptCurrent" }, { "fileName", m_pages.at(m_currentIndex - 1).fileName } };
    m_process.write(QJsonDocument(command).toJson(QJsonDocument::Compact) + '\n');
    return verifiedPath;
}

QString CurseForgeDownloadPageService::downloadDirectory() const
{
    return m_downloadDirectory.path();
}

QString CurseForgeDownloadPageService::errorString() const
{
    return m_error;
}

void CurseForgeDownloadPageService::handleOutputLine(const QByteArray& line)
{
    qDebug().noquote() << "[CurseForge WebView]" << line;

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(line, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return;

    const auto object = document.object();
    const auto event = object.value("event").toString();
    const auto fileName = object.value("fileName").toString();
    const auto fileIndex = object.value("fileIndex").toInt();
    const auto fileCount = object.value("fileCount").toInt();
    if (event == "downloadStarted") {
        m_currentIndex = fileIndex;
        emit downloadStarted(fileName, fileIndex, fileCount);
    } else if (event == "downloadProgress") {
        emit downloadProgress(fileName, fileIndex, fileCount, object.value("bytesReceived").toVariant().toLongLong(),
                              object.value("bytesPerSecond").toVariant().toLongLong());
    } else if (event == "downloadFailed") {
        m_currentIndex = fileIndex;
        emit downloadFailed(fileName, fileIndex, fileCount);
    } else if (event == "retryRequested") {
        m_currentIndex = fileIndex;
        m_restartRequested = true;
        emit retrying(fileName, fileIndex, fileCount);
    } else if (event == "downloadFinished") {
        m_currentIndex = qMin(fileIndex + 1, m_pages.size());
        emit downloadFinished(fileName, fileIndex, fileCount);
    }
}

void CurseForgeDownloadPageService::reportFailure(const QString& reason)
{
    if (m_failureReported)
        return;
    m_failureReported = true;
    m_error = reason;
    emit failed(reason);
}
