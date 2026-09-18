// SPDX-License-Identifier: GPL-3.0-only

#include "LauncherApiExports.h"

#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>

#include "Application.h"
#include "BaseInstance.h"
#include "InstanceList.h"
#include "MMCZip.h"
#include "api/LauncherApi.h"
#include "archive/ExportToZipTask.h"
#include "cli/OperationService.h"
#include "tasks/Task.h"

namespace {

QJsonObject stringProperty(const QString& description)
{
    return { { "type", "string" }, { "description", description } };
}

QJsonObject boolProperty(const QString& description, bool defaultValue = false)
{
    return { { "type", "boolean" }, { "description", description }, { "default", defaultValue } };
}

QJsonObject objectSchema(QJsonObject properties, QJsonArray required = {})
{
    QJsonObject schema{ { "type", "object" }, { "properties", properties }, { "additionalProperties", false } };
    if (!required.isEmpty())
        schema.insert("required", required);
    return schema;
}

BaseInstance* findInstance(const QString& reference)
{
    auto list = APPLICATION->instances();
    auto instance = list->getInstanceById(reference);
    if (!instance)
        instance = list->getInstanceByManagedName(reference);
    if (!instance) {
        for (int i = 0; i < list->count(); ++i) {
            if (list->at(i)->name().compare(reference, Qt::CaseInsensitive) == 0) {
                instance = list->at(i);
                break;
            }
        }
    }
    return instance;
}

bool waitForTask(Task* task, UserInteraction& interaction, QString* error, LauncherApi& api)
{
    if (!task) {
        if (error)
            *error = QObject::tr("The operation did not create a task.");
        return false;
    }

    QEventLoop loop;
    api.trackTask(task);
    QObject::connect(task, &Task::status, &loop, [&interaction](const QString& status) { interaction.status(status); });
    QObject::connect(task, &Task::finished, &loop, &QEventLoop::quit);
    if (!task->isFinished()) {
        if (!task->isRunning())
            task->start();
        if (!task->isFinished())
            loop.exec();
    }
    api.clearTrackedTask(task);
    if (!task->wasSuccessful() && error)
        *error = task->failReason().isEmpty() ? QObject::tr("The operation was aborted.") : task->failReason();
    return task->wasSuccessful();
}

QJsonObject exportInstance(const QJsonObject& parameters, UserInteraction& interaction, LauncherApi& api)
{
    const auto reference = parameters.value("instance").toString().trimmed();
    const auto outputPath = parameters.value("output").toString().trimmed();
    const auto format = parameters.value("format").toString("zip").trimmed().toLower();
    if (reference.isEmpty() || outputPath.isEmpty())
        return OperationService::failure(QObject::tr("instance and output are required."), 2);
    if (format != "zip")
        return OperationService::failure(QObject::tr("Only zip instance export is currently supported."), 3);

    auto instance = findInstance(reference);
    if (!instance)
        return OperationService::failure(QObject::tr("Instance not found: %1").arg(reference), 2);
    if (instance->isRunning())
        return OperationService::failure(QObject::tr("An instance cannot be exported while it is running."), 2);

    const QFileInfo outputInfo(outputPath);
    if (outputInfo.exists() && !parameters.value("overwrite").toBool(false))
        return OperationService::failure(QObject::tr("The output file already exists. Set overwrite=true to replace it."), 2);
    const auto parent = outputInfo.absoluteDir();
    if (!parent.exists() && !QDir().mkpath(parent.absolutePath()))
        return OperationService::failure(QObject::tr("Could not create the output directory."));

    const auto root = QDir(instance->instanceRoot());
    const auto gameRoot = QDir(instance->gameRoot());
    const auto gamePrefix = root.relativeFilePath(gameRoot.absolutePath());
    const auto includeLogs = parameters.value("includeLogs").toBool(false);
    const auto includeCrashReports = parameters.value("includeCrashReports").toBool(false);
    const auto includeCaches = parameters.value("includeCaches").toBool(false);

    QFileInfoList files;
    const auto excluded = [root, gamePrefix, includeLogs, includeCrashReports, includeCaches](const QFileInfo& file) {
        auto relative = QDir::fromNativeSeparators(root.relativeFilePath(file.absoluteFilePath()));
        const auto lower = relative.toLower();
        if (relative == ".packignore" || lower.endsWith("/.ds_store") || lower.endsWith("/thumbs.db") ||
            lower == ".ds_store" || lower == "thumbs.db")
            return true;
        const auto prefix = gamePrefix.isEmpty() ? QString() : QDir::fromNativeSeparators(gamePrefix) + "/";
        if (!prefix.isEmpty()) {
            if (!includeLogs && (lower == (prefix + "logs").toLower() || lower.startsWith((prefix + "logs/").toLower())))
                return true;
            if (!includeCrashReports &&
                (lower == (prefix + "crash-reports").toLower() || lower.startsWith((prefix + "crash-reports/").toLower())))
                return true;
            if (!includeCaches && (lower == (prefix + ".cache").toLower() || lower.startsWith((prefix + ".cache/").toLower()) ||
                                   lower == (prefix + ".fabric").toLower() || lower.startsWith((prefix + ".fabric/").toLower()) ||
                                   lower == (prefix + ".quilt").toLower() || lower.startsWith((prefix + ".quilt/").toLower())))
                return true;
        }
        return false;
    };
    if (!MMCZip::collectFileListRecursively(instance->instanceRoot(), nullptr, &files, excluded))
        return OperationService::failure(QObject::tr("Unable to enumerate instance files."));

    auto task = makeShared<MMCZip::ExportToZipTask>(outputPath, instance->instanceRoot(), files, "", true);
    QString error;
    if (!waitForTask(task.get(), interaction, &error, api))
        return OperationService::failure(error);

    return OperationService::success(QJsonObject{ { "instance", instance->id() },
                                                  { "format", format },
                                                  { "path", QFileInfo(outputPath).absoluteFilePath() },
                                                  { "files", files.size() },
                                                  { "changed", true } });
}

}  // namespace

void registerLauncherApiExportOperations(LauncherApi& api)
{
    const auto instance = stringProperty("Instance ID, managed name, or display name.");
    api.registerOperation({ "instance.export", "Export an instance directory as a zip archive.",
                            objectSchema({ { "instance", instance },
                                           { "output", stringProperty("Destination zip path.") },
                                           { "format", QJsonObject{ { "type", "string" }, { "default", "zip" } } },
                                           { "overwrite", boolProperty("Replace an existing output file.") },
                                           { "includeLogs", boolProperty("Include the game logs directory.") },
                                           { "includeCrashReports", boolProperty("Include crash reports.") },
                                           { "includeCaches", boolProperty("Include launcher and loader cache directories.") } },
                                          { "instance", "output" }), {}, true },
                           [&api](const QJsonObject& parameters, UserInteraction& interaction) {
                               return exportInstance(parameters, interaction, api);
                           });
}
