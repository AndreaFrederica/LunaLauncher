// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QUrl>
#include "AuthSession.h"
#include "QObjectPtr.h"

struct AccountData;
class AuthlibInjectorDownloadTask;

// Owns the Rust child process. The management credential lives only in memory
// and is delivered through the child's stdout pipe, never a command line/file.
class LocalYggdrasil : public QObject {
   public:
    static LocalYggdrasil& instance();
    bool ensureRunning(QString& error);
    QString apiUrl() const { return m_url; }
    bool request(const QByteArray& method,
                 const QString& path,
                 const QByteArray& body,
                 QJsonObject& result,
                 QString& error,
                 const QByteArray& contentType = "application/json");
    bool createProfile(const QString& name, const QString& password, const QString& uuid, QJsonObject& profile, QString& error);
    QUrl metadataUrl(const QUrl& source);
    QUrl textureUrl(const QUrl& source);
    bool isCacheUrl(const QUrl& url) const;
    shared_qobject_ptr<QNetworkAccessManager> cacheNetwork() const { return m_cacheNetwork; }
    void saveAccountSnapshot(AccountData& data);
    void restoreAccountSnapshot(AccountData& data);
    void removeAccountSnapshot(const QString& uuid);
    bool hasAccountSnapshot(AccountData& data);
    void refreshAccountSnapshotStatus(AccountData& data);
    bool createAccountSession(AccountData& data, AuthSessionPtr session, QString& error);

   private:
    explicit LocalYggdrasil(QObject* parent);
    void stop();
    bool configureCache();
    bool ensureCacheService();
    bool ensureAccountCacheService();
    QProcess m_process;
    QString m_url;
    QByteArray m_managementToken;
    bool m_starting = false;
    QByteArray m_cacheConfiguration;
    QString m_metadataPath;
    QString m_texturesPath;
    qint64 m_cacheRetryAfter = 0;
    shared_qobject_ptr<QNetworkAccessManager> m_cacheNetwork;
    QPointer<AuthlibInjectorDownloadTask> m_agentPreparation;
};
