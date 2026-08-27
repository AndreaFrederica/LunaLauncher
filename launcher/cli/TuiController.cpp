// SPDX-License-Identifier: GPL-3.0-only

#include "TuiController.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QTimer>

#include <atomic>
#include <csignal>

#include "cli/OperationService.h"

namespace {
std::atomic_bool interrupted = false;

void handleInterrupt(int)
{
    interrupted.store(true);
}

QJsonArray choicesFromItems(const QJsonArray& items, const QString& nameKey, const QString& idKey, const QString& detailKey = {})
{
    QJsonArray choices;
    for (const auto& value : items) {
        const auto item = value.toObject();
        auto name = item.value(nameKey).toString();
        const auto detail = item.value(detailKey).toString();
        if (!detail.isEmpty())
            name += QString(" (%1)").arg(detail);
        choices.append(QJsonObject{ { "name", name }, { "id", item.value(idKey).toString() } });
    }
    return choices;
}

QJsonValue parseTerminalValue(const QString& value)
{
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(("[" + value + "]").toUtf8(), &error);
    if (error.error == QJsonParseError::NoError && document.isArray() && document.array().size() == 1)
        return document.array().first();
    return value;
}

QString jsonValueText(const QJsonValue& value)
{
    const auto encoded = QJsonDocument(QJsonArray{ value }).toJson(QJsonDocument::Compact);
    return QString::fromUtf8(encoded.mid(1, encoded.size() - 2));
}
}  // namespace

TuiController::TuiController(QObject* parent) : QObject(parent) {}

void TuiController::run()
{
    OperationService service;
    interrupted.store(false);
    const auto previousHandler = std::signal(SIGINT, handleInterrupt);
    QTimer interruptTimer;
    connect(&interruptTimer, &QTimer::timeout, &service, [&service] {
        if (interrupted.exchange(false))
            service.cancelCurrent();
    });
    interruptTimer.start(100);

    auto out = QTextStream(stdout);
    bool running = true;
    while (running) {
        out << "\nLuna Launcher TUI\n"
               "=================\n"
               "  1. Instances\n"
               "  2. Accounts\n"
               "  3. Log in\n"
               "  4. Import pack\n"
               "  5. Launch instance\n"
               "  6. Launcher settings\n"
               "  7. Instance settings\n"
               "  0. Exit\n"
            << Qt::flush;
        const auto choice = m_interaction.input(tr("Choice"), false);
        if (!choice)
            break;

        bool pause = true;
        if (*choice == "1")
            pause = showInstances(service);
        else if (*choice == "2")
            pause = showAccounts(service);
        else if (*choice == "3")
            pause = loginAccount(service);
        else if (*choice == "4")
            pause = importInstance(service);
        else if (*choice == "5")
            pause = launchInstance(service);
        else if (*choice == "6")
            pause = manageSettings(service, false);
        else if (*choice == "7")
            pause = manageSettings(service, true);
        else if (*choice == "0" || choice->compare("q", Qt::CaseInsensitive) == 0)
            running = false;
        else
            out << "Unknown choice.\n";

        if (running && pause)
            waitForEnter();
    }

    interruptTimer.stop();
    std::signal(SIGINT, previousHandler);
    QCoreApplication::exit(0);
}

bool TuiController::showInstances(OperationService& service)
{
    return printResult("instance.list", service.execute("instance.list", {}, m_interaction));
}

bool TuiController::showAccounts(OperationService& service)
{
    return printResult("account.list", service.execute("account.list", {}, m_interaction));
}

bool TuiController::loginAccount(OperationService& service)
{
    const QJsonArray types{ QJsonObject{ { "name", "Microsoft" }, { "id", "microsoft" } },
                            QJsonObject{ { "name", "Offline" }, { "id", "offline" } },
                            QJsonObject{ { "name", "Yggdrasil" }, { "id", "yggdrasil" } },
                            QJsonObject{ { "name", "UnifiedPass" }, { "id", "unified-pass" } } };
    const auto selected = m_interaction.select(tr("Account type"), types);
    if (!selected)
        return false;

    QJsonObject parameters{ { "type", types.at(*selected).toObject().value("id") } };
    const auto type = parameters.value("type").toString();
    if (type != "microsoft") {
        const auto username = m_interaction.input(tr("Username"), false);
        if (!username || username->trimmed().isEmpty())
            return false;
        parameters.insert("username", username->trimmed());
    }
    if (type == "yggdrasil") {
        const auto authUrl = m_interaction.input(tr("Authentication server URL"), false);
        const auto sessionUrl = m_interaction.input(tr("Session server URL"), false);
        if (!authUrl || !sessionUrl || authUrl->trimmed().isEmpty() || sessionUrl->trimmed().isEmpty())
            return false;
        parameters.insert("authUrl", authUrl->trimmed());
        parameters.insert("sessionUrl", sessionUrl->trimmed());
        const auto sourceName = m_interaction.input(tr("Service name (optional)"), false);
        if (sourceName)
            parameters.insert("sourceName", sourceName->trimmed());
    } else if (type == "unified-pass") {
        const auto serverId = m_interaction.input(tr("Server ID"), false);
        if (!serverId || serverId->trimmed().isEmpty())
            return false;
        parameters.insert("serverId", serverId->trimmed());
    }

    return printResult("account.login", service.execute("account.login", parameters, m_interaction));
}

