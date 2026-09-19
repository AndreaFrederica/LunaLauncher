// SPDX-License-Identifier: GPL-3.0-only

#include "McpServer.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QIODevice>
#include <QTextStream>
#include <QTimer>
#include <QUuid>

#include <functional>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <unistd.h>
#include <cerrno>
#include <QSocketNotifier>
#endif

#include "Application.h"
#include "api/LauncherApi.h"
#include "cli/UserInteraction.h"
#include "cli/OperationService.h"

namespace {

class McpInteraction final : public UserInteraction {
   public:
    using Ask = std::function<QJsonValue(const QString&, bool, const QJsonArray*)>;
    McpInteraction(QJsonObject parameters, std::function<void(const QJsonObject&)> notify, Ask ask = {})
        : m_parameters(std::move(parameters)), m_notify(std::move(notify)), m_ask(std::move(ask))
    {}

    void status(const QString& message) override { send("status", QJsonObject{ { "message", message } }); }
    void deviceCode(const QString& url, const QString& code, int expiresIn) override
    {
        send("device_code", QJsonObject{ { "url", url }, { "code", code }, { "expiresIn", expiresIn } });
    }
    std::optional<QString> input(const QString& prompt, bool secret) override
    {
        const auto key = secret ? "password" : "username";
        if (m_parameters.contains(key))
            return m_parameters.value(key).toString();
        const auto answer = m_ask ? m_ask(prompt, secret, nullptr) : QJsonValue();
        return answer.isString() ? std::optional<QString>(answer.toString()) : std::nullopt;
    }
    std::optional<int> select(const QString& prompt, const QJsonArray& choices) override
    {
        const auto answer = m_ask ? m_ask(prompt, false, &choices) : QJsonValue();
        return answer.isDouble() ? std::optional<int>(answer.toInt()) : std::nullopt;
    }

   private:
    void send(const QString& kind, QJsonObject data)
    {
        data.insert("kind", kind);
        m_notify(data);
    }

    QJsonObject m_parameters;
    std::function<void(const QJsonObject&)> m_notify;
    Ask m_ask;
};

QJsonObject objectSchema(QJsonObject properties, QJsonArray required = {})
{
    QJsonObject schema{ { "type", "object" }, { "properties", properties }, { "additionalProperties", false } };
    if (!required.isEmpty())
        schema.insert("required", required);
    return schema;
}

QString toolNameForOperation(const QString& operation)
{
    QString tool = operation;
    tool.replace('-', '_');
    tool.replace('.', '_');
    return QStringLiteral("lunalauncher_") + tool;
}

}  // namespace

McpServer::McpServer(QObject* parent) : QObject(parent), m_service() {}

void McpServer::start()
{
#ifdef Q_OS_WIN
    // Anonymous pipes are not waitable console handles. Poll available bytes so
    // a fragmented request never blocks Qt's task and network event loop.
    auto notifier = new QTimer(this);
    connect(notifier, &QTimer::timeout, this, &McpServer::readMessage);
    notifier->start(20);
#else
    auto notifier = new QSocketNotifier(STDIN_FILENO, QSocketNotifier::Read, this);
    connect(notifier, &QSocketNotifier::activated, this, &McpServer::readMessage);
#endif
    m_notifier = notifier;
    auto streams = new QTimer(this);
    connect(streams, &QTimer::timeout, this, [this] {
        if (m_disconnected) return;
        for (const auto& batch : m_service.streamNotifications())
            writeMessage({ { "jsonrpc", "2.0" }, { "method", "launcher/stream" }, { "params", batch } });
    });
    streams->start(50);
}

