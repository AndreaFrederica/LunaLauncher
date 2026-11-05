// SPDX-License-Identifier: GPL-3.0-only
#include "VanillaConversionTask.h"

#include <QDir>
#include <QFileInfo>
#include <QtConcurrent>

#include "Application.h"
#include "FileSystem.h"
#include "InstanceStore.h"

namespace {
QString importPhase(int phase)
{
    static const QStringList phases{ QObject::tr("准备导入"), QObject::tr("扫描版本 JSON"),
                                     QObject::tr("复制游戏内容"), QObject::tr("生成 Prism 元数据"),
                                     QObject::tr("写入目标目录") };
    return phases.value(qBound(0, phase, phases.size() - 1));
}

QString exportPhase(int phase)
{
    static const QStringList phases{ QObject::tr("准备导出"), QObject::tr("检查 Prism 实例"),
                                     QObject::tr("复制游戏内容"), QObject::tr("合并 Loader 元数据"),
                                     QObject::tr("写入兼容目录") };
    return phases.value(qBound(0, phase, phases.size() - 1));
}
}  // namespace

VanillaConversionTask::VanillaConversionTask(Mode mode, QJsonObject parameters)
    : Task(), m_mode(mode), m_parameters(std::move(parameters))
{
    setAbortable(true);
}

void VanillaConversionTask::executeTask()
{
    // Resolve registered stores on the launcher thread. The worker only sees
    // immutable paths and performs filesystem work, avoiding access to the Qt
    // application models from a pool thread.
    if (m_mode == Mode::Import) {
        m_resolvedTarget = targetForImport(&m_targetError);
    }
    m_phase = 0;
    setProgress(0, 5);
    setStatus(m_mode == Mode::Import ? importPhase(m_phase) : exportPhase(m_phase));

    // Conversion itself is deliberately kept in a worker.  The phase timer
    // gives the UI useful feedback even though the legacy converter reports a
    // transactional result rather than per-file callbacks.
    m_phaseTimer.setInterval(350);
    m_phaseTimer.setSingleShot(false);
    connect(&m_phaseTimer, &QTimer::timeout, this, &VanillaConversionTask::advancePhase, Qt::UniqueConnection);
    connect(&m_watcher, &QFutureWatcher<VanillaConversionReport>::finished, this, &VanillaConversionTask::conversionFinished, Qt::UniqueConnection);
    m_future = QtConcurrent::run(QThreadPool::globalInstance(), [this] { return runConversion(); });
    m_watcher.setFuture(m_future);
    m_phaseTimer.start();
}

void VanillaConversionTask::advancePhase()
{
    if (m_phase >= 4) return;
    ++m_phase;
    setProgress(m_phase, 5);
    setStatus(m_mode == Mode::Import ? importPhase(m_phase) : exportPhase(m_phase));
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

VanillaConversionReport VanillaConversionTask::runConversion() const
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
    return VanillaConverter::exportToVanilla(m_parameters.value("source").toString(), m_parameters.value("target").toString(), options);
}

void VanillaConversionTask::conversionFinished()
{
    m_phaseTimer.stop();
    if (m_abortRequested.load()) {
        emitAborted();
        return;
    }
    m_report = m_future.result();
    const auto& report = m_report;
    setProgress(5, 5);
    if (!report.succeeded) {
        emitFailed(report.error.isEmpty() ? QObject::tr("转换失败。") : report.error);
        return;
    }
    if (!report.warnings.isEmpty())
        setDetails(report.warnings.join(QStringLiteral("；")));
    setStatus(QObject::tr("转换完成"));
    emitSucceeded();
}

bool VanillaConversionTask::abort()
{
    m_abortRequested.store(true);
    if (m_future.isRunning()) {
        m_future.cancel();
        return true;
    }
    if (!isFinished()) emitAborted();
    return true;
}
