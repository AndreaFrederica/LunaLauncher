// SPDX-License-Identifier: GPL-3.0-only
#include <QDir>
#include <QFontDatabase>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalSpy>
#include <QTest>

#include "ui/dialogs/ProgressDialog.h"

class ObservedProgressTask : public Task {
   public:
    QList<Task*> children;
    using Task::setProgress;
    using Task::setStatus;
    using Task::setTransferRate;
    QList<Task*> getSubTasks() const override { return children; }

   protected:
    void executeTask() override { setAbortable(true); }
};

class ProgressDialogTest : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase()
    {
#ifdef Q_OS_WIN
        // The offscreen plugin has no native Windows font database.
        QFontDatabase::addApplicationFont(QDir(qEnvironmentVariable("WINDIR")).filePath("Fonts/segoeui.ttf"));
#endif
    }

    void observesNestedDownloadsAlreadyInProgress()
    {
        ObservedProgressTask root, wrapper, client, metadata;
        root.children = {&wrapper};
        wrapper.children = {&client, &metadata};
        for (auto* task : {&root, &wrapper, &client, &metadata}) task->start();
        root.setStatus("Executing task 3 out of 5");
        root.setProgress(2, 5);
        client.setObjectName("FILE:https://example.invalid/minecraft-1.6.4.jar");
        client.setProgress(2 * 1024 * 1024, 5 * 1024 * 1024);
        client.setTransferRate(1024 * 1024);
        metadata.setObjectName("CACHE:https://example.invalid/net.minecraft/1.6.4.json");
        metadata.setStatus("Waiting for server response");
        metadata.setProgress(0, 0);

        ProgressDialog dialog;
        dialog.setWindowTitle("Import instance");
        dialog.setSkipButton(true, "Abort");
        dialog.refreshFromTask(&root);
        dialog.show();
        QCoreApplication::processEvents();
        auto* status = dialog.findChild<QLabel*>("globalStatusLabel");
        QCOMPARE(status->text(), root.getStatus());
        QVERIFY(status->height() >= status->heightForWidth(status->width()));
        QCOMPARE(dialog.findChildren<SubTaskProgressBar*>().size(), 2);
        QVERIFY(!dialog.findChild<QScrollArea*>("taskProgressScrollArea")->isHidden());
        QString labels;
        bool foundByteProgress = false, foundIndeterminateProgress = false;
        for (auto* row : dialog.findChildren<SubTaskProgressBar*>()) {
            labels += row->findChild<QLabel*>("statusLabel")->text();
            auto* bar = row->findChild<QProgressBar*>("progressBar");
            foundByteProgress |= bar->format().contains("MiB") && bar->value() > 0;
            foundIndeterminateProgress |= bar->maximum() == 0;
            if (bar->maximum() > 0) QVERIFY(bar->text().endsWith("(40%)"));
        }
        QVERIFY(labels.contains("minecraft-1.6.4.jar"));
        QVERIFY(labels.contains("1.6.4.json"));
        QVERIFY(labels.contains("Waiting for server response"));
        QVERIFY(foundByteProgress);
        QVERIFY(foundIndeterminateProgress);
        QCOMPARE(dialog.findChild<QProgressBar*>("globalProgressBar")->text(), QString("40%"));
        QVERIFY(dialog.findChild<QLabel*>("globalSpeedValueLabel")->text().contains("MiB/s"));
        dialog.refreshFromTask(&root);
        QCOMPARE(dialog.findChildren<SubTaskProgressBar*>().size(), 2);
        const auto screenshot = qEnvironmentVariable("LUNA_PROGRESS_TEST_SCREENSHOT");
        if (!screenshot.isEmpty()) QVERIFY(dialog.grab().save(screenshot));
    }

    void followsConversionAndRetiresDownloadRows()
    {
        ObservedProgressTask root, download, conversion;
        root.children = {&download};
        root.start();
        download.setObjectName("FILE:https://example.invalid/old.jar");
        download.start();
        conversion.start();
        ProgressDialog dialog;
        dialog.refreshFromTask(&root);
        QCOMPARE(dialog.getTask(), &root);
        QCOMPARE(dialog.findChildren<SubTaskProgressBar*>().size(), 1);
        QVERIFY(dialog.findChild<SubTaskProgressBar*>()->findChild<QLabel*>("statusLabel")->text().contains("old.jar"));
        TaskStepProgress step;
        step.label = "Copying instance content";
        step.progressUnit = "bytes";
        step.update(4LL * 1024 * 1024 * 1024, 8LL * 1024 * 1024 * 1024);
        conversion.propagateStepProgress(step);
        conversion.setProgress(step.current, step.total);
        dialog.refreshFromTask(&conversion);
        QCOMPARE(dialog.getTask(), &conversion);
        QCOMPARE(dialog.findChildren<SubTaskProgressBar*>().size(), 1);
        QVERIFY(!dialog.findChild<SubTaskProgressBar*>()->findChild<QLabel*>("statusLabel")->text().contains("old.jar"));
        QVERIFY(dialog.findChild<QWidget*>("globalSpeedWidget")->isHidden());
        auto* bar = dialog.findChild<QProgressBar*>("globalProgressBar");
        QVERIFY(bar->value() > 0);
        QVERIFY(qAbs(double(bar->value()) / bar->maximum() - 0.5) < 0.001);
        QCOMPARE(bar->text(), QString("50%"));
        ObservedProgressTask next;
        next.start();
        dialog.refreshFromTask(&next);
        QCOMPARE(dialog.findChildren<SubTaskProgressBar*>().size(), 0);
        QVERIFY(dialog.findChild<QWidget*>("globalSpeedWidget")->isHidden());
    }

    void cancellationDelegatesOnceAndStaysDisabledDuringCleanup()
    {
        ObservedProgressTask task, cleanup;
        task.start();
        cleanup.start();
        ProgressDialog dialog;
        dialog.setSkipButton(true, "Abort");
        dialog.refreshFromTask(&task);
        QSignalSpy cancel(&dialog, &ProgressDialog::cancelRequested);
        auto* button = dialog.findChild<QPushButton*>("skipButton");
        QVERIFY(button->isEnabled());
        button->click();
        QCOMPARE(cancel.count(), 1);
        QVERIFY(task.isRunning()); // LauncherApi owns cancellation and cleanup.
        dialog.refreshFromTask(&task);
        dialog.refreshFromTask(&cleanup);
        QVERIFY(!button->isEnabled());
        button->click();
        QCOMPARE(cancel.count(), 1);
    }

    void clearsDestroyedTaskAndCanObserveNextPhase()
    {
        ProgressDialog dialog;
        dialog.setSkipButton(true, "Abort");
        {
            ObservedProgressTask task;
            task.start();
            dialog.refreshFromTask(&task);
            QCOMPARE(dialog.getTask(), &task);
        }
        QVERIFY(!dialog.getTask());
        QVERIFY(!dialog.findChild<QPushButton*>("skipButton")->isEnabled());
        ObservedProgressTask next;
        next.start();
        dialog.refreshFromTask(&next);
        QCOMPARE(dialog.getTask(), &next);
        QVERIFY(dialog.findChild<QPushButton*>("skipButton")->isEnabled());
    }

    void unknownTotalsAndRestartedStepsRemainVisible()
    {
        ProgressDialog dialog;
        TaskStepProgress step;
        step.label = "Downloading metadata";
        step.total = 0;
        dialog.changeStepProgress(step);
        auto* row = dialog.findChild<SubTaskProgressBar*>();
        QVERIFY(row);
        QCOMPARE(row->findChild<QProgressBar*>("progressBar")->maximum(), 0);
        step.state = TaskStepState::Succeeded;
        dialog.changeStepProgress(step);
        QVERIFY(row->isHidden());
        step.state = TaskStepState::Running;
        dialog.changeStepProgress(step);
        QVERIFY(!row->isHidden());
    }
};

QTEST_MAIN(ProgressDialogTest)
#include "ProgressDialog_test.moc"