bool TuiController::importInstance(OperationService& service)
{
    const auto source = m_interaction.input(tr("Local pack path or CurseForge/Modrinth URL"), false);
    if (!source || source->trimmed().isEmpty())
        return false;
    const auto name = m_interaction.input(tr("Instance name override (optional)"), false);
    const QJsonObject parameters{ { "source", source->trimmed() }, { "name", name ? name->trimmed() : QString() } };
    return printResult("instance.import", service.execute("instance.import", parameters, m_interaction));
}

bool TuiController::launchInstance(OperationService& service)
{
    const auto instancesResult = service.execute("instance.list", {}, m_interaction);
    if (!instancesResult.value("ok").toBool())
        return printResult("instance.list", instancesResult);
    const auto instances = instancesResult.value("data").toArray();
    if (instances.isEmpty()) {
        QTextStream(stdout) << "No instances are installed.\n";
        return true;
    }
    const auto instanceChoices = choicesFromItems(instances, "name", "id", "id");
    const auto selectedInstance = m_interaction.select(tr("Instance"), instanceChoices);
    if (!selectedInstance)
        return false;

    QJsonObject parameters{ { "instance", instanceChoices.at(*selectedInstance).toObject().value("id") } };
    const QJsonArray accountModes{ QJsonObject{ { "name", "Default account" }, { "id", "default" } },
                                   QJsonObject{ { "name", "Select account" }, { "id", "select" } },
                                   QJsonObject{ { "name", "Offline player" }, { "id", "offline" } } };
    const auto selectedMode = m_interaction.select(tr("Launch account"), accountModes);
    if (!selectedMode)
        return false;
    const auto mode = accountModes.at(*selectedMode).toObject().value("id").toString();
    if (mode == "select") {
        const auto accountsResult = service.execute("account.list", {}, m_interaction);
        if (!accountsResult.value("ok").toBool())
            return printResult("account.list", accountsResult);
        const auto accounts = accountsResult.value("data").toArray();
        if (accounts.isEmpty()) {
            QTextStream(stdout) << "No accounts are configured.\n";
            return true;
        }
        const auto accountChoices = choicesFromItems(accounts, "profileName", "id", "type");
        const auto selectedAccount = m_interaction.select(tr("Account"), accountChoices);
        if (!selectedAccount)
            return false;
        parameters.insert("profile", accounts.at(*selectedAccount).toObject().value("profileName"));
    } else if (mode == "offline") {
        const auto offlineName = m_interaction.input(tr("Offline player name"), false);
        if (!offlineName || offlineName->trimmed().isEmpty())
            return false;
        parameters.insert("offlineName", offlineName->trimmed());
    }

    const QJsonArray targetModes{ QJsonObject{ { "name", "No direct target" }, { "id", "none" } },
                                  QJsonObject{ { "name", "Join server" }, { "id", "server" } },
                                  QJsonObject{ { "name", "Open world" }, { "id", "world" } } };
    const auto selectedTarget = m_interaction.select(tr("Launch target"), targetModes);
    if (!selectedTarget)
        return false;
    const auto target = targetModes.at(*selectedTarget).toObject().value("id").toString();
    if (target != "none") {
        const auto value = m_interaction.input(target == "server" ? tr("Server address") : tr("World path"), false);
        if (!value || value->trimmed().isEmpty())
            return false;
        parameters.insert(target, value->trimmed());
    }
    parameters.insert("wait", confirm(tr("Wait for the game to exit")));
    return printResult("instance.launch", service.execute("instance.launch", parameters, m_interaction));
}

