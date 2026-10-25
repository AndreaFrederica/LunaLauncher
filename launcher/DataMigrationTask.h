// SPDX-FileCopyrightText: 2022 Sefa Eyeoglu <contact@scrumplex.net>
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "FileSystem.h"
#include "Filter.h"
#include "tasks/Task.h"

#include <QFuture>
#include <QFutureWatcher>

/*
 * Migrate existing data from other MMC-like launchers.
 */

class DataMigrationTask : public Task {
    Q_OBJECT
   public:
    explicit DataMigrationTask(const QString& sourcePath, const QString& targetPath, Filter pathmatcher);
    ~DataMigrationTask() override = default;

   protected:
    virtual void executeTask() override;

   protected slots:
    void dryRunFinished();
    void dryRunAborted();
    void copyFinished();
    void copyAborted();

   private:
    // Keep the paths by value. Migration tasks can outlive the caller that
    // requested them (the Neo UI starts one through the API), so references to
    // temporary or stack-owned strings would make the task unsafe.
    QString m_sourcePath;
    QString m_targetPath;
    const Filter m_pathMatcher;

    FS::copy m_copy;
    int m_toCopy = 0;
    QFuture<bool> m_copyFuture;
    QFutureWatcher<bool> m_copyFutureWatcher;
};
