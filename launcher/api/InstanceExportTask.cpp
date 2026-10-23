// SPDX-License-Identifier: GPL-3.0-only

#include "InstanceExportTask.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

#include "BaseInstance.h"
#include "MMCZip.h"
#include "archive/ExportToZipTask.h"
#include "minecraft/MinecraftInstance.h"
#include "modplatform/flame/FlamePackExportTask.h"
#include "modplatform/modrinth/ModrinthPackExportTask.h"

InstanceExportTask::InstanceExportTask(BaseInstance* instance, QJsonObject parameters)
    : m_instance(instance), m_parameters(std::move(parameters))
{
    setStatus(tr("正在准备导出实例"));
}

void InstanceExportTask::executeTask()
{
    if (!m_instance) {
        fail(tr("Instance not found."));
        return;
    }
    if (m_instance->isRunning()) {
        fail(tr("An instance cannot be exported while it is running."));
        return;
    }
    const auto outputPath = m_parameters.value("output").toString().trimmed();
    const auto format = m_parameters.value("format").toString("zip").trimmed().toLower();
    if (outputPath.isEmpty()) {
        fail(tr("An output path is required."));
        return;
    }
    if (format != "zip" && format != "modrinth" && format != "curseforge") {
        fail("format must be zip, modrinth, or curseforge.");
        return;
    }
    const QFileInfo outputInfo(outputPath);
    if (outputInfo.exists() && !m_parameters.value("overwrite").toBool(false)) {
        fail(tr("The output file already exists. Set overwrite=true to replace it."));
        return;
    }
    const auto parent = outputInfo.absoluteDir();
    if (!parent.exists() && !QDir().mkpath(parent.absolutePath())) {
        fail(tr("Could not create the output directory."));
        return;
    }
    const auto root = QDir(m_instance->instanceRoot());
    const auto outputRelative = root.relativeFilePath(outputInfo.absoluteFilePath());
    if (!outputRelative.startsWith("../") && !QDir::isAbsolutePath(outputRelative)) {
        fail("Export output must be outside the instance directory.");
        return;
    }

    m_staging = std::make_unique<QTemporaryDir>(parent.filePath(".neo-export-XXXXXX"));
    if (!m_staging->isValid()) {
        fail("Could not stage export.");
        return;
    }
    m_stagedOutput = m_staging->filePath("pack.zip");
    m_output = outputPath;

    const auto gameRoot = QDir(m_instance->gameRoot());
    const auto gamePrefix = root.relativeFilePath(gameRoot.absolutePath());
    const auto includeLogs = m_parameters.value("includeLogs").toBool(false);
    const auto includeCrashReports = m_parameters.value("includeCrashReports").toBool(false);
    const auto includeCaches = m_parameters.value("includeCaches").toBool(false);
    const auto exclusions = m_parameters.value("exclude").toArray();
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
            if (!includeLogs && (lower == (prefix + "logs").toLower() || lower.startsWith((prefix + "logs/").toLower()))) return true;
            if (!includeCrashReports &&
                (lower == (prefix + "crash-reports").toLower() || lower.startsWith((prefix + "crash-reports/").toLower())))
                return true;
            if (!includeCaches &&
                (lower == (prefix + ".cache").toLower() || lower.startsWith((prefix + ".cache/").toLower()) ||
                 lower == (prefix + ".fabric").toLower() || lower.startsWith((prefix + ".fabric/").toLower()) ||
                 lower == (prefix + ".quilt").toLower() || lower.startsWith((prefix + ".quilt/").toLower())))
                return true;
        }
        return false;
    };

    QFileInfoList files;
    if (format == "zip") {
        if (!MMCZip::collectFileListRecursively(m_instance->instanceRoot(), nullptr, &files, excluded)) {
            fail(tr("Unable to enumerate instance files."));
            return;
        }
        m_fileCount = files.size();
        m_child = makeShared<MMCZip::ExportToZipTask>(m_stagedOutput, m_instance->instanceRoot(), files, "", true);
    } else {
        auto mc = dynamic_cast<MinecraftInstance*>(m_instance);
        if (!mc) {
            fail("Pack export requires a Minecraft instance.");
            return;
        }
        const auto name = m_parameters.value("name").toString(m_instance->name());
        const auto version = m_parameters.value("version").toString();
        if (version.isEmpty()) {
            fail("Pack version is required.");
            return;
        }
        if (format == "modrinth") {
            m_child = makeShared<ModrinthPackExportTask>(name, version, m_parameters.value("summary").toString(),
                                                               m_parameters.value("optionalFiles").toBool(), m_instance, m_stagedOutput, excluded);
        } else {
            FlamePackExportOptions options{ name, version, m_parameters.value("author").toString(),
                m_parameters.value("optionalFiles").toBool(), MinecraftInstancePtr(mc, [](MinecraftInstance*) {}), m_stagedOutput,
                excluded, m_parameters.value("recommendedRAM").toInt() };
            m_child = Task::Ptr(new FlamePackExportTask(std::move(options)));
        }
    }
    if (!m_child) {
        fail(tr("The export task could not be created."));
        return;
    }
    connect(m_child.get(), &Task::status, this, &Task::setStatus);
    connect(m_child.get(), &Task::progress, this, &Task::setProgress);
    connect(m_child.get(), &Task::finished, this, [this] {
        if (m_aborted) return;
        if (!m_child->wasSuccessful()) {
            emitFailed(m_child->failReason().isEmpty() ? tr("Export failed.") : m_child->failReason());
            return;
        }
        publish();
    });
    m_child->start();
}

void InstanceExportTask::publish()
{
    setStatus(tr("正在写入导出文件"));
    QFile source(m_stagedOutput);
    QSaveFile destination(m_output);
    if (!source.open(QIODevice::ReadOnly) || !destination.open(QIODevice::WriteOnly)) {
        fail(tr("Could not publish export."));
        return;
    }
    while (!source.atEnd()) {
        const auto bytes = source.read(1024 * 1024);
        if (bytes.isEmpty() || destination.write(bytes) != bytes.size()) {
            fail(tr("Export write failed."));
            return;
        }
    }
    if (!destination.commit()) {
        fail(destination.errorString());
        return;
    }
    setStatus(tr("导出完成"));
    emitSucceeded();
}

void InstanceExportTask::fail(const QString& message)
{
    if (!m_aborted) emitFailed(message);
}

bool InstanceExportTask::abort()
{
    m_aborted = true;
    if (m_child) m_child->abort();
    emitAborted();
    return true;
}
