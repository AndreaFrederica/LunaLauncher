// SPDX-License-Identifier: GPL-3.0-only

#include "LauncherApi.h"

#include <algorithm>
#include <utility>

#include <QList>
#include <QStringList>

#include "cli/OperationService.h"

namespace {

QList<ApiOperation> legacyOperations()
{
    return { { "instance.list", "List installed instances." },
             { "instance.info", "Read an installed instance." },
             { "instance.rename", "Rename an installed instance." },
             { "instance.group", "Move an instance to a group." },
             { "instance.copy", "Copy an installed instance." },
             { "instance.update", "Run an instance update." },
             { "instance.delete", "Trash or permanently delete an instance.", {}, {}, true },
             { "instance.undo-delete", "Restore the most recently trashed instance." },
             { "account.list", "List launcher accounts." },
             { "account.login", "Add a launcher account." },
             { "account.set-default", "Set or clear the default account." },
             { "account.refresh", "Refresh an account." },
             { "account.remove", "Remove an account.", {}, {}, true },
             { "instance.import", "Import an instance pack." },
             { "instance.launch", "Launch an instance." },
             { "resource.list", "List installed resources." },
             { "resource.install", "Install a resource from a path or direct URL." },
             { "resource.enable", "Enable an installed resource." },
             { "resource.disable", "Disable an installed resource." },
             { "resource.remove", "Remove an installed resource.", {}, {}, true },
             { "java.list", "List available Java installations." },
             { "settings.list", "List registered settings." },
             { "settings.get", "Read a registered setting." },
             { "settings.set", "Set a registered setting." },
             { "settings.reset", "Reset a registered setting." } };
}

}  // namespace

LauncherApi::LauncherApi(QObject* parent) : QObject(parent), m_legacyService(new OperationService(this))
{
    for (const auto& operation : legacyOperations()) {
        const auto name = operation.name;
        registerOperation(operation, [this, name](const QJsonObject& parameters, UserInteraction& interaction) {
            return m_legacyService->execute(name, parameters, interaction);
        });
    }
}

LauncherApi::~LauncherApi() = default;

QJsonObject LauncherApi::execute(const QString& operation, const QJsonObject& parameters, UserInteraction& interaction)
{
    const auto it = m_operations.constFind(operation);
    if (it == m_operations.constEnd())
        return OperationService::failure(QStringLiteral("Unknown API operation: %1").arg(operation), 2);
    return it->handler(parameters, interaction);
}

QJsonArray LauncherApi::describe() const
{
    QStringList names = m_operations.keys();
    std::sort(names.begin(), names.end());

    QJsonArray result;
    for (const auto& name : names)
        result.append(m_operations.value(name).metadata.toJson());
    return result;
}

void LauncherApi::registerOperation(ApiOperation operation, Handler handler)
{
    if (operation.name.isEmpty() || !handler)
        return;
    m_operations.insert(operation.name, { std::move(operation), std::move(handler) });
}

void LauncherApi::cancelCurrent()
{
    if (m_legacyService)
        m_legacyService->cancelCurrent();
}
