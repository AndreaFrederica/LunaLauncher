// SPDX-License-Identifier: GPL-3.0-only
#include "VanillaConversionTask.h"

#include <QDir>
#include <QFileInfo>
#include <QtConcurrent>

#include "Application.h"
#include "FileSystem.h"
#include "InstanceStore.h"

VanillaConversionTask::VanillaConversionTask(Mode mode, QJsonObject parameters)
    : Task(), m_mode(mode), m_parameters(std::move(parameters))
{
    setAbortable(true);
}

VanillaConversionTask::VanillaConversionTask(Work work) : Task(), m_work(std::move(work))
{
    setAbortable(true);
}

VanillaConversionTask::~VanillaConversionTask()
{
    m_progressState->cancelled.store(true);
    // Paths owned by the caller must outlive the filesystem worker.
    m_future.waitForFinished();
}

void VanillaConversionTask::executeTask()
{
    // Resolve registered stores on the launcher thread. The worker only sees
    // immutable paths and performs filesystem work, avoiding access to the Qt
    // application models from a pool thread.
    if (!m_work && m_mode == Mode::Import) {
        m_resolvedTarget = targetForImport(&m_targetError);
    }
    setProgress(0, 0);
    setStatus(tr("正在准备文件处理"));

    // Coalesce worker updates instead of queueing a Qt event for every small
    // file or buffer. Only the launcher thread touches Task state/signals.
    const auto state = m_progressState;
    VanillaConversionFeedback feedback;
    feedback.cancelled = [state] { return state->cancelled.load(); };
    feedback.progress = [state](const VanillaConversionProgress& progress) {
        std::lock_guard lock(state->mutex);
        state->progress = progress;
        ++state->revision;
    };
    m_phaseTimer.setInterval(100);
    m_phaseTimer.setSingleShot(false);
    connect(&m_phaseTimer, &QTimer::timeout, this, &VanillaConversionTask::publishProgress, Qt::UniqueConnection);
    connect(&m_watcher, &QFutureWatcher<VanillaConversionReport>::finished, this, &VanillaConversionTask::conversionFinished, Qt::UniqueConnection);
    m_future = QtConcurrent::run(QThreadPool::globalInstance(), [this, feedback] {
        return m_work ? m_work(feedback) : runConversion(feedback);
    });
    m_watcher.setFuture(m_future);
    m_phaseTimer.start();
}

void VanillaConversionTask::publishProgress()
{
    VanillaConversionProgress progress;
    {
        std::lock_guard lock(m_progressState->mutex);
        if (m_publishedRevision == m_progressState->revision) return;
        m_publishedRevision = m_progressState->revision;
        progress = m_progressState->progress;
    }
    setStatus(progress.stage);
    setDetails(progress.details);
    setProgress(progress.current, progress.total);
    m_step.label = progress.stage;
    m_step.status = progress.stage;
    m_step.details = progress.details;
    m_step.progressUnit = progress.unit;
    m_step.update(progress.current, progress.total);
    propagateStepProgress(m_step);
}

QString VanillaConversionTask::targetForImport(QString* error) const
{
    auto target = m_parameters.value("target").toString().trimmed();
    const auto storeId = m_parameters.value("targetStoreId").toString().trimmed();
    if (!storeId.isEmpty()) {
        const auto* store = APPLICATION->instanceStores()->find(storeId);
        if (!store || !store->isPrism() || !store->enabled || !store->writable) {
            if (error) *error = QObject::tr("The selected target store is not an enabled writable Prism store.");
            return {};
        }
        const auto name = m_parameters.value("name").toString().trimmed().isEmpty()
            ? QFileInfo(m_parameters.value("source").toString()).fileName()
            : m_parameters.value("name").toString().trimmed();
        target = QDir(store->path).filePath(FS::DirNameFromString(name, store->path));
    }
    if (target.isEmpty()) {
        const auto* store = APPLICATION->instanceStores()->defaultStore();
        if (!store) {
            if (error) *error = QObject::tr("No writable Prism Store is configured for the conversion.");
            return {};
        }
        const auto name = m_parameters.value("name").toString().trimmed().isEmpty()
            ? QFileInfo(m_parameters.value("source").toString()).fileName()
            : m_parameters.value("name").toString().trimmed();
        target = QDir(store->path).filePath(FS::DirNameFromString(name, store->path));
    }
    return target;
}

VanillaConversionReport VanillaConversionTask::runConversion(const VanillaConversionFeedback& feedback) const
{
    VanillaConversionReport report;
    if (m_mode == Mode::Import) {
        if (m_resolvedTarget.isEmpty()) {
            report.error = m_targetError;
            return report;
        }
        VanillaImportOptions options;
        options.name = m_parameters.value("name").toString();
        options.group = m_parameters.value("group").toString();
        options.copyGameContent = m_parameters.value("copyGameContent").toBool(true);
        options.feedback = feedback;
        return VanillaConverter::importToPrism(m_parameters.value("source").toString(), m_resolvedTarget, options);
    }

    VanillaExportOptions options;
    options.versionId = m_parameters.value("versionId").toString();
    options.versionJson = m_parameters.value("versionJson").toObject();
    options.dialect = m_parameters.value("dialect").toString();
    options.name = m_parameters.value("name").toString();
    options.group = m_parameters.value("group").toString();
    options.extensionFields = m_parameters.value("extensionFields").toObject();
    options.copyGameContent = m_parameters.value("copyGameContent").toBool(true);
    options.feedback = feedback;
    return VanillaConverter::exportToVanilla(m_parameters.value("source").toString(), m_parameters.value("target").toString(), options);
}

void VanillaConversionTask::conversionFinished()
{
    m_phaseTimer.stop();
    publishProgress();
    m_report = m_future.result();
    if (m_progressState->cancelled.load() && !m_report.succeeded) {
        emitAborted();
        return;
    }
    const auto& report = m_report;
    if (!report.succeeded) {
        m_step.state = TaskStepState::Failed;
        propagateStepProgress(m_step);
        emitFailed(report.error.isEmpty() ? QObject::tr("转换失败。") : report.error);
        return;
    }
    setProgress(1, 1);
    m_step.update(m_step.total > 0 ? m_step.total : 1, m_step.total > 0 ? m_step.total : 1);
    m_step.state = TaskStepState::Succeeded;
    propagateStepProgress(m_step);
    if (!report.warnings.isEmpty())
        setDetails(report.warnings.join(QStringLiteral("；")));
    setStatus(QObject::tr("转换完成"));
    emitSucceeded();
}

bool VanillaConversionTask::abort()
{
    if (isFinished()) return false;
    m_progressState->cancelled.store(true);
    // Even a completed future may still have its completion signal queued.
    // Finish only in conversionFinished(), after cleanup and commit outcome.
    setStatus(tr("正在取消并清理临时文件"));
    return true;
}
