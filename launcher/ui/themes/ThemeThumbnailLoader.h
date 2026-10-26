// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QCache>
#include <QHash>
#include <QImage>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <functional>
class QNetworkAccessManager;
class QNetworkReply;

// Shared by store cards and the detail panel. Replies never own view items.
class ThemeThumbnailLoader final : public QObject {
   public:
    using Callback = std::function<void(const QImage&)>;
    ThemeThumbnailLoader(QNetworkAccessManager* network, QString cacheDirectory, QObject* parent);
    ~ThemeThumbnailLoader() override;
    void load(const QString& url, Callback callback, bool priority = false);
    void reset(bool forgetImages = false);

   private:
    void pump();
    void decode(const QString& key, QByteArray bytes, const QString& savePath, quint64 generation);
    void finish(const QString& key, const QImage& image, quint64 generation);
    QNetworkAccessManager* m_network;
    QString m_directory;
    QCache<QString, QImage> m_cache{ 16 * 1024 };
    QHash<QString, QList<Callback>> m_waiting;
    QStringList m_queue;
    QList<QPointer<QNetworkReply>> m_replies;
    quint64 m_generation = 0;
    int m_active = 0;
};
