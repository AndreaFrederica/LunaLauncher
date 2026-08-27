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
}  // namespace

CliController::CliController(CliOptions options, QObject* parent) : QObject(parent), m_options(std::move(options)) {}

void CliController::run()
{
    QString operation;
    QJsonObject parameters;
    const auto command = m_options.command;
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
    } else {
        QTextStream(stderr) << "Usage:\n"
                               "  --cli instance list [--json]\n"
                               "  --cli account list [--json]\n"
                               "  --cli account login microsoft\n"
                               "  --cli account login offline --username NAME\n"
                               "  --cli account login yggdrasil --username USER --auth-url URL --session-url URL\n"
                               "  --cli account login unified-pass --username USER --server-id ID\n"
                               "  --cli import SOURCE [--name NAME]\n"
                               "  --cli launch INSTANCE [--profile NAME | --offline NAME] [--wait]\n"
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
    } else {
        out << QJsonDocument(data.toObject()).toJson(QJsonDocument::Compact) << Qt::endl;
    }
}