void McpServer::readMessage()
{
    if (m_disconnected)
        return;
    char buffer[65536];
#ifdef Q_OS_WIN
    DWORD available = 0, count = 0;
    const auto input = GetStdHandle(STD_INPUT_HANDLE);
    if (!PeekNamedPipe(input, nullptr, 0, nullptr, &available, nullptr)) {
        disconnectInput();
        return;
    }
    if (!available)
        return;
    if (!ReadFile(input, buffer, qMin<DWORD>(available, sizeof(buffer)), &count, nullptr) || !count) {
        disconnectInput();
        return;
    }
#else
    const auto count = ::read(STDIN_FILENO, buffer, sizeof(buffer));
    if (count < 0 && (errno == EINTR || errno == EAGAIN))
        return;
    if (count <= 0) {
        disconnectInput();
        return;
    }
#endif
    m_input.append(buffer, static_cast<int>(count));
    constexpr qsizetype maxMessageSize = 4 * 1024 * 1024;
    qsizetype newline;
    while ((newline = m_input.indexOf('\n')) >= 0) {
        const auto line = m_input.left(newline);
        m_input.remove(0, newline + 1);
        if (m_discardingLine) {
            m_discardingLine = false;
            continue;
        }
        if (line.size() > maxMessageSize) {
            writeError(QJsonValue(), -32600, QStringLiteral("Request exceeds 4 MiB."));
            continue;
        }
        // Queue all complete frames before invoking a handler. Long operations
        // run nested Qt loops, which must also receive replies and cancellation.
        QTimer::singleShot(0, this, [this, line] {
            if (m_disconnected)
                return;
            QJsonParseError parseError;
            const auto document = QJsonDocument::fromJson(line, &parseError);
            if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
                writeError(QJsonValue(), -32700, QStringLiteral("Parse error"));
                return;
            }
            handleMessage(document.object());
        });
    }
    if (m_input.size() > maxMessageSize) {
        if (!m_discardingLine)
            writeError(QJsonValue(), -32600, QStringLiteral("Request exceeds 4 MiB."));
        m_input.clear();
        m_discardingLine = true;
    }
}

void McpServer::disconnectInput()
{
    m_disconnected = true;
    if (m_notifier)
        m_notifier->deleteLater();
    m_cancelled = true;
    if (m_interactionLoop)
        m_interactionLoop->quit();
    if (m_activeService)
        m_activeService->cancelCurrent();
    else
        QCoreApplication::quit();
}

