// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include "tasks/Task.h"

class ThemeManager;
class QWidget;
class WindowChromeController;

class AppearanceService final : public QObject {
    Q_OBJECT
   public:
    explicit AppearanceService(ThemeManager* manager);
    ~AppearanceService() override;
    void initialize();
    void reloadCatalog();
    QJsonObject state() const;
    QJsonObject capabilities() const;
    QJsonArray components() const { return m_components; }
    QJsonArray packages() const { return m_packages; }
    QJsonObject component(const QString& id) const;
    QJsonObject packageConfiguration(const QString& id, QString& error) const;
    QJsonObject apply(const QJsonObject& configuration, qint64 baseRevision, QString& error);
    bool undoPending(qint64 baseRevision, QString& error);
    void adoptLegacySettings();
    bool removePackage(const QString& id, QString& error);
    bool removeLegacyComponent(const QString& kind, const QString& id, QString& error);
    bool isUsed(const QString& componentId) const;
    QString packageDirectory() const;
    QString cacheDirectory() const;
    void preview(const QJsonObject& configuration, QWidget* parent, QString& error);
    QList<Task::Ptr> tasks() const { return m_tasks; }
    void retainTask(const Task::Ptr& task);
   signals:
    void stateChanged();
    void catalogChanged();
    void taskAdded(Task* task);
    void previewFailed(const QString& error);

   private:
    bool persist(QString& error, bool initializing = false) const;
    QJsonObject defaults() const;
    bool validate(const QJsonObject& config, QString& error) const;
    bool render(const QJsonObject& config, bool initial, QString& error);
    QJsonObject resolvedConfiguration(const QJsonObject& input) const;
    void installSystemWatchers();
    ThemeManager* m_manager;
    std::unique_ptr<WindowChromeController> m_chrome;
    QJsonArray m_components, m_packages;
    QJsonObject m_requested, m_effective, m_pending, m_lastGood;
    QString m_error;
    qint64 m_revision = 0;
    bool m_initialized = false, m_rendering = false;
    QList<Task::Ptr> m_tasks;
};
