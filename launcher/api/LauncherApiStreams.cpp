// SPDX-License-Identifier: GPL-3.0-only
#include "LauncherApiStreams.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QPointer>
#include <QTimer>
#include <QUuid>

#include "Application.h"
#include "InstanceList.h"
#include "api/LauncherApiSupport.h"
#include "api/LauncherApiDomains.h"
#include "minecraft/auth/AccountList.h"
#include "cli/OperationService.h"
#include "launch/LaunchTask.h"
#include "server/ServerLaunchTask.h"

struct LauncherApiStreams::Subscription {
    QString id, type, instance;
    struct Event { QJsonObject json; qsizetype bytes; };
    QList<Event> events;
    qint64 sequence = 0, notifyCursor = 0;
    qsizetype bytes = 0;
    QObject context;
    std::unique_ptr<QObject> taskContext;
};

namespace {
using namespace ApiSupport;
QString boundedLogPath(BaseInstance* instance, const QString& requested)
{
    auto roots = instance->getLogFileSearchPaths();
    roots.append(QDir(instance->gameRoot()).filePath("logs"));
    for (const auto& root : roots) {
        const auto rootPath = QFileInfo(root).canonicalFilePath();
        if (rootPath.isEmpty()) continue;
        const QFileInfo candidate(QDir::isAbsolutePath(requested) ? requested : QDir(root).filePath(requested));
        if (candidate.isFile() && candidate.canonicalFilePath().startsWith(rootPath + '/')) return candidate.absoluteFilePath();
    }
    return {};
}
QJsonObject subscriptionJson(const QString& id, const QString& type, const QString& instance)
{
    return { { "subscriptionId", id }, { "type", type }, { "instance", instance } };
}
}  // namespace

std::shared_ptr<LauncherApiStreams::Subscription> LauncherApiStreams::create(const QString& type, const QString& instance)
{
    if (m_subscriptions.size() >= 16) return {};
    auto sub = std::make_shared<Subscription>();
    sub->id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    sub->type = type;
    sub->instance = instance;
    m_subscriptions.insert(sub->id, sub);
    return sub;
}

void LauncherApiStreams::append(const std::shared_ptr<Subscription>& sub, QJsonObject event)
{
    event.insert("sequence", ++sub->sequence);
    event.insert("subscriptionId", sub->id);
    event.insert("instance", sub->instance);
    const auto size = QJsonDocument(event).toJson(QJsonDocument::Compact).size();
    sub->events.append({ event, size });
    sub->bytes += size;
    while (sub->events.size() > 512 || sub->bytes > 1024 * 1024) sub->bytes -= sub->events.takeFirst().bytes;
}

void LauncherApiStreams::attachConsole(const std::shared_ptr<Subscription>& sub, LaunchTask* task)
{
    sub->taskContext = std::make_unique<QObject>();
    append(sub, { { "kind", "console.reset" }, { "available", task != nullptr } });
    if (!task) return;
    const auto weak = std::weak_ptr<Subscription>(sub);
    if (auto server = dynamic_cast<ServerLaunchTask*>(task)) {
        QObject::connect(server, &ServerLaunchTask::readyRead, sub->taskContext.get(), [this, weak](const QByteArray& data) {
            const auto sub = weak.lock();
            if (!sub) return;
            for (qsizetype offset = 0; offset < data.size(); offset += 32768)
                append(sub, { { "kind", "console.data" }, { "encoding", "base64" },
                              { "data", QString::fromLatin1(data.mid(offset, 32768).toBase64()) } });
        });
    } else {
        const auto model = task->getLogModel();
        const auto emitRows = [this, weak, model = QPointer<LogModel>(model.get())](int first, int last) {
            const auto sub = weak.lock();
            if (!sub || !model) return;
            for (int row = first; row <= last; ++row) {
                const auto index = model->index(row, 0);
                const auto text = index.data().toString();
                append(sub, { { "kind", "console.line" }, { "text", text.left(16384) },
                              { "level", index.data(LogModel::LevelRole).toInt() }, { "truncated", text.size() > 16384 } });
            }
        };
        QObject::connect(model.get(), &QAbstractItemModel::rowsInserted, sub->taskContext.get(),
            [emitRows](const QModelIndex&, int first, int last) { emitRows(first, last); });
        QObject::connect(model.get(), &QAbstractItemModel::modelReset, sub->taskContext.get(), [this, weak] {
            if (const auto sub = weak.lock()) append(sub, { { "kind", "console.reset" }, { "available", true } });
        });
        emitRows(qMax(0, model->rowCount() - 200), model->rowCount() - 1);
    }
    QObject::connect(task, &QObject::destroyed, sub->taskContext.get(), [this, weak] {
        if (const auto sub = weak.lock()) append(sub, { { "kind", "console.reset" }, { "available", false } });
    });
}