bool TuiController::manageSettings(OperationService& service, bool instanceScope)
{
    QJsonObject target{ { "scope", instanceScope ? "instance" : "launcher" } };
    if (instanceScope) {
        const auto instancesResult = service.execute("instance.list", {}, m_interaction);
        if (!instancesResult.value("ok").toBool())
            return printResult("instance.list", instancesResult);
        const auto instances = instancesResult.value("data").toArray();
        if (instances.isEmpty()) {
            QTextStream(stdout) << "No instances are installed.\n";
            return true;
        }
        const auto choices = choicesFromItems(instances, "name", "id", "id");
        const auto selected = m_interaction.select(tr("Instance"), choices);
        if (!selected)
            return false;
        target.insert("instance", choices.at(*selected).toObject().value("id"));
    }

    while (true) {
        const auto filter = m_interaction.input(tr("Setting name or filter (empty to return)"), false);
        if (!filter || filter->trimmed().isEmpty())
            return false;
        auto listParameters = target;
        listParameters.insert("filter", filter->trimmed());
        const auto result = service.execute("settings.list", listParameters, m_interaction);
        if (!result.value("ok").toBool())
            return printResult("settings.list", result);
        const auto settings = result.value("data").toArray();
        if (settings.isEmpty()) {
            QTextStream(stdout) << "No matching settings.\n";
            continue;
        }

        int selectedIndex = -1;
        for (int i = 0; i < settings.size(); ++i) {
            if (settings.at(i).toObject().value("key").toString().compare(filter->trimmed(), Qt::CaseInsensitive) == 0) {
                selectedIndex = i;
                break;
            }
        }
        if (selectedIndex < 0 && settings.size() > 1) {
            const auto choices = choicesFromItems(settings, "key", "key", "type");
            const auto selected = m_interaction.select(tr("Setting"), choices);
            if (!selected)
                continue;
            selectedIndex = *selected;
        }
        if (selectedIndex < 0)
            selectedIndex = 0;

        const auto setting = settings.at(selectedIndex).toObject();
        auto out = QTextStream(stdout);
        out << setting.value("key").toString() << " = " << jsonValueText(setting.value("value")) << "  ["
            << setting.value("type").toString() << "]\nDefault: " << jsonValueText(setting.value("default")) << Qt::endl;
        const QJsonArray actions{ QJsonObject{ { "name", "Set value" }, { "id", "set" } },
                                  QJsonObject{ { "name", "Reset to default/inherited value" }, { "id", "reset" } },
                                  QJsonObject{ { "name", "Back" }, { "id", "back" } } };
        const auto selectedAction = m_interaction.select(tr("Action"), actions);
        if (!selectedAction || *selectedAction == 2)
            continue;

        auto parameters = target;
        parameters.insert("key", setting.value("key"));
        QString operation;
        if (*selectedAction == 0) {
            const auto value = m_interaction.input(tr("New value"), setting.value("sensitive").toBool());
            if (!value)
                continue;
            operation = "settings.set";
            parameters.insert("value", parseTerminalValue(*value));
        } else {
            operation = "settings.reset";
        }
        printResult(operation, service.execute(operation, parameters, m_interaction));
        waitForEnter();
    }
}

bool TuiController::printResult(const QString& operation, const QJsonObject& result)
{
    auto out = QTextStream(stdout);
    if (!result.value("ok").toBool()) {
        out << "Error: " << result.value("error").toString() << Qt::endl;
        return true;
    }

    const auto data = result.value("data");
    if (operation == "instance.list") {
        out << "\nInstances\n---------\n";
        for (const auto& value : data.toArray()) {
            const auto instance = value.toObject();
            out << instance.value("name").toString() << "  [" << instance.value("id").toString() << ']';
            if (instance.value("running").toBool())
                out << "  running";
            out << Qt::endl;
        }
    } else if (operation == "account.list") {
        out << "\nAccounts\n--------\n";
        for (const auto& value : data.toArray()) {
            const auto account = value.toObject();
            out << account.value("profileName").toString() << "  " << account.value("type").toString() << "  "
                << account.value("state").toString();
            if (account.value("default").toBool())
                out << "  default";
            out << Qt::endl;
        }
    } else if (operation == "settings.list") {
        for (const auto& value : data.toArray()) {
            const auto setting = value.toObject();
            out << setting.value("key").toString() << " = " << jsonValueText(setting.value("value")) << "  ["
                << setting.value("type").toString() << "]" << Qt::endl;
        }
    } else {
        out << "Done: " << QJsonDocument(data.toObject()).toJson(QJsonDocument::Compact) << Qt::endl;
    }
    return true;
}

bool TuiController::confirm(const QString& prompt, bool defaultValue)
{
    const auto suffix = defaultValue ? " [Y/n]" : " [y/N]";
    const auto answer = m_interaction.input(prompt + suffix, false);
    if (!answer || answer->trimmed().isEmpty())
        return defaultValue;
    return answer->trimmed().compare("y", Qt::CaseInsensitive) == 0 || answer->trimmed().compare("yes", Qt::CaseInsensitive) == 0;
}

void TuiController::waitForEnter()
{
    m_interaction.input(tr("Press Enter to return to the menu"), false);
}
