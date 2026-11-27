// SPDX-FileCopyrightText: 2022 Sefa Eyeoglu <contact@scrumplex.net>
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "FileSystem.h"
#include "Filter.h"
#include "tasks/Task.h"

#include <QFuture>
#include <QFutureWatcher>

#include <functional>
#include <memory>

/*
 * Migrate existing data from other MMC-like launchers.
 */

class DataMigrationTask : public Task {
    Q_OBJECT
   public:
    struct CopyLocation {
        QString source;
        QString target;
        bool instances = false;
    };

    explicit DataMigrationTask(const QString& sourcePath, const QString& targetPath, Filter pathmatcher);
    DataMigrationTask(QList<CopyLocation> locations, std::function<QString()> finish);
    ~DataMigrationTask() override = default;

   protected:
    virtual void executeTask() override;

   protected slots:
    void dryRunFinished();
    void dryRunAborted();
    void copyFinished();
    void copyAborted();

   private:
    QString copyLocations() const;

    std::unique_ptr<FS::copy> m_copy;
    QList<CopyLocation> m_locations;
    std::function<QString()> m_finish;
    QFutureWatcher<QString> m_mergeWatcher;
    int m_toCopy = 0;
    QFuture<bool> m_copyFuture;
    QFutureWatcher<bool> m_copyFutureWatcher;
};
