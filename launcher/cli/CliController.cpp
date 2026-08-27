// SPDX-License-Identifier: GPL-3.0-only

#include "CliController.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QTimer>

#include <atomic>
#include <csignal>

#include "Application.h"
#include "cli/CliInteraction.h"
#include "cli/OperationService.h"

namespace {
std::atomic_bool interrupted = false;

void handleInterrupt(int)
{
    interrupted.store(true);
}

QJsonValue parseCliValue(const QString& value)
{
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(("[" + value + "]").toUtf8(), &error);
    if (error.error == QJsonParseError::NoError && document.isArray() && document.array().size() == 1)
        return document.array().first();
    return value;
}
}  // namespace

CliController::CliController(CliOptions options, QObject* parent) : QObject(parent), m_options(std::move(options)) {}

void CliController::run()
{
    QString operation;
    QJsonObject parameters;
    const auto command = m_options.command;
    const auto parseSettingsTarget = [&command](QJsonObject* target, int* next) {
        if (command.size() < 3)
            return false;
        const auto scope = command.at(2).toLower();
        target->insert("scope", scope);
        if (scope == "launcher" || scope == "global") {
            *next = 3;
            return true;
        }
        if (scope == "instance" && command.size() >= 4) {
            target->insert("instance", command.at(3));
            *next = 4;
            return true;
        }
        return false;
    };
    if (command.size() >= 2 && command.at(0) == "instance" && command.at(1) == "list") {
        operation = "instance.list";
    } else if (command.size() >= 2 && command.at(0) == "account" && command.at(1) == "list") {
        operation = "account.list";
    } else if (command.size() >= 3 && command.at(0) == "account" && command.at(1) == "login") {
        operation = "account.login";
        parameters = { { "type", command.at(2) },
                       { "username", m_options.username },
                       { "authUrl", m_options.authUrl },
                       { "sessionUrl", m_options.sessionUrl },
                       { "sourceName", m_options.sourceName },
                       { "serverId", m_options.serverId },
                       { "profileId", m_options.profileId },
                       { "minecraftProfileName", m_options.minecraftProfileName } };
    } else if (command.size() >= 3 && command.at(0) == "account" && command.at(1) == "default") {
        operation = "account.set-default";
        parameters = { { "account", command.at(2) } };
    } else if (command.size() >= 3 && command.at(0) == "account" && command.at(1) == "remove") {
        operation = "account.remove";
        parameters = { { "account", command.at(2) }, { "confirm", m_options.yes } };
    } else if (command.size() >= 3 && command.at(0) == "account" && command.at(1) == "refresh") {
        operation = "account.refresh";
        parameters = { { "account", command.at(2) } };
    } else if (command.size() >= 3 && command.at(0) == "instance" && command.at(1) == "info") {
        operation = "instance.info";
        parameters = { { "instance", command.at(2) } };
    } else if (command.size() >= 4 && command.at(0) == "instance" && command.at(1) == "rename") {
        operation = "instance.rename";
        parameters = { { "instance", command.at(2) }, { "name", command.at(3) } };
    } else if (command.size() >= 3 && command.at(0) == "instance" && command.at(1) == "group") {
        operation = "instance.group";
        parameters = { { "instance", command.at(2) }, { "group", command.size() >= 4 ? command.at(3) : QString("-") } };
    } else if (command.size() >= 4 && command.at(0) == "instance" && command.at(1) == "copy") {
        operation = "instance.copy";
        parameters = { { "instance", command.at(2) }, { "name", command.at(3) } };
    } else if (command.size() >= 3 && command.at(0) == "instance" && command.at(1) == "update") {
        operation = "instance.update";
        parameters = { { "instance", command.at(2) } };
    } else if (command.size() >= 3 && command.at(0) == "instance" && command.at(1) == "delete") {
        operation = "instance.delete";
        parameters = {
            { "instance", command.at(2) }, { "confirm", m_options.yes }, { "permanent", m_options.permanent }, { "force", m_options.force }
        };
    } else if (command.size() >= 2 && command.at(0) == "instance" && command.at(1) == "undo-delete") {
        operation = "instance.undo-delete";
    } else if (command.size() >= 4 && command.at(0) == "resource" && command.at(1) == "list") {
        operation = "resource.list";
        parameters = { { "instance", command.at(2) }, { "kind", command.at(3) } };
    } else if (command.size() >= 5 && command.at(0) == "resource" && command.at(1) == "install") {
        operation = "resource.install";
        parameters = { { "instance", command.at(2) }, { "kind", command.at(3) }, { "source", command.at(4) } };
    } else if (command.size() >= 5 && command.at(0) == "resource" &&
               (command.at(1) == "enable" || command.at(1) == "disable" || command.at(1) == "remove")) {
        operation = "resource." + command.at(1);
        parameters = {
            { "instance", command.at(2) }, { "kind", command.at(3) }, { "resource", command.at(4) }, { "confirm", m_options.yes }
        };
    } else if (command.size() >= 2 && command.at(0) == "java" && command.at(1) == "list") {
        operation = "java.list";
    } else if (command.size() >= 2 && command.at(0) == "import") {
        operation = "instance.import";
        parameters = { { "source", command.at(1) }, { "name", m_options.name } };
    } else if (command.size() >= 2 && command.at(0) == "launch") {
        operation = "instance.launch";
        parameters = { { "instance", command.at(1) },
                       { "profile", m_options.profile },
                       { "offlineName", m_options.offlineName },
                       { "server", m_options.server },
                       { "world", m_options.world },
                       { "username", m_options.username },
                       { "profileId", m_options.profileId },
                       { "wait", m_options.wait && !m_options.detach } };
    } else if (command.size() >= 2 && command.at(0) == "settings") {
        const auto action = command.at(1).toLower();
        int next = 0;
        if (parseSettingsTarget(&parameters, &next)) {
            parameters.insert("reveal", m_options.revealSecrets);
            if (action == "list") {
                operation = "settings.list";
                if (command.size() > next)
                    parameters.insert("filter", command.at(next));
            } else if ((action == "get" || action == "reset") && command.size() > next) {
                operation = "settings." + action;
                parameters.insert("key", command.at(next));
            } else if (action == "set" && command.size() > next) {
                operation = "settings.set";
                parameters.insert("key", command.at(next));
                if (command.size() > next + 1) {
                    parameters.insert("value", parseCliValue(command.at(next + 1)));
                    parameters.insert("valueFromCommandLine", true);
                }
            }
        }
        if (operation.isEmpty()) {
            QTextStream(stderr) << "Invalid settings command.\n";
            QCoreApplication::exit(2);
            return;
        }
    } else {
        QTextStream(stderr) << "Usage:\n"
                               "  --cli instance list [--json]\n"
                               "  --cli account list [--json]\n"
                               "  --cli account login microsoft\n"
                               "  --cli account login offline --username NAME\n"
                               "  --cli account login yggdrasil --username USER --auth-url URL --session-url URL\n"
                               "  --cli account login unified-pass --username USER --server-id ID\n"
                               "  --cli account default ACCOUNT|-\n"
                               "  --cli account refresh ACCOUNT\n"
                               "  --cli account remove ACCOUNT --yes\n"
                               "  --cli instance info INSTANCE\n"
                               "  --cli instance rename INSTANCE NAME\n"
                               "  --cli instance group INSTANCE [GROUP]\n"
                               "  --cli instance copy INSTANCE NAME\n"
                               "  --cli instance update INSTANCE\n"
                               "  --cli instance delete INSTANCE --yes [--permanent] [--force]\n"
                               "  --cli instance undo-delete\n"
                               "  --cli import SOURCE [--name NAME]\n"
                               "  --cli launch INSTANCE [--profile NAME | --offline NAME] [--wait]\n"
                               "  --cli resource list INSTANCE KIND\n"
                               "  --cli resource install INSTANCE KIND SOURCE\n"
                               "  --cli resource enable|disable INSTANCE KIND RESOURCE\n"
                               "  --cli resource remove INSTANCE KIND RESOURCE --yes\n"
                               "  --cli java list\n"
                               "  --cli settings list SCOPE [INSTANCE] [FILTER]\n"
                               "  --cli settings get SCOPE [INSTANCE] KEY [--reveal-secrets]\n"
                               "  --cli settings set SCOPE [INSTANCE] KEY [VALUE]\n"
                               "  --cli settings reset SCOPE [INSTANCE] KEY\n"
                               "    SCOPE is launcher or instance.\n"
                               "  --mcp\n";
        QCoreApplication::exit(2);
        return;
    }

    CliInteraction interaction(m_options.nonInteractive, m_options.passwordStdin);
    OperationService service;
    interrupted.store(false);
    const auto previousHandler = std::signal(SIGINT, handleInterrupt);
    QTimer interruptTimer;
    connect(&interruptTimer, &QTimer::timeout, &service, [&service] {
        if (interrupted.exchange(false))
            service.cancelCurrent();
    });
    interruptTimer.start(100);
    const auto result = service.execute(operation, parameters, interaction);
    interruptTimer.stop();
    std::signal(SIGINT, previousHandler);
    if (m_options.json) {
        QTextStream(stdout) << QJsonDocument(result).toJson(QJsonDocument::Compact) << Qt::endl;
    } else {
        printHuman(operation, result);
    }
    const int exitCode = result.value("exitCode").toInt(result.value("ok").toBool() ? 0 : 1);
    QCoreApplication::exit(exitCode);
}

