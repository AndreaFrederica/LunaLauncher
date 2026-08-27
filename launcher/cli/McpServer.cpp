// SPDX-License-Identifier: GPL-3.0-only

#include "McpServer.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QTimer>

#include <functional>

#ifdef Q_OS_WIN
#include <windows.h>
#include <QWinEventNotifier>
#else
#include <unistd.h>
#include <QSocketNotifier>
#endif

#include "Application.h"
#include "cli/OperationService.h"

namespace {

class McpInteraction final : public UserInteraction {
   public:
    McpInteraction(QJsonObject parameters, std::function<void(const QJsonObject&)> notify)
        : m_parameters(std::move(parameters)), m_notify(std::move(notify))
    {}

    void status(const QString& message) override { send("status", QJsonObject{ { "message", message } }); }
    void deviceCode(const QString& url, const QString& code, int expiresIn) override
    {
        send("device_code", QJsonObject{ { "url", url }, { "code", code }, { "expiresIn", expiresIn } });
    }
    std::optional<QString> input(const QString&, bool secret) override
    {
        const auto key = secret ? "password" : "username";
        if (!m_parameters.contains(key))
            return std::nullopt;
        return m_parameters.value(key).toString();
    }
    std::optional<int> select(const QString&, const QJsonArray&) override { return std::nullopt; }

   private:
    void send(const QString& kind, QJsonObject data)
    {
        data.insert("kind", kind);
        m_notify(data);
    }

    QJsonObject m_parameters;
    std::function<void(const QJsonObject&)> m_notify;
};

QJsonObject objectSchema(QJsonObject properties, QJsonArray required = {})
{
    QJsonObject schema{ { "type", "object" }, { "properties", properties }, { "additionalProperties", false } };
    if (!required.isEmpty())
        schema.insert("required", required);
    return schema;
}

QJsonObject stringProperty(const QString& description)
{
    return { { "type", "string" }, { "description", description } };
}

}  // namespace

McpServer::McpServer(QObject* parent) : QObject(parent), m_input(stdin, QIODevice::ReadOnly) {}

void McpServer::start()
{
#ifdef Q_OS_WIN
    auto notifier = new QWinEventNotifier(GetStdHandle(STD_INPUT_HANDLE), this);
    connect(notifier, &QWinEventNotifier::activated, this, &McpServer::readMessage);
#else
    auto notifier = new QSocketNotifier(STDIN_FILENO, QSocketNotifier::Read, this);
    connect(notifier, &QSocketNotifier::activated, this, &McpServer::readMessage);
#endif
    m_notifier = notifier;
}

void McpServer::readMessage()
{
    const auto line = m_input.readLine();
    if (line.isNull()) {
        QCoreApplication::quit();
        return;
    }
    if (!m_input.atEnd())
        QTimer::singleShot(0, this, &McpServer::readMessage);
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(line.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        writeError(QJsonValue(), -32700, QStringLiteral("Parse error"));
        return;
    }
    handleMessage(document.object());
}

