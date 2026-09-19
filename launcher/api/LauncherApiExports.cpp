// SPDX-License-Identifier: GPL-3.0-only

#include "LauncherApiExports.h"

#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QSaveFile>
#include <QTemporaryDir>

#include "Application.h"
#include "BaseInstance.h"
#include "InstanceList.h"
#include "MMCZip.h"
#include "api/LauncherApi.h"
#include "archive/ExportToZipTask.h"
#include "cli/OperationService.h"
#include "tasks/Task.h"
#include "minecraft/MinecraftInstance.h"
#include "modplatform/modrinth/ModrinthPackExportTask.h"
#include "modplatform/flame/FlamePackExportTask.h"
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

    const QFileInfo outputInfo(outputPath);
    if (outputInfo.exists() && !parameters.value("overwrite").toBool(false))
        return OperationService::failure(QObject::tr("The output file already exists. Set overwrite=true to replace it."), 2);
    const auto parent = outputInfo.absoluteDir();
    if (!parent.exists() && !QDir().mkpath(parent.absolutePath()))
        return OperationService::failure(QObject::tr("Could not create the output directory."));

    const auto root = QDir(instance->instanceRoot());
    const auto outputRelative = root.relativeFilePath(outputInfo.absoluteFilePath());
    if (!outputRelative.startsWith("../") && !QDir::isAbsolutePath(outputRelative))
        return OperationService::failure("Export output must be outside the instance directory.", 2);
    QTemporaryDir staging(parent.filePath(".neo-export-XXXXXX"));
    if (!staging.isValid()) return OperationService::failure("Could not stage export.");
    const auto stagedOutput = staging.filePath("pack.zip");
    const auto gameRoot = QDir(instance->gameRoot());
    const auto gamePrefix = root.relativeFilePath(gameRoot.absolutePath());
    const auto includeLogs = parameters.value("includeLogs").toBool(false);
    const auto includeCrashReports = parameters.value("includeCrashReports").toBool(false);
    const auto includeCaches = parameters.value("includeCaches").toBool(false);

    QFileInfoList files;
    const auto exclusions = parameters.value("exclude").toArray();
    const auto excluded = [root, gamePrefix, includeLogs, includeCrashReports, includeCaches, exclusions, format](const QFileInfo& file) {
        auto relative = QDir::fromNativeSeparators(root.relativeFilePath(file.absoluteFilePath()));
        if (file.isSymLink()) return true;
        for (const auto& path : exclusions) {
            const auto blocked = QDir::cleanPath(QDir::fromNativeSeparators(path.toString()));
            if (relative == blocked || relative.startsWith(blocked + '/')) return true;
        }
        if (format != "zip" && (relative.endsWith(".pw.toml") || relative.contains("/.index/"))) return true;
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
    Task::Ptr task;
    if (format == "zip") {
        if (!MMCZip::collectFileListRecursively(instance->instanceRoot(), nullptr, &files, excluded))
            return OperationService::failure(QObject::tr("Unable to enumerate instance files."));
        task = makeShared<MMCZip::ExportToZipTask>(stagedOutput, instance->instanceRoot(), files, "", true);
    } else {
        auto mc = dynamic_cast<MinecraftInstance*>(instance);
        if (!mc) return OperationService::failure("Pack export requires a Minecraft instance.", 2);
        const auto name = parameters.value("name").toString(instance->name());
        const auto version = parameters.value("version").toString();
        if (version.isEmpty()) return OperationService::failure("Pack version is required.", 2);
        if (format == "modrinth") task = makeShared<ModrinthPackExportTask>(name, version, parameters.value("summary").toString(),
            parameters.value("optionalFiles").toBool(), instance, stagedOutput, excluded);
        else {
            FlamePackExportOptions options{ name, version, parameters.value("author").toString(), parameters.value("optionalFiles").toBool(),
                MinecraftInstancePtr(mc, [](MinecraftInstance*) {}), stagedOutput, excluded, parameters.value("recommendedRAM").toInt() };
            task.reset(new FlamePackExportTask(std::move(options)));
        }
    }
    QString error;
    if (!waitForTask(task.get(), interaction, &error, api))
        return OperationService::failure(error);
    QFile source(stagedOutput);
    QSaveFile destination(outputPath);
    if (!source.open(QIODevice::ReadOnly) || !destination.open(QIODevice::WriteOnly)) return OperationService::failure("Could not publish export.");
    while (!source.atEnd()) {
        const auto bytes = source.read(1024 * 1024);
        if (bytes.isEmpty() || destination.write(bytes) != bytes.size()) return OperationService::failure("Export write failed.");
    }
    if (!destination.commit()) return OperationService::failure(destination.errorString());

    return OperationService::success(QJsonObject{ { "instance", instance->id() },
                                                  { "format", format },
                                                  { "path", QFileInfo(outputPath).absoluteFilePath() },
                                                  { "files", files.size() },
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
                                           { "overwrite", boolProperty("Replace an existing output file.") },
                                           { "includeLogs", boolProperty("Include the game logs directory.") },
                                           { "includeCrashReports", boolProperty("Include crash reports.") },
                                           { "includeCaches", boolProperty("Include launcher and loader cache directories.") } },
                                          { "instance", "output" }), {}, true },
                           [&api](const QJsonObject& parameters, UserInteraction& interaction) {
                               return exportInstance(parameters, interaction, api);
                           });
}
