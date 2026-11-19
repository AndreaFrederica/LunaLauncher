// SPDX-License-Identifier: MPL-2.0

#include "LauncherApiExports.h"
#include "InstanceExportTask.h"
#include "ExportSelection.h"

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
#include "cli/OperationService.h"
#include "tasks/Task.h"
#include "minecraft/MinecraftInstance.h"
#include "modplatform/helpers/ExportToModList.h"
#include "minecraft/mod/tasks/LocalModParseTask.h"
#include "api/LauncherApiSupport.h"

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
    if (format != "zip" && format != "modrinth" && format != "curseforge")
        return OperationService::failure("format must be zip, modrinth, or curseforge.", 2);

    auto instance = findInstance(reference);
    if (!instance)
        return OperationService::failure(QObject::tr("Instance not found: %1").arg(reference), 2);
    if (instance->isRunning())
        return OperationService::failure(QObject::tr("An instance cannot be exported while it is running."), 2);

    InstanceExportTask task(instance, parameters);
    QString error;
    if (!waitForTask(&task, interaction, &error, api)) return OperationService::failure(error);

    return OperationService::success(QJsonObject{ { "instance", instance->id() },
                                                  { "format", format },
                                                  { "path", QFileInfo(outputPath).absoluteFilePath() },
                                                  { "files", task.fileCount() },
                                                  { "changed", true } });
}

}  // namespace

