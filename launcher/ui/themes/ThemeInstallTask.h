// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QFutureWatcher>
#include <QJsonObject>
#include <QTemporaryDir>
#include <atomic>
#include "tasks/Task.h"

class QNetworkAccessManager;
class ThemeStoreTask;
class NetJob;
class ThemeInstallTask final : public Task {
   public:
    ThemeInstallTask(QNetworkAccessManager* network, QJsonObject request, QString packageDirectory, QString cacheDirectory);
    ~ThemeInstallTask() override;
    QJsonObject result() const { return m_result; }
    QJsonObject request() const { return m_request; }
    bool abort() override;
    QList<Task*> getSubTasks() const override;

   protected:
    void executeTask() override;

   private:
    struct Staged {
        QString error, digest;
        QJsonObject record;
        std::shared_ptr<QTemporaryDir> directory;
    };
    void resolve();
    void download(const QJsonObject& file);
    void inspect(const QString& path, const QString& name);
    void publish();
    QNetworkAccessManager* m_network;
    QJsonObject m_request, m_result;
    QString m_packageDirectory, m_cacheDirectory, m_downloadError;
    std::shared_ptr<std::atomic_bool> m_cancelled;
    std::shared_ptr<QTemporaryDir> m_downloadDirectory;
    shared_qobject_ptr<ThemeStoreTask> m_resolver;
    shared_qobject_ptr<NetJob> m_download;
    QFutureWatcher<Staged> m_worker;
};
