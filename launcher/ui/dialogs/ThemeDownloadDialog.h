// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QDialog>
#include <QHash>
#include <QJsonObject>
#include <QPointer>
#include "QObjectPtr.h"
class AppearanceService;
class ThemeStoreTask;
class Task;
class QComboBox;
class QCheckBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QStackedWidget;
class QTextBrowser;
class QTreeWidget;
class QNetworkReply;
// Original name retained for existing Classic UI entry points.
class ThemeDownloadDialog final : public QDialog {
    Q_OBJECT
   public:
    explicit ThemeDownloadDialog(QWidget* parent = nullptr);
    static void openCenter();
   signals:
    void themesInstalled();

   private:
    void reloadSources();
    void manageSources();
    QJsonObject queryParameters() const;
    void query(bool refresh = false, int page = 0, const QString& contentId = {});
    void showDetails();
    void loadFiles(const QJsonObject& identity);
    void loadThumbnail(const QString& url, quint64 generation);
    void displayThumbnail(const QByteArray& bytes,quint64 generation,const QString& cachePath = {});
    void filter();
    void installSelected();
    void startInstall(QJsonObject request);
    void importLocal(bool directory);
    void refreshInstalled();
    void configureSelected();
    void removeSelected();
    void updateSelected();
    void refreshChoices(bool fromState);
    void updateState();
    void addTask(Task* task);
    Task* selectedTask() const;
    QJsonObject draft() const;
    QString describe(const QJsonObject& config) const;
    AppearanceService* m_service;
    QStackedWidget* m_pages;
    QListWidget* m_navigation;
    QLabel *m_summary, *m_status, *m_title, *m_thumbnail, *m_stateLabel, *m_draftStatus;
    QComboBox *m_source, *m_category, *m_sort, *m_files;
    QLineEdit* m_search;
    QCheckBox *m_compatible, *m_titlebarPalette;
    QTreeWidget *m_online, *m_installed, *m_tasks;
    QTextBrowser* m_description;
    QPushButton *m_install, *m_previous, *m_next, *m_remove, *m_update, *m_cancel, *m_retry, *m_undo;
    QHash<QString, QComboBox*> m_options;
    QHash<QString, QCheckBox*> m_allowed;
    shared_qobject_ptr<ThemeStoreTask> m_query, m_details;
    QPointer<QNetworkReply> m_imageReply;
    QJsonObject m_selected, m_loadedQuery;
    qint64 m_baseRevision = 0;
    quint64 m_queryGeneration = 0, m_detailGeneration = 0;
    int m_page = 0;
    bool m_filling = false, m_dirty = false;
};