void McpServer::handleMessage(const QJsonObject& request)
{
    const auto id = request.value("id");
    const auto method = request.value("method").toString();
    const auto parameters = request.value("params").toObject();
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
        if (m_activeService)
            m_activeService->cancelCurrent();
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
        if (m_activeService) {
            writeError(id, -32000, QStringLiteral("Another launcher operation is already running."));
            return;
        }
        const auto name = parameters.value("name").toString();
        const auto arguments = parameters.value("arguments").toObject();
        static const QMap<QString, QString> operations{
            { "lunalauncher_instance_list", "instance.list" },     { "lunalauncher_account_list", "account.list" },
            { "lunalauncher_account_login", "account.login" },     { "lunalauncher_instance_import", "instance.import" },
            { "lunalauncher_instance_launch", "instance.launch" }, { "lunalauncher_settings_list", "settings.list" },
            { "lunalauncher_settings_get", "settings.get" },       { "lunalauncher_settings_set", "settings.set" },
            { "lunalauncher_settings_reset", "settings.reset" }
        };
        if (!operations.contains(name)) {
            writeError(id, -32602, QStringLiteral("Unknown tool: %1").arg(name));
            return;
        }
        const auto progressToken = parameters.value("_meta").toObject().value("progressToken");
        McpInteraction interaction(arguments, [this, progressToken, progress = 0](const QJsonObject& event) mutable {
            const auto message = QString::fromUtf8(QJsonDocument(event).toJson(QJsonDocument::Compact));
            if (!progressToken.isUndefined()) {
                const QJsonObject params{ { "progressToken", progressToken }, { "progress", ++progress }, { "message", message } };
                writeMessage(QJsonObject{ { "jsonrpc", "2.0" }, { "method", "notifications/progress" }, { "params", params } });
            } else {
                const QJsonObject params{ { "level", "info" }, { "logger", "lunalauncher" }, { "data", event } };
                writeMessage(QJsonObject{ { "jsonrpc", "2.0" }, { "method", "notifications/message" }, { "params", params } });
            }
        });
        OperationService service;
        m_activeService = &service;
        const auto result = service.execute(operations.value(name), arguments, interaction);
        m_activeService = nullptr;
        const auto text = QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact));
        writeResult(id, QJsonObject{ { "content", QJsonArray{ QJsonObject{ { "type", "text" }, { "text", text } } } },
                                     { "structuredContent", result },
                                     { "isError", !result.value("ok").toBool() } });
        return;
    }
    if (!id.isUndefined())
        writeError(id, -32601, QStringLiteral("Method not found"));
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
    return {
        QJsonObject{ { "name", "lunalauncher_instance_list" },
                     { "description", "List installed Minecraft instances." },
                     { "inputSchema", objectSchema({}) } },
        QJsonObject{ { "name", "lunalauncher_account_list" },
                     { "description", "List launcher accounts without exposing tokens." },
                     { "inputSchema", objectSchema({}) } },
        QJsonObject{
            { "name", "lunalauncher_account_login" },
            { "description", "Add a Microsoft, offline, Yggdrasil, or UnifiedPass account." },
            { "inputSchema",
              objectSchema(
                  QJsonObject{ { "type", QJsonObject{ { "type", "string" },
                                                      { "enum", QJsonArray{ "microsoft", "offline", "yggdrasil", "unified-pass" } } } },
                               { "username", stringProperty("Account username; omitted for Microsoft device code login.") },
                               { "password", stringProperty("Password for the third-party authentication request.") },
                               { "authUrl", stringProperty("Yggdrasil authentication server URL.") },
                               { "sessionUrl", stringProperty("Yggdrasil session server URL.") },
                               { "sourceName", stringProperty("Display name for the Yggdrasil service.") },
                               { "serverId", stringProperty("UnifiedPass server ID.") },
                               { "profileId", stringProperty("Profile ID to select when the account has multiple profiles.") },
                               { "minecraftProfileName", stringProperty("Name to create when a Microsoft account has no Java profile.") } },

                  QJsonArray{ "type" }) } },
        QJsonObject{ { "name", "lunalauncher_instance_import" },
                     { "description", "Import a local pack, direct URL, CurseForge page URL, or Modrinth page URL." },
                     { "inputSchema", objectSchema(QJsonObject{ { "source", stringProperty("Local path or URL.") },
                                                                { "name", stringProperty("Optional instance name override.") } },
                                                   QJsonArray{ "source" }) } },
        QJsonObject{
            { "name", "lunalauncher_instance_launch" },
            { "description", "Launch an instance headlessly; detaches by default or waits for game exit when wait is true." },
            { "inputSchema", objectSchema(QJsonObject{ { "instance", stringProperty("Instance ID or managed name.") },
                                                       { "profile", stringProperty("Account profile name.") },
                                                       { "username", stringProperty("Username for third-party reauthentication.") },
                                                       { "password", stringProperty("Password for third-party reauthentication.") },
                                                       { "profileId", stringProperty("Profile ID for third-party reauthentication.") },
                                                       { "offlineName", stringProperty("Offline player name.") },
                                                       { "server", stringProperty("Server address to join.") },
                                                       { "world", stringProperty("World to open.") },
                                                       { "wait", QJsonObject{ { "type", "boolean" }, { "default", false } } } },
                                          QJsonArray{ "instance" }) } },
        QJsonObject{
            { "name", "lunalauncher_settings_list" },
            { "description", "List every registered launcher or instance setting. Sensitive values are redacted unless reveal is true." },
            { "inputSchema",
              objectSchema(QJsonObject{ { "scope", QJsonObject{ { "type", "string" }, { "enum", QJsonArray{ "launcher", "instance" } } } },
                                        { "instance", stringProperty("Instance ID or name; required for instance scope.") },
                                        { "filter", stringProperty("Optional case-insensitive setting name filter.") },
                                        { "reveal", QJsonObject{ { "type", "boolean" }, { "default", false } } } },
                           QJsonArray{ "scope" }) } },
        QJsonObject{
            { "name", "lunalauncher_settings_get" },
            { "description", "Read one registered launcher or instance setting." },
            { "inputSchema",
              objectSchema(QJsonObject{ { "scope", QJsonObject{ { "type", "string" }, { "enum", QJsonArray{ "launcher", "instance" } } } },
                                        { "instance", stringProperty("Instance ID or name; required for instance scope.") },
                                        { "key", stringProperty("Canonical setting ID.") },
                                        { "reveal", QJsonObject{ { "type", "boolean" }, { "default", false } } } },
                           QJsonArray{ "scope", "key" }) } },
        QJsonObject{
            { "name", "lunalauncher_settings_set" },
            { "description", "Set one registered launcher or instance setting using its existing value type." },
            { "inputSchema",
              objectSchema(QJsonObject{ { "scope", QJsonObject{ { "type", "string" }, { "enum", QJsonArray{ "launcher", "instance" } } } },
                                        { "instance", stringProperty("Instance ID or name; required for instance scope.") },
                                        { "key", stringProperty("Canonical setting ID.") },
                                        { "value", QJsonObject{ { "description", "JSON value to store." } } },
                                        { "reveal", QJsonObject{ { "type", "boolean" }, { "default", false } } } },
                           QJsonArray{ "scope", "key", "value" }) } },
        QJsonObject{
            { "name", "lunalauncher_settings_reset" },
            { "description", "Reset one registered launcher or instance setting to its default or inherited value." },
            { "inputSchema",
              objectSchema(QJsonObject{ { "scope", QJsonObject{ { "type", "string" }, { "enum", QJsonArray{ "launcher", "instance" } } } },
                                        { "instance", stringProperty("Instance ID or name; required for instance scope.") },
                                        { "key", stringProperty("Canonical setting ID.") },
                                        { "reveal", QJsonObject{ { "type", "boolean" }, { "default", false } } } },
                           QJsonArray{ "scope", "key" }) } }
    };
}
