// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QJsonObject>
#include <QNetworkReply>
#include <QPointer>
#include "tasks/Task.h"

class QNetworkAccessManager;
// One request object per query: callers retain identity/generation independently.
class ThemeStoreTask final : public Task {
   public:
    ThemeStoreTask(QNetworkAccessManager* network, QJsonObject request, QString cacheDirectory);
    ~ThemeStoreTask() override;
    QJsonObject result() const { return m_result; }
    bool abort() override;
    static QJsonObject parseOcs(const QByteArray& bytes, const QJsonObject& request, QString& error);
    static QJsonObject parseGitHub(const QByteArray& bytes, const QJsonObject& request, QString& error);
    static QString githubRepository(const QString& source);

   protected:
    void executeTask() override;

   private:
    void finishNetwork();
    bool loadCache(bool offline);
    void fail(const QString& error);
    QNetworkAccessManager* m_network;
    QJsonObject m_request, m_result, m_cacheIdentity;
    QString m_cacheDirectory, m_cachePath, m_networkError;
    QPointer<QNetworkReply> m_reply;
    QByteArray m_bytes;
};
