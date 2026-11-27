// SPDX-FileCopyrightText: 2022 Sefa Eyeoglu <contact@scrumplex.net>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "DataMigrationTask.h"

#include "FileSystem.h"

#include <QFileInfo>
#include <QTemporaryDir>

#include <QtConcurrent>

DataMigrationTask::DataMigrationTask(const QString& sourcePath, const QString& targetPath, Filter pathMatcher)
    : m_copy(std::make_unique<FS::copy>(sourcePath, targetPath))
{
    m_copy->matcher(std::move(pathMatcher)).whitelist(true);
}

DataMigrationTask::DataMigrationTask(QList<CopyLocation> locations, std::function<QString()> finish)
    : m_locations(std::move(locations)), m_finish(std::move(finish))
{
}

QString DataMigrationTask::copyLocations() const
{
    for (const auto& location : m_locations) {
        if (!location.instances) {
            FS::copy copy(location.source, location.target);
            copy.skipExisting(true);
            if (!copy())
                return tr("Could not copy %1.").arg(copy.failed().constFirst());
            continue;
        }

        // Publish each instance only once its complete directory is ready.
        // On retry, existing instances can be skipped as a whole.
        QDir target(location.target);
        if (!target.mkpath("."))
            return tr("Could not create %1.").arg(location.target);
        for (const auto& entry : QDir(location.source).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (entry.fileName() == ".tmp" || !QFileInfo(entry.filePath(), "instance.cfg").isFile())
                continue;
            const auto destination = target.filePath(entry.fileName());
            const QFileInfo existing(destination);
            if (existing.exists() || existing.isSymLink())
                continue;
            if (!target.mkpath(".tmp"))
                return tr("Could not create an instance staging directory in %1.").arg(location.target);
            QTemporaryDir staging(target.filePath(".tmp/luna-import-XXXXXX"));
            if (!staging.isValid())
                return tr("Could not stage an instance in %1.").arg(location.target);
            FS::copy copyInstance(entry.filePath(), staging.path());
            if (!copyInstance.skipExisting(true)())
                return tr("Could not copy %1.").arg(copyInstance.failed().constFirst());
            if (!target.rename(staging.path(), destination))
                return tr("Could not move the copied instance to %1.").arg(destination);
            staging.setAutoRemove(false);
        }
        const auto groups = QDir(location.source).filePath("instgroups.json");
        const auto destination = target.filePath("instgroups.json");
        if (QFileInfo::exists(groups) && !QFileInfo::exists(destination) && !QFile::copy(groups, destination))
            return tr("Could not copy %1.").arg(destination);
    }
    return {};
}

void DataMigrationTask::executeTask()
{
    if (m_finish) {
        setStatus(tr("Copying launcher data…"));
        connect(&m_mergeWatcher, &QFutureWatcher<QString>::finished, this, [this] {
            auto error = m_mergeWatcher.result();
            if (error.isEmpty())
                error = m_finish();
            if (error.isEmpty())
                emitSucceeded();
            else
                emitFailed(error);
        });
        m_mergeWatcher.setFuture(QtConcurrent::run(QThreadPool::globalInstance(), [this] { return copyLocations(); }));
        return;
    }

    setStatus(tr("Scanning files..."));

    // 1. Scan
    // Check how many files we gotta copy
    m_copyFuture = QtConcurrent::run(QThreadPool::globalInstance(), [this] {
        return (*m_copy)(true);  // dry run to collect amount of files
    });
    connect(&m_copyFutureWatcher, &QFutureWatcher<bool>::finished, this, &DataMigrationTask::dryRunFinished);
    connect(&m_copyFutureWatcher, &QFutureWatcher<bool>::canceled, this, &DataMigrationTask::dryRunAborted);
    m_copyFutureWatcher.setFuture(m_copyFuture);
}

void DataMigrationTask::dryRunFinished()
{
    disconnect(&m_copyFutureWatcher, &QFutureWatcher<bool>::finished, this, &DataMigrationTask::dryRunFinished);
    disconnect(&m_copyFutureWatcher, &QFutureWatcher<bool>::canceled, this, &DataMigrationTask::dryRunAborted);

    if (!m_copyFuture.isValid() || !m_copyFuture.result()) {
        emitFailed(tr("Failed to scan source path."));
        return;
    }

    // 2. Copy
    // Actually copy all files now.
    m_toCopy = m_copy->totalCopied();
    connect(m_copy.get(), &FS::copy::fileCopied, [this](const QString& relativeName) {
        QString shortenedName = relativeName;
        // shorten the filename to hopefully fit into one line
        if (shortenedName.length() > 50)
            shortenedName = relativeName.left(20) + "…" + relativeName.right(29);
        setProgress(m_copy->totalCopied(), m_toCopy);
        setStatus(tr("Copying %1…").arg(shortenedName));
    });
    m_copyFuture = QtConcurrent::run(QThreadPool::globalInstance(), [this] {
        return (*m_copy)();  // actually copy now
    });
    connect(&m_copyFutureWatcher, &QFutureWatcher<bool>::finished, this, &DataMigrationTask::copyFinished);
    connect(&m_copyFutureWatcher, &QFutureWatcher<bool>::canceled, this, &DataMigrationTask::copyAborted);
    m_copyFutureWatcher.setFuture(m_copyFuture);
}

void DataMigrationTask::dryRunAborted()
{
    emitAborted();
}

void DataMigrationTask::copyFinished()
{
    disconnect(&m_copyFutureWatcher, &QFutureWatcher<bool>::finished, this, &DataMigrationTask::copyFinished);
    disconnect(&m_copyFutureWatcher, &QFutureWatcher<bool>::canceled, this, &DataMigrationTask::copyAborted);

    if (!m_copyFuture.isValid() || !m_copyFuture.result()) {
        emitFailed(tr("Some paths could not be copied!"));
        return;
    }

    emitSucceeded();
}

void DataMigrationTask::copyAborted()
{
    emitAborted();
}