void McpServer::handleMessage(const QJsonObject& request)
{
    const auto id = request.value("id");
    const auto method = request.value("method").toString();
    const auto parameters = request.value("params").toObject();
    if (request.value("jsonrpc") != "2.0" || method.isEmpty() ||
        (!id.isUndefined() && !id.isString() && !id.isDouble()) ||
        (request.contains("params") && !request.value("params").isObject())) {
        writeError(QJsonValue(), -32600, QStringLiteral("Invalid JSON-RPC request."));
        return;
    }
    if (method == "initialize") {
        writeResult(
            id, QJsonObject{
                    { "protocolVersion", "2025-06-18" },
                    { "capabilities", QJsonObject{ { "tools", QJsonObject{ { "listChanged", false } } }, { "logging", QJsonObject() } } },
                    { "serverInfo", QJsonObject{ { "name", "lunalauncher" }, { "version", QCoreApplication::applicationVersion() } } } });
        return;
    }
    if (method == "notifications/initialized")
        return;
    if (method == "notifications/cancelled") {
        if (m_activeService && parameters.value("requestId") == m_activeRequestId) {
            m_cancelled = true;
            if (m_interactionLoop)
                m_interactionLoop->quit();
            m_activeService->cancelCurrent();
        }
        return;
    }
    if (id.isUndefined())
        return;
    if (method == "launcher/catalog") {
        writeResult(id, QJsonObject{ { "apiVersion", 1 }, { "operations", m_service.describe() } });
        return;
    }
    if (method == "launcher/respond") {
        if (!m_interactionLoop || parameters.value("interactionId").toString() != m_interactionId) {
            writeError(id, -32602, QStringLiteral("No matching pending interaction."));
            return;
        }
        const auto answer = parameters.value("value");
        const auto cancel = parameters.value("cancel").toBool();
        if (!cancel && ((m_choiceCount < 0 && !answer.isString()) ||
                        (m_choiceCount >= 0 && (!answer.isDouble() || answer.toDouble() != answer.toInt(-1) ||
                                               answer.toInt(-1) < 0 || answer.toInt() >= m_choiceCount)))) {
            writeError(id, -32602, QStringLiteral("Expected text or a valid zero-based choice index."));
            return;
        }
        m_interactionAnswer = cancel ? QJsonValue() : answer;
        auto loop = m_interactionLoop;
        m_interactionLoop = nullptr;
        writeResult(id, QJsonObject{ { "accepted", true } });
        loop->quit();
        return;
    }
    if (method == "launcher/execute") {
        if (!parameters.value("operation").isString() ||
            (parameters.contains("parameters") && !parameters.value("parameters").isObject())) {
            writeError(id, -32602, QStringLiteral("Expected operation and parameters object."));
            return;
        }
        executeOperation(id, parameters.value("operation").toString(), parameters.value("parameters").toObject(),
                         parameters.value("_meta").toObject(), true);
        return;
    }
    if (method == "ping") {
        writeResult(id, QJsonObject());
        return;
    }
    if (method == "logging/setLevel") {
        writeResult(id, QJsonObject());
        return;
    }
    if (method == "tools/list") {
        writeResult(id, QJsonObject{ { "tools", tools() } });
        return;
    }
    if (method == "resources/list") {
        writeResult(id, QJsonObject{ { "resources", QJsonArray() } });
        return;
    }
    if (method == "prompts/list") {
        writeResult(id, QJsonObject{ { "prompts", QJsonArray() } });
        return;
    }
    if (method == "tools/call") {
        const auto name = parameters.value("name").toString();
        const auto arguments = parameters.value("arguments").toObject();
        QString operation;
        if (operation.isEmpty()) {
            for (const auto& metadata : m_service.describe()) {
                const auto candidate = metadata.toObject();
                if (toolNameForOperation(candidate.value("name").toString()) == name) {
                    operation = candidate.value("name").toString();
                    break;
                }
            }
        }
        if (operation.isEmpty()) {
            writeError(id, -32602, QStringLiteral("Unknown tool: %1").arg(name));
            return;
        }
        executeOperation(id, operation, arguments, parameters.value("_meta").toObject(), false);
        return;
    }
    if (!id.isUndefined())
        writeError(id, -32601, QStringLiteral("Method not found"));
}

