// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QFileInfoList>
#include <QJsonObject>
#include <QTemporaryDir>
#include <memory>

#include "tasks/Task.h"

class BaseInstance;

/**
 * Asynchronous instance export. File enumeration and archive creation run
 * inside child tasks; the finished archive is published with QSaveFile.
 */
class InstanceExportTask final : public Task {
    Q_OBJECT

   public:
    InstanceExportTask(BaseInstance* instance, QJsonObject parameters);
    ~InstanceExportTask() override = default;

    void executeTask() override;
    bool abort() override;
    bool canAbort() const override { return true; }

   private:
    void publish();
    void fail(const QString& message);

    BaseInstance* m_instance = nullptr;
    QJsonObject m_parameters;
    std::unique_ptr<QTemporaryDir> m_staging;
    Task::Ptr m_child;
    QString m_output;
    QString m_stagedOutput;
    int m_fileCount = 0;
    bool m_aborted = false;
};