void CliController::printHuman(const QString& operation, const QJsonObject& result)
{
    if (!result.value("ok").toBool()) {
        QTextStream(stderr) << "Error: " << result.value("error").toString() << Qt::endl;
        return;
    }

    auto out = QTextStream(stdout);
    const auto data = result.value("data");
    if (operation == "instance.list") {
        for (const auto& value : data.toArray()) {
            const auto instance = value.toObject();
            out << instance.value("id").toString() << '\t' << instance.value("name").toString();
            if (instance.value("running").toBool())
                out << "\trunning";
            out << Qt::endl;
        }
    } else if (operation == "account.list") {
        for (const auto& value : data.toArray()) {
            const auto account = value.toObject();
            out << account.value("profileName").toString() << '\t' << account.value("type").toString() << '\t'
                << account.value("state").toString();
            if (account.value("default").toBool())
                out << "\tdefault";
            out << Qt::endl;
        }
    } else if (operation == "settings.list") {
        for (const auto& value : data.toArray()) {
            const auto setting = value.toObject();
            const auto encoded = QJsonDocument(QJsonArray{ setting.value("value") }).toJson(QJsonDocument::Compact);
            out << setting.value("key").toString() << '\t' << setting.value("type").toString() << '\t'
                << QString::fromUtf8(encoded.mid(1, encoded.size() - 2)) << Qt::endl;
        }
    } else if (data.isArray()) {
        for (const auto& value : data.toArray())
            out << QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact) << Qt::endl;
    } else {
        out << QJsonDocument(data.toObject()).toJson(QJsonDocument::Compact) << Qt::endl;
    }
}