QJsonObject LauncherApiStreams::subscribeConsole(const QJsonObject& p)
{
    const auto instance = APPLICATION->instances()->getInstanceById(p.value("instance").toString());
    if (!instance) return OperationService::failure("Instance ID not found.", 2);
    auto sub = create("console", instance->id());
    if (!sub) return OperationService::failure("At most 16 subscriptions may be active.", 2);
    const auto weak = std::weak_ptr<Subscription>(sub);
    QObject::connect(instance, &BaseInstance::launchTaskChanged, &sub->context, [this, weak](LaunchTask* task) {
        if (const auto sub = weak.lock()) attachConsole(sub, task);
    });
    QObject::connect(instance, &BaseInstance::runningStatusChanged, &sub->context, [this, weak](bool running) {
        if (const auto sub = weak.lock()) append(sub, { { "kind", "instance.state" }, { "running", running } });
    });
    QObject::connect(instance, &QObject::destroyed, &sub->context, [this, weak] {
        if (const auto sub = weak.lock()) append(sub, { { "kind", "instance.removed" } });
    });
    attachConsole(sub, instance->getLaunchTask());
    return OperationService::success(subscriptionJson(sub->id, sub->type, sub->instance));
}

QJsonObject LauncherApiStreams::subscribeFile(const QJsonObject& p)
{
    const auto instance = APPLICATION->instances()->getInstanceById(p.value("instance").toString());
    if (!instance) return OperationService::failure("Instance ID not found.", 2);
    const auto path = boundedLogPath(instance, p.value("file").toString());
    if (path.isEmpty()) return OperationService::failure("Log file not found in the instance log directories.", 2);
    auto sub = create("log-file", instance->id());
    if (!sub) return OperationService::failure("At most 16 subscriptions may be active.", 2);
    auto timer = new QTimer(&sub->context);
    const QFileInfo info(path);
    const auto initialOffset = qMax<qint64>(0, info.size() - p.value("tailBytes").toInteger(0));
    QObject::connect(timer, &QTimer::timeout, &sub->context,
        [this, weak = std::weak_ptr<Subscription>(sub), instance = QPointer<BaseInstance>(instance), path,
         offset = initialOffset, born = info.birthTime(), missing = false]() mutable {
            const auto sub = weak.lock();
            if (!sub) return;
            QFile file(path);
            if (!instance || boundedLogPath(instance, path).isEmpty() || !file.open(QIODevice::ReadOnly)) {
                if (!missing) append(sub, { { "kind", "log.unavailable" } });
                missing = true;
                return;
            }
            const QFileInfo current(path);
            if (missing || current.size() < offset || current.birthTime() != born) {
                offset = 0;
                append(sub, { { "kind", "log.reset" } });
            }
            missing = false;
            born = current.birthTime();
            if (!file.seek(offset)) return;
            const auto bytes = file.read(32768);
            if (bytes.isEmpty()) return;
            append(sub, { { "kind", "log.data" }, { "offset", offset }, { "encoding", "base64" },
                          { "data", QString::fromLatin1(bytes.toBase64()) } });
            offset += bytes.size();
        });
    timer->start(100);
    auto result = subscriptionJson(sub->id, sub->type, sub->instance);
    result.insert("offset", initialOffset);
    return OperationService::success(result);
}

QJsonObject LauncherApiStreams::poll(const std::shared_ptr<Subscription>& sub, qint64 after, int limit) const
{
    const auto oldest = sub->events.isEmpty() ? sub->sequence + 1 : sub->events.first().json.value("sequence").toInteger();
    QJsonArray events;
    qint64 next = after;
    qsizetype bytes = 0;
    for (const auto& event : sub->events) {
        const auto sequence = event.json.value("sequence").toInteger();
        if (sequence <= after) continue;
        if (events.size() >= limit || (bytes + event.bytes > 128 * 1024 && !events.isEmpty())) break;
        events.append(event.json);
        bytes += event.bytes;
        next = sequence;
    }
    return { { "subscriptionId", sub->id }, { "events", events }, { "nextCursor", next },
             { "dropped", qMax<qint64>(0, oldest - after - 1) }, { "hasMore", next < sub->sequence } };
}

QJsonArray LauncherApiStreams::notifications()
{
    QJsonArray result;
    for (const auto& sub : m_subscriptions) {
        if (sub->notifyCursor >= sub->sequence) continue;
        const auto batch = poll(sub, sub->notifyCursor, 64);
        sub->notifyCursor = batch.value("nextCursor").toInteger();
        result.append(batch);
    }
    return result;
}