void McpServer::executeOperation(const QJsonValue& id, const QString& operation, const QJsonObject& arguments,
                                 const QJsonObject& metadata, bool native)
{
    const bool control = operation == "task.list" || operation == "task.status" || operation == "task.cancel" || operation == "api.describe" ||
                         operation == "event.poll" || operation == "event.subscriptions" || operation == "event.unsubscribe" ||
                         operation == "server.console.command" || operation == "server.console.write" || operation == "server.console.resize";
    if (m_activeService && (!control || id == m_activeRequestId)) {
        writeError(id, -32000, QStringLiteral("Another launcher operation is already running. Task controls remain available."));
        return;
    }
    const std::function<void(const QJsonObject&)> notify = [this, id, native, progressToken = metadata.value("progressToken"), progress = 0](const QJsonObject& event) mutable {
        if (native) {
            auto params = event;
            params.insert("requestId", id);
            writeMessage({ { "jsonrpc", "2.0" }, { "method", "launcher/event" }, { "params", params } });
        } else if (!progressToken.isUndefined()) {
            const auto message = QString::fromUtf8(QJsonDocument(event).toJson(QJsonDocument::Compact));
            const QJsonObject params{ { "progressToken", progressToken }, { "progress", ++progress }, { "message", message } };
            writeMessage({ { "jsonrpc", "2.0" }, { "method", "notifications/progress" }, { "params", params } });
        } else {
            const QJsonObject params{ { "level", "info" }, { "logger", "lunalauncher" }, { "data", event } };
            writeMessage({ { "jsonrpc", "2.0" }, { "method", "notifications/message" }, { "params", params } });
        }
    };
    McpInteraction::Ask ask;
    if (native)
        ask = [this](const QString& prompt, bool secret, const QJsonArray* choices) { return requestInteraction(prompt, secret, choices); };
    McpInteraction interaction(arguments, notify, ask);
    const bool ownsRequest = !m_activeService;
    if (ownsRequest) {
        m_activeService = &m_service;
        m_activeRequestId = id;
        m_cancelled = false;
    }
    QTimer progressTimer;
    if (native && !control) {
        connect(&progressTimer, &QTimer::timeout, &progressTimer, [this, &interaction, notify, last = QJsonObject()]() mutable {
            const auto state = m_service.execute("task.status", {}, interaction);
            if (state.value("ok").toBool() && state != last) {
                last = state;
                notify({ { "kind", "task" }, { "data", state.value("data") } });
            }
        });
        progressTimer.start(100);
    }
    const auto result = m_service.execute(operation, arguments, interaction);
    progressTimer.stop();
    if (ownsRequest) {
        m_activeService = nullptr;
        m_activeRequestId = QJsonValue();
    }
    if (native) {
        writeResult(id, result);
    } else {
        const auto text = QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact));
        writeResult(id, QJsonObject{ { "content", QJsonArray{ QJsonObject{ { "type", "text" }, { "text", text } } } },
                                     { "structuredContent", result }, { "isError", !result.value("ok").toBool() } });
    }
    if (m_disconnected && ownsRequest)
        QCoreApplication::quit();
}

QJsonValue McpServer::requestInteraction(const QString& prompt, bool secret, const QJsonArray* choices)
{
    if (m_cancelled || m_disconnected)
        return {};
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    m_interactionLoop = &loop;
    m_interactionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_choiceCount = choices ? choices->size() : -1;
    m_interactionAnswer = QJsonValue();
    QJsonObject event{ { "requestId", m_activeRequestId }, { "kind", "input" }, { "interactionId", m_interactionId },
                       { "prompt", prompt }, { "secret", secret }, { "expiresIn", 300 } };
    if (choices)
        event.insert("choices", *choices);
    writeMessage({ { "jsonrpc", "2.0" }, { "method", "launcher/event" }, { "params", event } });
    timeout.start(300000);
    loop.exec();
    m_interactionLoop = nullptr;
    m_interactionId.clear();
    const auto answer = m_cancelled ? QJsonValue() : m_interactionAnswer;
    m_interactionAnswer = QJsonValue();
    return answer;
}

void McpServer::writeMessage(const QJsonObject& message)
{
    QTextStream(stdout) << QJsonDocument(message).toJson(QJsonDocument::Compact) << Qt::endl;
}

void McpServer::writeResult(const QJsonValue& id, const QJsonValue& result)
{
    writeMessage(QJsonObject{ { "jsonrpc", "2.0" }, { "id", id }, { "result", result } });
}

void McpServer::writeError(const QJsonValue& id, int code, const QString& message)
{
    writeMessage(QJsonObject{ { "jsonrpc", "2.0" }, { "id", id }, { "error", QJsonObject{ { "code", code }, { "message", message } } } });
}

QJsonArray McpServer::tools() const
{
    QJsonArray result;
    for (const auto& value : m_service.describe()) {
        const auto operation = value.toObject();
        auto input = operation.value("inputSchema").toObject();
        if (input.isEmpty()) input = objectSchema({});
        result.append(QJsonObject{ { "name", toolNameForOperation(operation.value("name").toString()) },
            { "description", operation.value("description") }, { "inputSchema", input } });
    }
    return result;
}
