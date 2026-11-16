/// SPDX-License-Identifier: GPL-3.0-only
/*
 *  PrismLaucher - Minecraft Launcher
 *  Copyright (C) 2023 Rachel Powers <508861+Ryex@users.noreply.github.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *      Copyright 2013-2021 MultiMC Contributors
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#include "ProgressDialog.h"
#include <QPoint>
#include "ui_ProgressDialog.h"

#include <QDebug>
#include <QKeyEvent>
#include <limits>

#include "StringUtils.h"
#include "tasks/Task.h"
#include "tasks/TaskProgressSnapshot.h"

#include "ui/widgets/SubTaskProgressBar.h"

// Preserve small counters exactly; scale large byte totals without overflowing
// QProgressBar's int range or turning an exact 50% into 49% through truncation.
static std::tuple<int, int> mapProgress(qint64 current, qint64 total)
{
    current = qBound<qint64>(0, current, total);
    if (total <= std::numeric_limits<int>::max())
        return { int(current), int(total) };
    constexpr int scale = 1'000'000;
    return { int(static_cast<long double>(current) * scale / total), scale };
}

ProgressDialog::ProgressDialog(QWidget* parent) : QDialog(parent), ui(new Ui::ProgressDialog)
{
    ui->setupUi(this);
    ui->globalStatusLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    ui->taskProgressLayout->setAlignment(Qt::AlignTop);
    ui->taskProgressScrollArea->setHidden(true);
    auto speedSizePolicy = ui->globalSpeedWidget->sizePolicy();
    speedSizePolicy.setRetainSizeWhenHidden(true);
    ui->globalSpeedWidget->setSizePolicy(speedSizePolicy);
    ui->globalSpeedWidget->setHidden(true);
    this->setWindowFlags(this->windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setAttribute(Qt::WidgetAttribute::WA_QuitOnClose, true);
    changeProgress(0, 100);
    updateSize(true);
    setSkipButton(false);
}

void ProgressDialog::setSkipButton(bool present, QString label)
{
    ui->skipButton->setAutoDefault(false);
    ui->skipButton->setDefault(false);
    ui->skipButton->setFocusPolicy(Qt::ClickFocus);
    ui->skipButton->setEnabled(present);
    ui->skipButton->setVisible(present);
    ui->skipButton->setText(label);
    updateSize();
}

void ProgressDialog::on_skipButton_clicked(bool checked)
{
    Q_UNUSED(checked);
    if (!ui->skipButton->isEnabled()) return;
    if (m_observing) {
        m_cancelRequested = true;
        ui->skipButton->setEnabled(false);
        emit cancelRequested();
    } else if (m_task) {
        m_task->abort();
    }
}

ProgressDialog::~ProgressDialog()
{
    disconnect(m_taskDestroyedConnection);
    for (auto conn : this->m_taskConnections) {
        disconnect(conn);
    }
    delete ui;
}

void ProgressDialog::updateSize(bool recenterParent)
{
    QSize lastSize = this->size();
    QPoint lastPos = this->pos();
    int minHeight = ui->globalStatusDetailsLabel->minimumSize().height() + (ui->verticalLayout->spacing() * 2);
    minHeight += ui->globalProgressBar->minimumSize().height() + ui->verticalLayout->spacing();
    minHeight += ui->globalSpeedWidget->minimumSizeHint().height() + ui->verticalLayout->spacing();
    if (!ui->taskProgressScrollArea->isHidden())
        minHeight += ui->taskProgressScrollArea->minimumSizeHint().height() + ui->verticalLayout->spacing();
    if (ui->skipButton->isVisible())
        minHeight += ui->skipButton->height() + ui->verticalLayout->spacing();
    minHeight = std::max(minHeight, 60);
    QSize minSize = QSize(480, minHeight);

    setMinimumSize(minSize);

    // Grow to fit new content without shrinking either dimension that is already large enough.
    // adjustSize() cannot be used here because it can grow one dimension while shrinking the other.
    const QSize expandedSize = lastSize.expandedTo(sizeHint());
    if (lastSize != expandedSize) {
        resize(expandedSize);
    }

    QSize newSize = this->size();
    // if the current window is a different size
    auto parent = this->parentWidget();
    if (recenterParent && parent) {
        auto newX = std::max(0, parent->x() + ((parent->width() - newSize.width()) / 2));
        auto newY = std::max(0, parent->y() + ((parent->height() - newSize.height()) / 2));
        this->move(newX, newY);
    } else if (lastSize != newSize) {
        // center on old position after resize
        QSize sizeDiff = lastSize - newSize;  // last size was smaller, the results should be negative
        auto newX = std::max(0, lastPos.x() + (sizeDiff.width() / 2));
        auto newY = std::max(0, lastPos.y() + (sizeDiff.height() / 2));
        this->move(newX, newY);
    }
}

int ProgressDialog::execWithTask(Task* task)
{
    observeTask(task);

    if (!task) {
        qDebug() << "Programmer error: Progress dialog created with null task.";
        return QDialog::DialogCode::Accepted;
    }

    QDialog::DialogCode result{};
    if (handleImmediateResult(result)) {
        return result;
    }

    // Connect signals.
    this->m_taskConnections.push_back(connect(task, &Task::started, this, &ProgressDialog::onTaskStarted));
    this->m_taskConnections.push_back(connect(task, &Task::failed, this, &ProgressDialog::onTaskFailed));
    this->m_taskConnections.push_back(connect(task, &Task::succeeded, this, &ProgressDialog::onTaskSucceeded));
    this->m_taskConnections.push_back(connect(task, &Task::status, this, &ProgressDialog::changeStatus));
    this->m_taskConnections.push_back(connect(task, &Task::details, this, &ProgressDialog::changeStatus));
    this->m_taskConnections.push_back(connect(task, &Task::stepProgress, this, &ProgressDialog::changeStepProgress));
    this->m_taskConnections.push_back(connect(task, &Task::progress, this, &ProgressDialog::changeProgress));
    this->m_taskConnections.push_back(connect(task, &Task::transferRate, this, &ProgressDialog::changeTransferRate));
    this->m_taskConnections.push_back(connect(task, &Task::aborted, this, &ProgressDialog::hide));
    this->m_taskConnections.push_back(connect(task, &Task::abortStatusChanged, ui->skipButton, &QPushButton::setEnabled));
    this->m_taskConnections.push_back(connect(task, &Task::abortButtonTextChanged, ui->skipButton, &QPushButton::setText));

    m_is_multi_step = task->isMultiStep();
    ui->taskProgressScrollArea->setHidden(!m_is_multi_step);
    updateSize();

    // It's a good idea to start the task after we entered the dialog's event loop :^)
    if (!task->isRunning()) {
        QMetaObject::invokeMethod(task, &Task::start, Qt::QueuedConnection);
    } else {
        changeStatus(task->getStatus());
        changeProgress(task->getProgress(), task->getTotalProgress());
        if (task->getTransferRate() > 0) {
            changeTransferRate(task->getTransferRate());
        }
    }

    return QDialog::exec();
}

// TODO: only provide the unique_ptr overloads
int ProgressDialog::execWithTask(std::unique_ptr<Task>&& task)
{
    connect(this, &ProgressDialog::destroyed, task.get(), &Task::deleteLater);
    return execWithTask(task.release());
}
int ProgressDialog::execWithTask(std::unique_ptr<Task>& task)
{
    connect(this, &ProgressDialog::destroyed, task.get(), &Task::deleteLater);
    return execWithTask(task.release());
}

bool ProgressDialog::handleImmediateResult(QDialog::DialogCode& result)
{
    if (m_task->isFinished()) {
        if (m_task->wasSuccessful()) {
            result = QDialog::Accepted;
        } else {
            result = QDialog::Rejected;
        }
        return true;
    }
    return false;
}

Task* ProgressDialog::getTask()
{
    return m_task;
}

void ProgressDialog::observeTask(Task* task)
{
    if (m_task == task) return;
    disconnect(m_taskDestroyedConnection);
    m_task = task;
    if (task) {
        m_taskDestroyedConnection = connect(task, &QObject::destroyed, this, [this] {
            m_task = nullptr;
            ui->skipButton->setEnabled(false);
            hide();
        });
    }
}

void ProgressDialog::refreshFromTask(Task* task)
{
    m_observing = true;
    observeTask(task);
    if (!task) return;

    // Imports can switch from extraction to metadata, downloads and conversion.
    // Read the same leaf-task snapshot as Neo so nested wrappers cannot hide
    // file names, byte progress or transfer rates from the classic UI.
    changeStatus(task->getStatus());
    changeProgress(task->getProgress(), task->getTotalProgress());
    const auto snapshot = TaskProgressSnapshot::snapshot(task, 48);
    const auto children = snapshot.value("children").toArray();
    QSet<QUuid> visible;
    qint64 transferRate = 0;
    for (const auto& value : children) {
        const auto child = value.toObject();
        const auto state = child.value("state").toString();
        if (state != "running" && state != "waiting") continue;
        TaskStepProgress step(QUuid(child.value("id").toString()));
        step.label = child.value("label").toString();
        step.status = child.value("status").toString();
        step.details = child.value("details").toString();
        step.current = child.value("current").toInteger();
        step.total = child.value("total").toInteger(-1);
        step.progressUnit = child.value("progressUnit").toString();
        step.transferRate = child.value("transferRate").toInteger();
        step.state = state == "running" ? TaskStepState::Running : TaskStepState::Waiting;
        if (step.label.isEmpty() && step.status.isEmpty()) continue;
        visible.insert(step.uid);
        transferRate += step.transferRate;
        changeStepProgress(step);
    }
    // Retire rows when the underlying phase or batch changes. Keep the dialog's
    // widget count bounded even for imports containing thousands of downloads.
    for (auto it = taskProgress.begin(); it != taskProgress.end();) {
        if (!visible.contains(it.key())) {
            delete it.value();
            it = taskProgress.erase(it);
        } else {
            ++it;
        }
    }
    ui->taskProgressScrollArea->setVisible(!visible.isEmpty());
    transferRate = std::max(transferRate, task->getTransferRate());
    changeTransferRate(transferRate);
    // File-copy phases also report bytes, but they are not network transfers.
    ui->globalSpeedWidget->setVisible(transferRate > 0);
    ui->skipButton->setEnabled(!m_cancelRequested && task->isRunning() && task->canAbort());
    updateSize();
}

void ProgressDialog::onTaskStarted() {}

void ProgressDialog::onTaskFailed([[maybe_unused]] QString failure)
{
    reject();
    hide();
}

void ProgressDialog::onTaskSucceeded()
{
    accept();
    hide();
}

void ProgressDialog::changeStatus([[maybe_unused]] const QString& status)
{
    if (!m_task) return;
    ui->globalStatusLabel->setText(m_task->getStatus());
    ui->globalStatusDetailsLabel->setText(m_task->getDetails());

    updateSize();
}

void ProgressDialog::addTaskProgress(TaskStepProgress const& progress)
{
    SubTaskProgressBar* task_bar = new SubTaskProgressBar(this);
    taskProgress.insert(progress.uid, task_bar);
    ui->taskProgressLayout->addWidget(task_bar);
}

void ProgressDialog::changeStepProgress(TaskStepProgress const& task_progress)
{
    m_is_multi_step = true;
    if (ui->taskProgressScrollArea->isHidden()) {
        ui->taskProgressScrollArea->setHidden(false);
        updateSize();
    }

    if (!taskProgress.contains(task_progress.uid))
        addTaskProgress(task_progress);
    auto task_bar = taskProgress.value(task_progress.uid);

    if (task_progress.total <= 0) {
        task_bar->setRange(0, 0);
        task_bar->setValue(0);
    } else {
        auto const [mapped_current, mapped_total] = mapProgress(task_progress.current, task_progress.total);
        task_bar->setRange(0, mapped_total);
        task_bar->setValue(mapped_current);
    }

    auto status = task_progress.label;
    if (status.isEmpty()) status = task_progress.status;
    else if (!task_progress.status.isEmpty() && task_progress.status != status)
        status += QStringLiteral(" — ") + task_progress.status;
    task_bar->setStatus(status);
    QStringList details;
    if (!task_progress.details.isEmpty()) details.append(task_progress.details);
    if (task_progress.progressUnit == "bytes") {
        const auto current = StringUtils::humanReadableFileSize(std::max<qint64>(0, task_progress.current));
        const auto total = task_progress.total > 0 ? StringUtils::humanReadableFileSize(task_progress.total) : tr("Unknown");
        details.append(tr("%1 / %2").arg(current, total));
        task_bar->setFormat(task_progress.total > 0 ? tr("%1 / %2 (%p%)").arg(current, total) : "%p%");
        if (task_progress.transferRate > 0)
            details.append(tr("%1/s").arg(StringUtils::humanReadableFileSize(task_progress.transferRate)));
    } else {
        task_bar->setFormat(task_progress.progressUnit == "files" && task_progress.total > 0
            ? tr("%1 / %2 (%p%)").arg(task_progress.current).arg(task_progress.total) : "%p%");
    }
    task_bar->setDetails(details.join(QStringLiteral(" · ")));

    task_bar->setVisible(!task_progress.isDone());
}

void ProgressDialog::changeProgress(qint64 current, qint64 total)
{
    if (total <= 0) {
        ui->globalProgressBar->setRange(0, 0);
    } else {
        const auto [value, maximum] = mapProgress(current, total);
        ui->globalProgressBar->setRange(0, maximum);
        ui->globalProgressBar->setValue(value);
    }
}

void ProgressDialog::changeTransferRate(qint64 bytesPerSecond)
{
    ui->globalSpeedValueLabel->setText(tr("%1/s").arg(StringUtils::humanReadableFileSize(bytesPerSecond)));
    ui->globalSpeedWidget->setVisible(true);
}

void ProgressDialog::keyPressEvent(QKeyEvent* e)
{
    if (ui->skipButton->isVisible()) {
        if (e->key() == Qt::Key_Escape) {
            on_skipButton_clicked(true);
            return;
        } else if (e->key() == Qt::Key_Tab) {
            ui->skipButton->setFocusPolicy(Qt::StrongFocus);
            ui->skipButton->setFocus();
            ui->skipButton->setAutoDefault(true);
            ui->skipButton->setDefault(true);
            return;
        }
    }
    QDialog::keyPressEvent(e);
}

void ProgressDialog::closeEvent(QCloseEvent* e)
{
    if (m_task && m_task->isRunning()) {
        e->ignore();
    } else {
        QDialog::closeEvent(e);
    }
}