void registerLauncherApiExportOperations(LauncherApi& api)
{
    api.registerOperation({ "resource.export-list", "Render selected installed mods as text, HTML, Markdown, JSON, CSV, or a custom template.",
        ApiSupport::schema({ { "instance", ApiSupport::string("Minecraft instance ID.") },
            { "format", ApiSupport::string("text, html, markdown, json, csv, or custom.") }, { "files", ApiSupport::strings() },
            { "template", ApiSupport::string("Custom line template with name/authors/url/version/filename placeholders.") },
            { "authors", ApiSupport::boolean() }, { "url", ApiSupport::boolean() }, { "version", ApiSupport::boolean() }, { "filename", ApiSupport::boolean() } }, { "instance", "format" }), "resources" },
        [](const QJsonObject& p, UserInteraction&) {
            auto instance = dynamic_cast<MinecraftInstance*>(findInstance(p.value("instance").toString()));
            if (!instance) return OperationService::failure("Minecraft instance not found.", 2);
            const QMap<QString, ExportToModList::Formats> formats{ { "text", ExportToModList::PLAINTXT }, { "html", ExportToModList::HTML },
                { "markdown", ExportToModList::MARKDOWN }, { "json", ExportToModList::JSON }, { "csv", ExportToModList::CSV }, { "custom", ExportToModList::CUSTOM } };
            const auto format = p.value("format").toString();
            if (!formats.contains(format)) return OperationService::failure("Unknown mod list format.", 2);
            QSet<QString> selected;
            for (const auto& file : p.value("files").toArray()) selected.insert(file.toString());
            auto remaining = selected;
            QList<Mod*> mods;
            std::vector<std::unique_ptr<Mod>> storage;
            for (const auto& path : { instance->modsRoot(), instance->coreModsDir(), instance->nilModsDir() }) {
                for (const auto& file : QDir(path).entryInfoList({ "*.jar", "*.zip", "*.litemod", "*.nilmod", "*.disabled" }, QDir::Files, QDir::Name)) {
                    if (file.isSymLink()) continue;
                    if (!selected.isEmpty() && !selected.contains(file.fileName())) continue;
                    remaining.remove(file.fileName());
                    auto mod = std::make_unique<Mod>(file);
                    ModUtils::process(*mod, ModUtils::ProcessingLevel::Full);
                    mods.append(mod.get()); storage.push_back(std::move(mod));
                }
            }
            if (!remaining.isEmpty()) return OperationService::failure("Some selected mods no longer exist.", 2);
            int flags = 0;
            if (p.value("authors").toBool()) flags |= ExportToModList::Authors;
            if (p.value("url").toBool()) flags |= ExportToModList::Url;
            if (p.value("version").toBool()) flags |= ExportToModList::Version;
            if (p.value("filename").toBool()) flags |= ExportToModList::FileName;
            const auto content = format == "custom" ? ExportToModList::exportToModList(mods, p.value("template").toString()) :
                ExportToModList::exportToModList(mods, formats.value(format), static_cast<ExportToModList::OptionalData>(flags));
            return OperationService::success(QJsonObject{ { "content", content }, { "format", format }, { "count", mods.size() } });
        });
    api.registerOperation({ "instance.export-files", "Browse selectable export files without following symbolic links.",
        ApiSupport::schema({ { "instance", ApiSupport::string("Instance ID.") }, { "path", ApiSupport::string("Instance-relative directory, default root.") } }, { "instance" }), "files" },
        [](const QJsonObject& p, UserInteraction&) {
            auto instance = findInstance(p.value("instance").toString());
            if (!instance) return OperationService::failure("Instance not found.", 2);
            const QDir root(instance->instanceRoot());
            const auto relative = QDir::cleanPath(QDir::fromNativeSeparators(p.value("path").toString(".")));
            if (QDir::isAbsolutePath(relative) || relative.contains(':') || relative.split('/').contains(".."))
                return OperationService::failure("Path must stay inside the instance.", 2);
            auto current = root.absolutePath();
            for (const auto& part : relative.split('/')) {
                if (part == ".") continue;
                current = QDir(current).filePath(part);
                if (QFileInfo(current).isSymLink()) return OperationService::failure("Symbolic links cannot be exported.", 2);
            }
            const QDir directory(current);
            if (!directory.exists()) return OperationService::failure("Directory not found.", 2);
            const auto prefix = ExportSelection::gamePrefix(root.absolutePath(), instance->gameRoot());
            QJsonArray entries;
            for (const auto& file : directory.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden, QDir::DirsFirst | QDir::Name)) {
                if (entries.size() >= 4096) return OperationService::failure("Directory exceeds 4096 entries.", 2);
                if (file.isSymLink()) continue;
                const auto path = QDir::fromNativeSeparators(root.relativeFilePath(file.absoluteFilePath()));
                entries.append(QJsonObject{ { "path", path }, { "name", file.fileName() }, { "isDir", file.isDir() },
                    { "category", ExportSelection::category(path, prefix) }, { "defaultSelected", ExportSelection::defaultSelected(path, prefix) } });
            }
            return OperationService::success(QJsonObject{ { "entries", entries }, { "gamePrefix", prefix } });
        });
    const auto instance = stringProperty("Instance ID, managed name, or display name.");
    api.registerOperation({ "instance.export", "Export a launcher ZIP, Modrinth pack, or CurseForge pack using existing export tasks.",
                            objectSchema({ { "instance", instance },
                                           { "output", stringProperty("Destination zip path.") },
                                           { "format", QJsonObject{ { "type", "string" }, { "default", "zip" } } },
                                           { "name", stringProperty("Pack display name.") }, { "version", stringProperty("Pack version; required for modrinth/curseforge.") },
                                           { "summary", stringProperty("Modrinth summary.") }, { "author", stringProperty("CurseForge author.") },
                                           { "optionalFiles", boolProperty("Mark optional resources.") },
                                           { "recommendedRAM", QJsonObject{ { "type", "integer" }, { "minimum", 0 }, { "maximum", 1048576 } } },
                                           { "exclude", ApiSupport::strings() },
                                           { "include", ApiSupport::strings() },
                                           { "fileSelection", QJsonObject{ { "type", "object" }, { "additionalProperties", QJsonObject{ { "type", "boolean" } } } } },
                                           { "items", ApiSupport::strings() },
                                           { "modMode", stringProperty("auto: platform links with bundled fallback; bundled: local mod files; links: require hash-verified platform matches.") },
                                           { "overwrite", boolProperty("Replace an existing output file.") },
                                           { "includeLogs", boolProperty("Include the game logs directory.") },
                                           { "includeCrashReports", boolProperty("Include crash reports.") },
                                           { "includeCaches", boolProperty("Include launcher and loader cache directories.") } },
                                          { "instance", "output" }), {}, true },
                           [&api](const QJsonObject& parameters, UserInteraction& interaction) {
                               return exportInstance(parameters, interaction, api);
                           });
}