LauncherApiStreams::~LauncherApiStreams() = default;
LauncherApiStreams::LauncherApiStreams(LauncherApi& api)
{
    using namespace ApiSupport;
    const auto instance = string("Installed instance ID.");
    const auto subscription = string("Subscription UUID from a subscribe operation.");
    api.registerOperation({ "launcher.update.subscribe", "Observe changes to updater markers and bounded log tail; includes the initial state.", schema({}), "streams" },
        [this](const QJsonObject&, UserInteraction&) {
            auto sub = create("launcher-update", {});
            if (!sub) return OperationService::failure("At most 16 subscriptions may be active.", 2);
            auto timer = new QTimer(&sub->context);
            const auto snapshot = launcherUpdateSnapshot();
            append(sub, { { "kind", "launcher.update.state" }, { "data", snapshot } });
            QObject::connect(timer, &QTimer::timeout, &sub->context,
                [this, weak = std::weak_ptr<Subscription>(sub), previous = snapshot]() mutable {
                    if (const auto sub = weak.lock()) {
                        const auto current = launcherUpdateSnapshot();
                        if (current != previous) {
                            previous = current;
                            append(sub, { { "kind", "launcher.update.state" }, { "data", current } });
                        }
                    }
                });
            timer->start(500);
            return OperationService::success(subscriptionJson(sub->id, sub->type, {}));
        });
    api.registerOperation({ "account.subscribe", "Subscribe to account, authentication activity and default selection changes. Includes an initial snapshot.", schema({}), "streams" },
        [this](const QJsonObject&, UserInteraction&) {
            auto sub = create("accounts", {});
            if (!sub) return OperationService::failure("At most 16 subscriptions may be active.", 2);
            const auto changed = [this, weak = std::weak_ptr<Subscription>(sub)] {
                if (const auto sub = weak.lock()) append(sub, { { "kind", "account.snapshot" }, { "data", launcherAccountSnapshot() } });
            };
            const auto list = APPLICATION->accounts();
            QObject::connect(list, &AccountList::listChanged, &sub->context, changed);
            QObject::connect(list, &AccountList::defaultAccountChanged, &sub->context, changed);
            QObject::connect(list, &AccountList::listActivityChanged, &sub->context, changed);
            QObject::connect(list, &QAbstractItemModel::dataChanged, &sub->context, changed);
            changed();
            return OperationService::success(subscriptionJson(sub->id, sub->type, {}));
        });
    api.registerOperation({ "instance.console.subscribe", "Subscribe to launch state and console output, including future launches.",
        schema({ { "instance", instance } }, { "instance" }), "streams" },
        [this](const QJsonObject& p, UserInteraction&) { return subscribeConsole(p); });
    api.registerOperation({ "instance.log.subscribe", "Follow an existing log file with bounded chunks and rotation/truncation notices.",
        schema({ { "instance", instance }, { "file", string("Log filename or absolute path within instance log roots.") },
            { "tailBytes", QJsonObject{ { "type", "integer" }, { "minimum", 0 }, { "maximum", 1048576 } } } }, { "instance", "file" }), "streams" },
        [this](const QJsonObject& p, UserInteraction&) { return subscribeFile(p); });
    api.registerOperation({ "event.poll", "Read a bounded event batch after a cursor without consuming other clients' history.",
        schema({ { "subscriptionId", subscription }, { "after", QJsonObject{ { "type", "integer" }, { "minimum", 0 } } },
            { "limit", QJsonObject{ { "type", "integer" }, { "minimum", 1 }, { "maximum", 256 } } } }, { "subscriptionId" }), "streams" },
        [this](const QJsonObject& p, UserInteraction&) {
            const auto sub = m_subscriptions.value(p.value("subscriptionId").toString());
            if (!sub) return OperationService::failure("Subscription not found.", 2);
            const auto after = p.value("after").toInteger(0);
            if (after > sub->sequence) return OperationService::failure("Cursor is ahead of the subscription.", 2);
            return OperationService::success(poll(sub, after, p.value("limit").toInt(64)));
        });
    api.registerOperation({ "event.subscriptions", "List active subscriptions in this API session.", schema({}), "streams" },
        [this](const QJsonObject&, UserInteraction&) {
            QJsonArray result;
            for (const auto& sub : m_subscriptions) result.append(subscriptionJson(sub->id, sub->type, sub->instance));
            return OperationService::success(result);
        });
    api.registerOperation({ "event.unsubscribe", "Close a subscription and discard its buffered events.",
        schema({ { "subscriptionId", subscription } }, { "subscriptionId" }), "streams" },
        [this](const QJsonObject& p, UserInteraction&) {
            return OperationService::success(QJsonObject{ { "removed", m_subscriptions.remove(p.value("subscriptionId").toString()) } });
        });
}
