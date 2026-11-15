// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QFuture>
#include <QFutureWatcher>
#include <QJsonObject>
#include <QTimer>

#include <atomic>
#include <mutex>
#include <memory>

#include "VanillaConversion.h"
#include "tasks/Task.h"

/**
 * Runs a PCL/HMCL/vanilla conversion outside the UI thread while exposing the
 * conversion as a normal Neo task.  The converter is intentionally kept
 * synchronous internally so its transaction and MMC-data checks remain in one
 * place; this wrapper forwards measured filesystem progress to the task centre.
 */
class VanillaConversionTask final : public Task {
    Q_OBJECT

   public:
    enum class Mode { Import, Export };
    using Work = std::function<VanillaConversionReport(const VanillaConversionFeedback&)>;

    VanillaConversionTask(Mode mode, QJsonObject parameters);
    explicit VanillaConversionTask(Work work);
    ~VanillaConversionTask() override;

    const VanillaConversionReport& report() const { return m_report; }

   protected:
    void executeTask() override;
    bool abort() override;

   private slots:
    void conversionFinished();
    void publishProgress();

   private:
    VanillaConversionReport runConversion(const VanillaConversionFeedback& feedback) const;
    QString targetForImport(QString* error) const;

    Mode m_mode = Mode::Import;
    Work m_work;
    QJsonObject m_parameters;
    QFuture<VanillaConversionReport> m_future;
    QFutureWatcher<VanillaConversionReport> m_watcher;
    QTimer m_phaseTimer;
    struct ProgressState {
        std::atomic_bool cancelled = false;
        std::mutex mutex;
        VanillaConversionProgress progress;
        quint64 revision = 0;
    };
    std::shared_ptr<ProgressState> m_progressState = std::make_shared<ProgressState>();
    quint64 m_publishedRevision = 0;
    TaskStepProgress m_step;
    VanillaConversionReport m_report;
    QString m_resolvedTarget;
    QString m_targetError;
};
