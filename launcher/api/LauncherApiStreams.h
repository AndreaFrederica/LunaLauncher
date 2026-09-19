// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <memory>

class LauncherApi;
class LaunchTask;

// Per-session subscriptions. Both polling consumers and the native transport
// have independent cursors into the same bounded event history.
class LauncherApiStreams {
   public:
    explicit LauncherApiStreams(LauncherApi& api);
    ~LauncherApiStreams();
    QJsonArray notifications();

   private:
    struct Subscription;
    std::shared_ptr<Subscription> create(const QString& type, const QString& instance);
    void append(const std::shared_ptr<Subscription>& sub, QJsonObject event);
    void attachConsole(const std::shared_ptr<Subscription>& sub, LaunchTask* task);
    QJsonObject subscribeConsole(const QJsonObject& p);
    QJsonObject subscribeFile(const QJsonObject& p);
    QJsonObject poll(const std::shared_ptr<Subscription>& sub, qint64 after, int limit) const;
    QHash<QString, std::shared_ptr<Subscription>> m_subscriptions;
};
