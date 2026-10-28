// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "Task.h"
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QUrl>
#include <algorithm>

namespace TaskProgressSnapshot {
inline QString stateName(Task::State state)
{
    switch (state) {
        case Task::State::Running: return "running";
        case Task::State::Succeeded: return "succeeded";
        case Task::State::Failed: return "failed";
        case Task::State::AbortedByUser: return "cancelled";
        default: return "waiting";
    }
}

inline QString downloadLabel(Task* task)
{
    const auto name = task->objectName();
    for (const auto& prefix : { "FILE:", "CACHE:", "BYTES:", "ARIA2_FILE:", "ARIA2_CACHE:", "PCL_FILE:", "PCL_CACHE:" }) {
        if (name.startsWith(QLatin1String(prefix)))
            return QFileInfo(QUrl(name.mid(qstrlen(prefix))).path()).fileName();
    }
    return {};
}

// Traverse every leaf before limiting display rows. Proxy step caches duplicate
// descendants (and retain old stages), so prefer their actual task tree.
inline void collect(Task* task, QHash<QString, QJsonObject>& rows, QSet<Task*>& visited, bool includeSelf = false)
{
    if (!task || visited.contains(task)) return;
    visited.insert(task);
    const auto children = task->getSubTasks();
    if (!children.isEmpty()) {
        for (auto* child : children) collect(child, rows, visited, true);
        return;
    }
    const auto steps = task->getStepProgress();
    if (!steps.isEmpty()) {
        for (const auto& step : steps) {
            if (!step) continue;
            const auto id = step->uid.toString(QUuid::WithoutBraces);
            const auto state = step->state == TaskStepState::Running ? "running" :
                               step->state == TaskStepState::Succeeded ? "succeeded" :
                               step->state == TaskStepState::Failed ? "failed" : "waiting";
            rows.insert(id, QJsonObject{ { "id", id }, { "label", step->label }, { "state", state },
                { "current", step->current }, { "total", step->total }, { "progressUnit", step->progressUnit },
                { "status", step->status }, { "details", step->details },
                { "transferRate", step->isDone() ? 0 : step->transferRate }, { "finished", step->isDone() } });
        }
    } else if (includeSelf) {
        const auto id = task->getUid().toString(QUuid::WithoutBraces);
        const auto fileName = downloadLabel(task);
        rows.insert(id, QJsonObject{ { "id", id },
            { "label", fileName.isEmpty() ? (task->getStatus().isEmpty() ? task->objectName() : task->getStatus()) : fileName },
            { "state", stateName(task->getState()) }, { "running", task->isRunning() }, { "finished", task->isFinished() },
            { "current", task->getProgress() }, { "total", task->getState() == Task::State::Inactive ? -1 : task->getTotalProgress() },
            { "progressUnit", fileName.isEmpty() ? "progress" : "bytes" },
            { "status", task->getStatus() }, { "details", task->getDetails() },
            { "transferRate", task->isRunning() ? task->getTransferRate() : 0 } });
    }
}

inline QJsonObject snapshot(Task* task, int limit = 96)
{
    QHash<QString, QJsonObject> unique;
    QSet<Task*> visited;
    collect(task, unique, visited);
    auto rows = unique.values();
    auto priority = [](const QJsonObject& row) {
        const auto state = row.value("state").toString();
        return state == "running" ? 0 : state == "failed" ? 1 : state == "waiting" ? 2 : 3;
    };
    std::sort(rows.begin(), rows.end(), [&](const auto& a, const auto& b) {
        return priority(a) == priority(b) ? a.value("id").toString() < b.value("id").toString() : priority(a) < priority(b);
    });
    int succeeded = 0, failed = 0, cancelled = 0, running = 0, waiting = 0, unknownBytes = 0;
    qint64 currentBytes = 0, totalBytes = 0;
    bool hasBytes = false;
    for (const auto& row : rows) {
        const auto state = row.value("state").toString();
        succeeded += state == "succeeded";
        failed += state == "failed";
        cancelled += state == "cancelled";
        running += state == "running";
        waiting += state == "waiting";
        if (row.value("progressUnit") != "bytes") continue;
        hasBytes = true;
        const auto total = row.value("total").toInteger();
        const auto current = std::max<qint64>(0, row.value("current").toInteger());
        currentBytes += total > 0 ? std::min(current, total) : current;
        if (total > 0) totalBytes += total;
        else ++unknownBytes;
    }
    QJsonArray visible;
    for (int i = 0; i < std::min(limit, static_cast<int>(rows.size())); ++i) visible.append(rows[i]);
    return { { "children", visible }, { "childrenTruncated", rows.size() > limit },
        { "summary", QJsonObject{ { "total", rows.size() }, { "succeeded", succeeded }, { "failed", failed },
            { "cancelled", cancelled }, { "running", running }, { "waiting", waiting },
            { "hasBytes", hasBytes }, { "currentBytes", currentBytes }, { "totalBytes", totalBytes },
            { "unknownByteTotals", unknownBytes } } } };
}
} // namespace TaskProgressSnapshot
