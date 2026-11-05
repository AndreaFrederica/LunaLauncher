// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QFuture>
#include <QFutureWatcher>
#include <QJsonObject>
#include <QTimer>

#include <atomic>

#include "VanillaConversion.h"
#include "tasks/Task.h"

/**
 * Runs a PCL/HMCL/vanilla conversion outside the UI thread while exposing the
 * conversion as a normal Neo task.  The converter is intentionally kept
 * synchronous internally so its transaction and MMC-data checks remain in one
 * place; this wrapper provides lifecycle, cancellation and coarse phase
 * progress to the task centre.
 */
class VanillaConversionTask final : public Task {
    Q_OBJECT

   public:
    enum class Mode { Import, Export };

    VanillaConversionTask(Mode mode, QJsonObject parameters);
    ~VanillaConversionTask() override = default;

    const VanillaConversionReport& report() const { return m_report; }

   protected:
    void executeTask() override;
    bool abort() override;

   private slots:
    void conversionFinished();
    void advancePhase();

   private:
    VanillaConversionReport runConversion() const;
    QString targetForImport(QString* error) const;

    Mode m_mode;
    QJsonObject m_parameters;
    QFuture<VanillaConversionReport> m_future;
    QFutureWatcher<VanillaConversionReport> m_watcher;
    QTimer m_phaseTimer;
    std::atomic_bool m_abortRequested = false;
    int m_phase = 0;
    VanillaConversionReport m_report;
    QString m_resolvedTarget;
    QString m_targetError;
};
