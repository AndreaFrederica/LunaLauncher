// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
 *  Copyright (c) 2022 flowln <flowlnlnln@gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *      Copyright 2013-2021 MultiMC Contributors
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#include "InstanceImportTask.h"
#include "cli/ScopedUserInteraction.h"

#include "Application.h"
#include "FileSystem.h"
#include "NullInstance.h"

#include "QObjectPtr.h"
#include "archive/ArchiveReader.h"
#include "archive/ExtractZipTask.h"
#include "icons/IconList.h"
#include "icons/IconUtils.h"
#include "meta/CleanroomMeta.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/OneSixVersionFormat.h"
#include "meta/Index.h"
#include "modplatform/LegacyJarModPack.h"

#include "modplatform/flame/FlameInstanceCreationTask.h"
#include "modplatform/modrinth/ModrinthInstanceCreationTask.h"
#include "modplatform/pcl/PCLPack.h"
#include "modplatform/pcl/PCLPlainPack.h"
#include "VanillaConversion.h"
#include "VanillaConversionTask.h"
#include "VanillaCompatibleProvider.h"
#include "modplatform/technic/TechnicPackProcessor.h"

#include "settings/INISettingsObject.h"
#include "tasks/Task.h"
#include "ui/dialogs/CustomMessageBox.h"

#include "net/ApiDownload.h"
#include "net/Download.h"

#include <QBuffer>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QInputDialog>
#include <QLineEdit>
#include <QTemporaryFile>
#include <QtConcurrentRun>
#include <algorithm>
#include <limits>
#include <memory>

InstanceImportTask::InstanceImportTask(const QUrl& sourceUrl, QWidget* parent, QMap<QString, QString>&& extra_info)
    : m_sourceUrl(sourceUrl), m_extra_info(extra_info), m_parent(parent)
{}

bool InstanceImportTask::abort()
{
    if (!canAbort())
        return false;

    bool wasAborted = false;
    if (m_task)
        wasAborted = m_task->abort();
    return wasAborted;
}

void InstanceImportTask::executeTask()
{
    setAbortable(true);

    if (m_sourceUrl.isLocalFile()) {
        m_archivePath = m_sourceUrl.toLocalFile();
        processZipPack();
    } else {
        setStatus(tr("Downloading modpack:\n%1").arg(m_sourceUrl.toString()));

        downloadFromUrl();
    }
}

void InstanceImportTask::downloadFromUrl()
{
    const QString path(m_sourceUrl.host() + '/' + m_sourceUrl.path());

    auto entry = APPLICATION->metacache()->resolveEntry("general", path);
    entry->setStale(true);
    m_archivePath = entry->getFullPath();

    auto filesNetJob = makeShared<NetJob>(tr("Modpack download"), APPLICATION->network());
    filesNetJob->addNetAction(Net::ApiDownload::makeCached(m_sourceUrl, entry));

    connect(filesNetJob.get(), &NetJob::succeeded, this, &InstanceImportTask::processZipPack);
    connect(filesNetJob.get(), &NetJob::progress, this, &InstanceImportTask::setProgress);
    connect(filesNetJob.get(), &Task::transferRate, this, &Task::setTransferRate);
    connect(filesNetJob.get(), &NetJob::stepProgress, this, &InstanceImportTask::propagateStepProgress);
    connect(filesNetJob.get(), &NetJob::failed, this, &InstanceImportTask::emitFailed);
    connect(filesNetJob.get(), &NetJob::aborted, this, &InstanceImportTask::emitAborted);
    m_task.reset(filesNetJob);
    filesNetJob->start();
}

QString cleanPath(QString path)
{
    if (path == ".")
        return QString();
    QString result = path;
    if (result.startsWith("./"))
        result = result.mid(2);
    return result;
}

QList<Task*> InstanceImportTask::getSubTasks() const
{
    return m_task ? QList<Task*>{ m_task.get() } : QList<Task*>{};
}

class DiscardWriteDevice final : public QIODevice {
   public:
    DiscardWriteDevice() { open(QIODevice::WriteOnly); }

   protected:
    qint64 readData(char*, qint64) override { return -1; }
    qint64 writeData(const char*, qint64 size) override { return size; }
};

namespace {
std::optional<int> selectPackProfile(QWidget* parent, const QString& title, const QString& prompt, const QJsonArray& choices)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    dialog.resize(720, 390);
    auto* layout = new QVBoxLayout(&dialog);
    auto* description = new QLabel(prompt, &dialog);
    description->setTextFormat(Qt::PlainText);
    description->setWordWrap(true);
    layout->addWidget(description);

    auto* tree = new QTreeWidget(&dialog);
    tree->setRootIsDecorated(false);
    tree->setWordWrap(true);
    tree->setHeaderLabels({ QObject::tr("Version"), QObject::tr("Validation") });
    for (qsizetype index = 0; index < choices.size(); ++index) {
        const auto choice = choices.at(index).toObject();
        const auto error = choice.value("error").toString();
        auto* row = new QTreeWidgetItem(tree, {
            choice.value("name").toString() + '\n' + choice.value("path").toString(),
            error.isEmpty() ? QObject::tr("Ready to import") + '\n' + choice.value("details").toString() : error,
        });
        row->setData(0, Qt::UserRole, static_cast<int>(index));
        if (!error.isEmpty()) {
            row->setFlags(row->flags() & ~Qt::ItemIsSelectable);
            row->setForeground(1, dialog.palette().brush(QPalette::LinkVisited));
        }
    }
    tree->setColumnWidth(0, 330);
    layout->addWidget(tree);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, &dialog);
    auto* confirm = buttons->addButton(QObject::tr("Import selected version"), QDialogButtonBox::AcceptRole);
    confirm->setEnabled(false);
    QObject::connect(tree, &QTreeWidget::itemSelectionChanged, &dialog, [tree, confirm] {
        confirm->setEnabled(!tree->selectedItems().isEmpty());
    });
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted || tree->selectedItems().isEmpty()) return std::nullopt;
    return tree->selectedItems().constFirst()->data(0, Qt::UserRole).toInt();
}

QJsonObject readArchiveProfile(MMCZip::ArchiveReader::File& file)
{
    if (file.size() < 0 || file.size() > 4 * 1024 * 1024) return {};
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly);
    if (!file.copyTo(buffer, 4 * 1024 * 1024)) return {};
    return QJsonDocument::fromJson(buffer.data()).object();
}
}  // namespace

void InstanceImportTask::processZipPack()
{
    setStatus(tr("Attempting to determine instance type"));
    QDir extractDir(m_stagingPath);
    qDebug() << "Attempting to create instance from" << m_archivePath;

    // open the zip and find relevant files in it
    MMCZip::ArchiveReader packZip(m_archivePath);
    qDebug() << "Attempting to determine instance type";

    QString root;
    QStringList nestedPackCandidates;
    QStringList archiveFiles;
    QHash<QString, QJsonObject> profiles;
    QSet<QString> encryptedProfiles;
    QString encryptedValidationPath;
    qint64 encryptedValidationSize = std::numeric_limits<qint64>::max();
    m_modpackType = ModpackType::Unknown;
    m_legacyJarMod = false;
    // NOTE: Prioritize modpack platforms that aren't searched for recursively.
    // Especially Flame has a very common filename for its manifest, which may appear inside overrides for example
    // https://docs.modrinth.com/docs/modpacks/format_definition/#storage
    auto detectInstance = [this, &extractDir, &root, &nestedPackCandidates, &archiveFiles, &encryptedValidationPath,
                           &encryptedValidationSize, &profiles, &encryptedProfiles](MMCZip::ArchiveReader::File* f, bool& stop) {
        if (!isRunning()) {
            stop = true;
            return true;
        }
        auto fileName = f->filename();
        if (f->isFile()) {
            const auto path = QDir::cleanPath(QString(fileName).replace('\\', '/'));
            archiveFiles.append(path);
            const QFileInfo profilePath(path);
            if (m_modpackType == ModpackType::Unknown
                && (PCL::isVersionProfilePath(path) || (profilePath.suffix() == "json"
                    && profilePath.completeBaseName() == profilePath.dir().dirName()))) {
                if (f->isEncrypted()) encryptedProfiles.insert(path);
                else profiles.insert(path, readArchiveProfile(*f));
            }
            const auto fileSize = f->size();
            if (f->isEncrypted() && fileSize >= 0 &&
                (encryptedValidationSize == std::numeric_limits<qint64>::max() ||
                 (fileSize > 0 && (encryptedValidationSize == 0 || fileSize < encryptedValidationSize)))) {
                encryptedValidationPath = fileName;
                encryptedValidationSize = fileSize;
            }
        }
        if (f->isFile() && !PCL::findNestedPackCandidates({ fileName }).isEmpty()) {
            nestedPackCandidates.append(fileName);
        }
        if (m_modpackType != ModpackType::Unknown) {
            QCoreApplication::processEvents();
            return true;
        }
        if (fileName == "modrinth.index.json") {
            // process as Modrinth pack
            qDebug() << "Modrinth:" << true;
            m_modpackType = ModpackType::Modrinth;
        } else if (fileName == "bin/modpack.jar" || fileName == "bin/version.json") {
            // process as Technic pack
            qDebug() << "Technic:" << true;
            extractDir.mkpath("minecraft");
            extractDir.cd("minecraft");
            m_modpackType = ModpackType::Technic;
        } else if (fileName == "manifest.json") {
            qDebug() << "Flame:" << true;
            m_modpackType = ModpackType::Flame;
        } else if (QFileInfo fileInfo(fileName); fileInfo.fileName() == "instance.cfg") {
            qDebug() << "MultiMC:" << true;
            m_modpackType = ModpackType::MultiMC;
            root = cleanPath(fileInfo.path());
        }
        QCoreApplication::processEvents();
        return true;
    };
    if (!packZip.parse(detectInstance)) {
        emitFailed(tr("Unable to open supplied modpack zip file."));
        return;
    }
    if (!isRunning())
        return;
    auto requestArchivePassphrase = [this, &encryptedValidationPath]() {
        if (encryptedValidationPath.isEmpty() || !m_archivePassphrase.isEmpty())
            return true;
        const auto prompt = tr("请输入压缩包 %1 的解压密码：")
                                .arg(m_nestedArchiveName.isEmpty() ? QFileInfo(m_archivePath).fileName() : m_nestedArchiveName);
        if (APPLICATION->isHeadless()) {
            while (activeUserInteraction) {
                const auto password = activeUserInteraction->input(prompt, true);
                if (!password) { emitAborted(); return false; }
                MMCZip::ArchiveReader validator(m_archivePath, *password);
                auto encryptedFile = validator.goToFile(encryptedValidationPath);
                DiscardWriteDevice sink;
                if (encryptedFile && encryptedFile->copyTo(sink)) { m_archivePassphrase = *password; return true; }
                activeUserInteraction->status(tr("Incorrect archive password. Try again or cancel."));
            }
            emitFailed(tr("Archive password input is unavailable.")); return false;
        }
        while (true) {
            bool accepted = false;
            const auto passphrase = QInputDialog::getText(m_parent, tr("Encrypted modpack"), prompt,
                                                          QLineEdit::Password, QString(), &accepted);
            if (!accepted) {
                emitAborted();
                return false;
            }
            MMCZip::ArchiveReader validator(m_archivePath, passphrase);
            auto encryptedFile = validator.goToFile(encryptedValidationPath);
            DiscardWriteDevice sink;
            if (encryptedFile && encryptedFile->copyTo(sink)) {
                m_archivePassphrase = passphrase;
                return true;
            }
            CustomMessageBox::selectable(m_parent, tr("Incorrect archive password"),
                                         tr("The password could not decrypt this modpack. Try again or cancel the import."),
                                         QMessageBox::Warning)
                ->exec();
        }
    };
    if (m_modpackType == ModpackType::Unknown) {
        if (!encryptedProfiles.isEmpty()) {
            if (!requestArchivePassphrase()) return;
            MMCZip::ArchiveReader reader(m_archivePath, m_archivePassphrase);
            if (!reader.parse([&](MMCZip::ArchiveReader::File* file, bool& stop) {
                if (!isRunning()) { stop = true; return true; }
                const auto path = QDir::cleanPath(file->filename().replace('\\', '/'));
                if (encryptedProfiles.remove(path)) profiles.insert(path, readArchiveProfile(*file));
                stop = encryptedProfiles.isEmpty();
                QCoreApplication::processEvents();
                return true;
            })) {
                emitFailed(tr("Could not read the encrypted version profiles."));
                return;
            }
            if (!isRunning()) return;
        }
        auto candidates = PCL::findPlainPackCandidates(archiveFiles, profiles);
        const auto installers = LegacyJarModPack::findCandidates(archiveFiles);
        for (const auto& candidate : installers) {
            const auto path = candidate.root + candidate.version + '/' + candidate.version + ".json";
            candidates.removeIf( [&](const auto& entry) { return entry.profilePath == path; });
            candidates.append({ candidate.root, candidate.version, path, {}, {} });
        }
        // Prefer a game profile in this archive over resource ZIPs bundled with it.
        if (!candidates.isEmpty())
            nestedPackCandidates.clear();
        nestedPackCandidates.removeDuplicates();
        if (nestedPackCandidates.size() > 1) {
            emitFailed(tr("压缩包中有多个内层 ZIP / MRPACK，无法确定要导入哪一个。请解压外层后，直接导入所需的整合包：\n%1")
                           .arg(nestedPackCandidates.join('\n')));
            return;
        }
        if (nestedPackCandidates.size() == 1) {
            if (m_nestedArchiveDepth >= 1) {
                emitFailed(tr("内层压缩包仍包含其他压缩包，已停止继续嵌套解压。请解压外层后，直接导入包含游戏文件的 ZIP / MRPACK。"));
                return;
            }

            if (!requestArchivePassphrase())
                return;
            MMCZip::ArchiveReader nestedSource(m_archivePath, m_archivePassphrase);
            auto nestedFile = nestedSource.goToFile(nestedPackCandidates.constFirst());
            constexpr qint64 MAX_NESTED_PACK_SIZE = 8LL * 1024 * 1024 * 1024;
            if (!nestedFile || !nestedFile->isFile() || nestedFile->size() <= 0 || nestedFile->size() > MAX_NESTED_PACK_SIZE) {
                emitFailed(tr("内层整合包为空或超过 8 GiB 大小限制：%1").arg(nestedPackCandidates.constFirst()));
                return;
            }

            setStatus(tr("正在读取内层整合包：%1").arg(nestedPackCandidates.constFirst()));
            auto temporary = std::make_unique<QTemporaryFile>(QDir::tempPath() + "/luna-nested-XXXXXX.zip");
            temporary->setAutoRemove(true);
            if (!temporary->open() || !nestedFile->copyTo(*temporary) || !temporary->flush()) {
                emitFailed(tr("无法读取内层整合包：%1").arg(nestedPackCandidates.constFirst()));
                return;
            }
            temporary->close();

            m_nestedArchive = std::move(temporary);
            m_nestedArchiveName = nestedPackCandidates.constFirst();
            m_archivePath = m_nestedArchive->fileName();
            m_archivePassphrase.clear();
            m_nestedArchiveDepth++;
            const auto nestedName = QFileInfo(m_nestedArchiveName).fileName();
            m_pclWrapperDetected |= nestedName.compare("modpack.zip", Qt::CaseInsensitive) == 0 ||
                                    nestedName.compare("modpack.mrpack", Qt::CaseInsensitive) == 0;
            // Reuse the complete detector, converter and password interaction for
            // the inner archive, including vanilla directory and patched JAR packs.
            processZipPack();
            return;
        }
        if (!candidates.isEmpty()) {
            const QSet<QString> files(archiveFiles.cbegin(), archiveFiles.cend());
            QJsonArray choices;
            QStringList errors;
            QList<bool> legacy;
            for (auto& candidate : candidates) {
                const bool installer = std::any_of(installers.cbegin(), installers.cend(), [&](const auto& entry) {
                    return candidate.profilePath == entry.root + entry.version + '/' + entry.version + ".json";
                });
                legacy.append(installer);
                PCL::validatePlainPackCandidate(candidate, profiles, files, installer);
                if (!candidate.error.isEmpty())
                    errors.append(candidate.version + ": " + candidate.error);
                choices.append(QJsonObject{ { "name", candidate.version }, { "id", candidate.profilePath },
                    { "path", candidate.profilePath }, { "details", candidate.details }, { "error", candidate.error },
                    { "disabled", !candidate.error.isEmpty() }, { "kind", "import-profile" } });
            }
            std::optional<int> selection = 0;
            if (candidates.size() > 1 || !errors.isEmpty()) {
                const auto title = tr("Select a version to import");
                const auto prompt = errors.size() == candidates.size()
                    ? tr("None of the detected versions can be imported.")
                    : tr("Choose a version that passed validation to continue.");
                if (APPLICATION->isHeadless())
                    selection = activeUserInteraction ? activeUserInteraction->select(title, choices) : std::nullopt;
                else
                    selection = selectPackProfile(m_parent, title, prompt, choices);
            }

            if (!selection || *selection < 0 || *selection >= candidates.size() || !candidates.at(*selection).error.isEmpty()) {
                if (errors.size() == candidates.size()) emitFailed(errors.join('\n'));
                else emitAborted();
                return;
            }
            const auto& candidate = candidates.at(*selection);
            m_modpackType = ModpackType::PCLPlain;
            m_pclPlainVersion = candidate.version;
            m_legacyJarMod = legacy.at(*selection);
            m_pclProfilePath = candidate.profilePath.mid(candidate.root.size());
            root = candidate.root;
            m_pclWrapperDetected = true;
        } else {
            emitFailed(m_nestedArchiveName.isEmpty()
                           ? tr("Archive does not contain a recognized modpack type.")
                           : tr("内层压缩包 %1 未包含可识别的整合包类型。").arg(m_nestedArchiveName));
            return;
        }
    }
    if (!requestArchivePassphrase())
        return;
    setStatus(tr("Extracting modpack"));

    // make sure we extract just the pack
    if (m_modpackType == ModpackType::PCLPlain) {
        extractDir.mkpath("pcl-source");
        extractDir.cd("pcl-source");
    }
    auto zipTask = makeShared<MMCZip::ExtractZipTask>(m_archivePath, extractDir, root, m_archivePassphrase);

    auto progressStep = std::make_shared<TaskStepProgress>();
    connect(zipTask.get(), &Task::finished, this, [this, progressStep] {
        progressStep->state = TaskStepState::Succeeded;
        propagateStepProgress(*progressStep);
    });

    connect(zipTask.get(), &Task::succeeded, this, [this] {
        if (m_modpackType == ModpackType::PCLPlain && !m_legacyJarMod) {
            const QDir source(FS::PathCombine(m_stagingPath, "pcl-source"));
            const auto canonical = source.filePath("versions/" + m_pclPlainVersion + '/' + m_pclPlainVersion + ".json");
            const auto selected = source.filePath(m_pclProfilePath);
            if (selected != canonical && !QFile::copy(selected, canonical)) {
                emitFailed(tr("Could not prepare the selected version JSON: %1").arg(m_pclProfilePath));
                return;
            }
        }
        if (m_legacyJarMod) prepareLegacyJarMod();
        else extractFinished();
    }, Qt::QueuedConnection);
    connect(zipTask.get(), &Task::aborted, this, [this] {
        if (m_legacyJarMod) finishLegacyJarModEarly();
        else emitAborted();
    });
    connect(zipTask.get(), &Task::failed, this, [this, progressStep](QString reason) {
        progressStep->state = TaskStepState::Failed;
        propagateStepProgress(*progressStep);
        if (m_legacyJarMod) finishLegacyJarModEarly(reason);
        else emitFailed(reason);
    });
    connect(zipTask.get(), &Task::stepProgress, this, &InstanceImportTask::propagateStepProgress);

    connect(zipTask.get(), &Task::progress, this, [this, progressStep](qint64 current, qint64 total) {
        progressStep->update(current, total);
        propagateStepProgress(*progressStep);
    });
    connect(zipTask.get(), &Task::transferRate, this, &Task::setTransferRate);
    connect(zipTask.get(), &Task::status, this, [this, progressStep](QString status) {
        progressStep->status = status;
        propagateStepProgress(*progressStep);
    });
    connect(zipTask.get(), &Task::warningLogged, this, [this](const QString& line) { m_Warnings.append(line); });
    m_task.reset(zipTask);
    zipTask->start();
}

void InstanceImportTask::prepareLegacyJarMod()
{
    const auto root = QDir(m_stagingPath).filePath("pcl-source");
    QFile file(QDir(root).filePath(m_pclPlainVersion + '/' + m_pclPlainVersion + ".json"));
    if (!file.open(QIODevice::ReadOnly)) { finishLegacyJarModEarly(tr("无法读取旧式 JAR 补丁包的版本 JSON。")); return; }
    QString error;
    const auto base = LegacyJarModPack::baseVersion(QJsonDocument::fromJson(file.readAll()).object(), m_pclPlainVersion, error);
    file.close();
    if (base.isEmpty()) { finishLegacyJarModEarly(error); return; }
    setStatus(tr("已识别旧式 JAR 补丁包；正在获取 Minecraft %1 基底元数据").arg(base));
    auto metadata = APPLICATION->metadataIndex()->loadVersion("net.minecraft", base);
    connect(metadata.get(), &Task::progress, this, &InstanceImportTask::setProgress);
    connect(metadata.get(), &Task::stepProgress, this, &InstanceImportTask::propagateStepProgress);
    connect(metadata.get(), &Task::aborted, this, [this] { finishLegacyJarModEarly(); });
    connect(metadata.get(), &Task::failed, this, [this, base](const QString& reason) {
        finishLegacyJarModEarly(tr("无法获取补丁基底 Minecraft %1 的元数据：%2").arg(base, reason));
    });
    connect(metadata.get(), &Task::succeeded, this, [this, base, root] {
        const auto version = APPLICATION->metadataIndex()->get("net.minecraft", base);
        if (!version || !version->data()) { finishLegacyJarModEarly(tr("补丁基底的 Minecraft 版本元数据为空。")); return; }
        m_legacyBaseProfile = OneSixVersionFormat::versionFileToJson(version->data()).object();
        const auto client = m_legacyBaseProfile.value("mainJar").toObject().value("downloads").toObject().value("artifact").toObject();
        const QUrl url(client.value("url").toString());
        static const QRegularExpression sha1("^[0-9a-fA-F]{40}$");
        if (!url.isValid() || (url.scheme() != "https" && url.scheme() != "http")
            || !sha1.match(client.value("sha1").toString()).hasMatch() || client.value("size").toInteger() <= 0) {
            finishLegacyJarModEarly(tr("补丁基底的版本元数据缺少有效的客户端地址、SHA-1 或文件大小；已停止安装。")); return;
        }
        m_legacyBaseJar = QDir(root).filePath(".legacy-base.jar");
        setStatus(tr("正在下载并校验补丁基底 Minecraft %1").arg(base));
        auto download = makeShared<NetJob>(tr("下载旧式补丁包的基础客户端"), APPLICATION->network());
        auto action = Net::Download::makeFile(url, m_legacyBaseJar);
        action->setExpectedIntegrity("sha1", client.value("sha1").toString(), client.value("size").toInteger());
        download->addNetAction(action);
        connect(download.get(), &Task::progress, this, &InstanceImportTask::setProgress);
        connect(download.get(), &Task::stepProgress, this, &InstanceImportTask::propagateStepProgress);
        connect(download.get(), &Task::transferRate, this, &Task::setTransferRate);
        connect(download.get(), &Task::aborted, this, [this] { finishLegacyJarModEarly(); });
        connect(download.get(), &Task::failed, this, [this, base](const QString& reason) {
            finishLegacyJarModEarly(tr("下载或校验补丁基底 Minecraft %1 失败：%2").arg(base, reason));
        });
        connect(download.get(), &Task::succeeded, this, &InstanceImportTask::extractFinished, Qt::QueuedConnection);
        m_task = download;
        download->start();
    }, Qt::QueuedConnection);
    m_task = metadata;
    metadata->start();
}

void InstanceImportTask::finishLegacyJarModEarly(const QString& error)
{
    setAbortable(false);
    setStatus(tr("正在清理解压临时文件"));
    // Release the completed network task and its file handles after its signal
    // stack unwinds, then clean up outside the launcher thread.
    QMetaObject::invokeMethod(this, [this, error] {
        const auto staging = m_stagingPath;
        auto cleanup = makeShared<VanillaConversionTask>([staging](const VanillaConversionFeedback& feedback) {
            feedback.update(tr("正在清理解压临时文件"));
            VanillaConversionReport report;
            const bool removed = FS::deletePath(staging);
            const bool removedConverted = FS::deletePath(staging + ".converted");
            report.succeeded = removed && removedConverted;
            if (!report.succeeded) report.error = tr("无法完整清理导入临时目录：%1").arg(staging);
            return report;
        });
        connect(cleanup.get(), &Task::status, this, &InstanceImportTask::setStatus);
        connect(cleanup.get(), &Task::stepProgress, this, &InstanceImportTask::propagateStepProgress);
        connect(cleanup.get(), &Task::succeeded, this, [this, error] {
            if (error.isEmpty()) emitAborted();
            else emitFailed(error);
        });
        connect(cleanup.get(), &Task::failed, this, [this, error](const QString& cleanupError) {
            emitFailed(error.isEmpty() ? cleanupError : error + '\n' + cleanupError);
        });
        m_task = cleanup;
        cleanup->start();
    }, Qt::QueuedConnection);
}

void InstanceImportTask::extractFinished()
{
    const auto staging = m_stagingPath;
    const auto version = m_pclPlainVersion;
    const auto convert = m_modpackType == ModpackType::PCLPlain && !m_preserveVanillaSource;
    VanillaImportOptions options;
    options.name = name().isEmpty() ? version : name();
    options.group = group();
    const auto installer = m_legacyJarMod;
    const auto baseProfile = m_legacyBaseProfile;
    const auto baseJar = m_legacyBaseJar;
    auto preparation = makeShared<VanillaConversionTask>([staging, version, convert, options, installer, baseProfile, baseJar](const VanillaConversionFeedback& feedback) mutable {
        const auto prepare = [&]() -> VanillaConversionReport {
            VanillaConversionReport report;
            const auto checkCancelled = [&] {
                if (!feedback.cancelled()) return false;
                report.error = tr("导入已取消。");
                return true;
            };
            feedback.update(tr("正在扫描解压文件"));
            QStringList paths;
            QDirIterator it(staging, QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                if (checkCancelled()) return report;
                paths.append(it.next());
                feedback.update(tr("正在扫描解压文件"), paths.size(), 0);
            }
            feedback.update(tr("正在整理文件权限"), 0, paths.size());
            qint64 processed = 0;
            for (const auto& path : paths) {
                if (checkCancelled()) return report;
                const QFileInfo file(path);
                const auto original = QFile::permissions(path);
                auto permissions = original | QFileDevice::Permission::ReadUser | QFileDevice::Permission::WriteUser;
                if (file.isDir()) permissions |= QFileDevice::Permission::ExeUser;
                if (permissions != original && !QFile::setPermissions(path, permissions))
                    report.warnings.append(tr("Could not fix permissions for %1").arg(path));
                feedback.update(tr("正在整理文件权限"), ++processed, paths.size(), "files", QDir(staging).relativeFilePath(path));
            }
            if (installer && !LegacyJarModPack::prepare(QDir(staging).filePath("pcl-source"), version, baseProfile, baseJar, feedback, report.error))
                return report;
            if (convert) {
                const auto sourceRoot = QDir(staging).filePath("pcl-source");
                const auto versionRoot = QDir(sourceRoot).filePath("versions/" + version);
                const auto layout = detectVanillaInstanceLayout(versionRoot);
                const auto relative = QDir(sourceRoot).relativeFilePath(layout.gameRoot);
                if (!layout.isValid() || QDir::isAbsolutePath(relative) || relative == ".." || relative.startsWith("../")) {
                    report.error = tr("The directory pack refers to a game directory outside the archive.");
                    return report;
                }
                options.feedback = feedback;
                const auto converted = staging + ".converted";
                const auto warnings = report.warnings;
                report = VanillaConverter::importToPrism(versionRoot, converted, options);
                report.warnings.append(warnings);
                if (!report.succeeded) return report;
                report.succeeded = false;
                if (checkCancelled()) { FS::deletePath(converted); return report; }
                feedback.update(tr("正在清理解压临时文件"), 0, 0);
                // Large recursive deletion must also stay off the launcher thread.
                if (!FS::deletePath(staging) || !QDir().rename(converted, staging)) {
                    FS::deletePath(converted);
                    report.error = tr("Could not commit the converted directory pack.");
                    return report;
                }
            }
            report.succeeded = true;
            return report;
        };
        auto report = prepare();
        if (!report.succeeded) {
            feedback.update(tr("正在清理解压临时文件"));
            FS::deletePath(staging);
            FS::deletePath(staging + ".converted");
        }
        return report;
    });
    connect(preparation.get(), &Task::progress, this, &InstanceImportTask::setProgress);
    connect(preparation.get(), &Task::status, this, &InstanceImportTask::setStatus);
    connect(preparation.get(), &Task::details, this, &InstanceImportTask::setDetails);
    connect(preparation.get(), &Task::stepProgress, this, &InstanceImportTask::propagateStepProgress);
    connect(preparation.get(), &Task::aborted, this, &InstanceImportTask::emitAborted);
    connect(preparation.get(), &Task::failed, this, &InstanceImportTask::emitFailed);
    connect(preparation.get(), &Task::succeeded, this, [this, worker = preparation.get()] {
        for (const auto& warning : worker->report().warnings) logWarning(warning);
        extractedFilesPrepared();
    });
    m_task = preparation;
    preparation->start();
}

void InstanceImportTask::extractedFilesPrepared()
{
    setAbortable(false);
    switch (m_modpackType) {
        case ModpackType::MultiMC:
            processMultiMC();
            return;
        case ModpackType::Technic:
            processTechnic();
            return;
        case ModpackType::Flame:
            processFlame();
            return;
        case ModpackType::Modrinth:
            processModrinth();
            return;
        case ModpackType::PCLPlain:
            processPCLPlain();
            return;
        case ModpackType::Unknown:
            emitFailed(tr("Archive does not contain a recognized modpack type."));
            return;
    }
}

bool installIcon(QString root, QString instIconKey)
{
    auto importIconPath = IconUtils::findBestIconIn(root, instIconKey);
    if (importIconPath.isNull() || !QFile::exists(importIconPath))
        importIconPath = IconUtils::findBestIconIn(root, "icon.png");
    if (importIconPath.isNull() || !QFile::exists(importIconPath))
        importIconPath = IconUtils::findBestIconIn(FS::PathCombine(root, "overrides"), "icon.png");
    if (!importIconPath.isNull() && QFile::exists(importIconPath)) {
        // import icon
        auto iconList = APPLICATION->icons();
        if (iconList->iconFileExists(instIconKey)) {
            iconList->deleteIcon(instIconKey);
        }
        iconList->installIcon(importIconPath, instIconKey + "." + QFileInfo(importIconPath).suffix());
        return true;
    }
    return false;
}

void InstanceImportTask::processPCLPlain()
{
    const auto sourceRoot = FS::PathCombine(m_stagingPath, "pcl-source");
    const auto versionRoot = FS::PathCombine(sourceRoot, "versions", m_pclPlainVersion);
    if (m_preserveVanillaSource) {
        const auto layout = detectVanillaInstanceLayout(versionRoot);
        const auto relative = QDir(sourceRoot).relativeFilePath(layout.gameRoot);
        if (!layout.isValid() || QDir::isAbsolutePath(relative) || relative == ".." || relative.startsWith("../")) {
            emitFailed(tr("The directory pack refers to a game directory outside the archive."));
            return;
        }
        m_vanillaSource = versionRoot;
        if (name().isEmpty()) setName(m_pclPlainVersion);
        emitSucceeded();
        return;
    }
    auto settings = std::make_unique<INISettingsObject>(FS::PathCombine(m_stagingPath, "instance.cfg"));
    MinecraftInstance instance(m_globalSettings, std::move(settings), m_stagingPath);
    instance.setName(name().isEmpty() ? m_pclPlainVersion : name());
    instance.setIconKey(m_instIcon);
    const auto settingsReport = PCL::convertInstanceConfig(instance);
    if (!settingsReport.error.isEmpty()) logWarning(settingsReport.error);
    instance.saveNow();
    emitSucceeded();
}

void InstanceImportTask::addPclCompatibilityWarning()
{
    const auto warning =
        tr("This instance was imported from a PCL modpack using experimental compatibility support. Some PCL-specific settings "
           "or files may not behave exactly as they do in PCL. Review the PCL compatibility page and the reports in "
           "lunaui/migration before launching the instance.");
    if (!m_Warnings.contains(warning))
        logWarning(warning);
}

void InstanceImportTask::processFlame()
{
    shared_qobject_ptr<FlameCreationTask> inst_creation_task = nullptr;
    if (!m_extra_info.isEmpty()) {
        auto pack_id_it = m_extra_info.constFind("pack_id");
        Q_ASSERT(pack_id_it != m_extra_info.constEnd());
        auto pack_id = pack_id_it.value();

        auto pack_version_id_it = m_extra_info.constFind("pack_version_id");
        Q_ASSERT(pack_version_id_it != m_extra_info.constEnd());
        auto pack_version_id = pack_version_id_it.value();

        QString original_instance_id;
        auto original_instance_id_it = m_extra_info.constFind("original_instance_id");
        if (original_instance_id_it != m_extra_info.constEnd())
            original_instance_id = original_instance_id_it.value();

        inst_creation_task =
            makeShared<FlameCreationTask>(m_stagingPath, m_globalSettings, m_parent, pack_id, pack_version_id, original_instance_id);
    } else {
        // FIXME: Find a way to get IDs in directly imported ZIPs
        inst_creation_task = makeShared<FlameCreationTask>(m_stagingPath, m_globalSettings, m_parent, QString(), QString());
    }

    inst_creation_task->setName(*this);
    // if the icon was specified by user, use that. otherwise pull icon from the pack
    if (m_instIcon == "default") {
        auto iconKey = QString("Flame_%1_Icon").arg(name());

        if (installIcon(m_stagingPath, iconKey)) {
            m_instIcon = iconKey;
        }
    }
    inst_creation_task->setIcon(m_instIcon);
    inst_creation_task->setGroup(m_instGroup);
    inst_creation_task->setConfirmUpdate(shouldConfirmUpdate());

    auto weak = inst_creation_task.toWeakRef();
    connect(inst_creation_task.get(), &Task::succeeded, this, [this, weak] {
        if (auto sp = weak.lock()) {
            setOverride(sp->shouldOverride(), sp->originalInstanceID());
        }
        emitSucceeded();
    });
    connect(inst_creation_task.get(), &Task::failed, this, &InstanceImportTask::emitFailed);
    connect(inst_creation_task.get(), &Task::progress, this, &InstanceImportTask::setProgress);
    connect(inst_creation_task.get(), &Task::transferRate, this, &Task::setTransferRate);
    connect(inst_creation_task.get(), &Task::stepProgress, this, &InstanceImportTask::propagateStepProgress);
    connect(inst_creation_task.get(), &Task::status, this, &InstanceImportTask::setStatus);
    connect(inst_creation_task.get(), &Task::details, this, &InstanceImportTask::setDetails);

    connect(inst_creation_task.get(), &Task::aborted, this, &InstanceImportTask::emitAborted);
    connect(inst_creation_task.get(), &Task::abortStatusChanged, this, &Task::setAbortable);
    connect(inst_creation_task.get(), &Task::abortButtonTextChanged, this, &Task::setAbortButtonText);

    connect(inst_creation_task.get(), &Task::warningLogged, this, [this](const QString& line) { m_Warnings.append(line); });

    m_task.reset(inst_creation_task);
    setAbortable(true);
    m_task->start();
}

void InstanceImportTask::processTechnic()
{
    shared_qobject_ptr<Technic::TechnicPackProcessor> packProcessor{ new Technic::TechnicPackProcessor };
    connect(packProcessor.get(), &Technic::TechnicPackProcessor::succeeded, this, &InstanceImportTask::emitSucceeded);
    connect(packProcessor.get(), &Technic::TechnicPackProcessor::failed, this, &InstanceImportTask::emitFailed);
    packProcessor->run(m_globalSettings, name(), m_instIcon, m_stagingPath);
}

void InstanceImportTask::processMultiMC()
{
    Meta::Cleanroom::normalizeInstance(m_stagingPath);

    QString configPath = FS::PathCombine(m_stagingPath, "instance.cfg");
    auto instanceSettings = std::make_unique<INISettingsObject>(configPath);

    NullInstance instance(m_globalSettings, std::move(instanceSettings), m_stagingPath);

    // reset time played on import... because packs.
    instance.resetTimePlayed();

    // set a new nice name
    instance.setName(name());

    // if the icon was specified by user, use that. otherwise pull icon from the pack
    if (m_instIcon != "default") {
        instance.setIconKey(m_instIcon);
    } else {
        m_instIcon = instance.iconKey();

        installIcon(instance.instanceRoot(), m_instIcon);
    }
    emitSucceeded();
}

void InstanceImportTask::processModrinth()
{
    shared_qobject_ptr<ModrinthCreationTask> inst_creation_task = nullptr;
    if (!m_extra_info.isEmpty()) {
        auto pack_id_it = m_extra_info.constFind("pack_id");
        Q_ASSERT(pack_id_it != m_extra_info.constEnd());
        auto pack_id = pack_id_it.value();

        QString pack_version_id;
        auto pack_version_id_it = m_extra_info.constFind("pack_version_id");
        if (pack_version_id_it != m_extra_info.constEnd())
            pack_version_id = pack_version_id_it.value();

        QString original_instance_id;
        auto original_instance_id_it = m_extra_info.constFind("original_instance_id");
        if (original_instance_id_it != m_extra_info.constEnd())
            original_instance_id = original_instance_id_it.value();

        inst_creation_task =
            makeShared<ModrinthCreationTask>(m_stagingPath, m_globalSettings, m_parent, pack_id, pack_version_id, original_instance_id);
    } else {
        QString pack_id;
        if (!m_sourceUrl.isEmpty()) {
            static const QRegularExpression s_regex(R"(data\/([^\/]*)\/versions)");
            pack_id = s_regex.match(m_sourceUrl.toString()).captured(1);
        }

        // FIXME: Find a way to get the ID in directly imported ZIPs
        inst_creation_task = makeShared<ModrinthCreationTask>(m_stagingPath, m_globalSettings, m_parent, pack_id);
    }

    inst_creation_task->setName(*this);
    // if the icon was specified by user, use that. otherwise pull icon from the pack
    if (m_instIcon == "default") {
        auto iconKey = QString("Modrinth_%1_Icon").arg(name());

        if (installIcon(m_stagingPath, iconKey)) {
            m_instIcon = iconKey;
        }
    }
    inst_creation_task->setIcon(m_instIcon);
    inst_creation_task->setGroup(m_instGroup);
    inst_creation_task->setConfirmUpdate(shouldConfirmUpdate());

    auto weak = inst_creation_task.toWeakRef();
    connect(inst_creation_task.get(), &Task::succeeded, this, [this, weak] {
        if (auto sp = weak.lock()) {
            setOverride(sp->shouldOverride(), sp->originalInstanceID());
            if (m_pclWrapperDetected || sp->isPclPack())
                addPclCompatibilityWarning();
        } else if (m_pclWrapperDetected) {
            addPclCompatibilityWarning();
        }
        emitSucceeded();
    });
    connect(inst_creation_task.get(), &Task::failed, this, &InstanceImportTask::emitFailed);
    connect(inst_creation_task.get(), &Task::progress, this, &InstanceImportTask::setProgress);
    connect(inst_creation_task.get(), &Task::transferRate, this, &Task::setTransferRate);
    connect(inst_creation_task.get(), &Task::stepProgress, this, &InstanceImportTask::propagateStepProgress);
    connect(inst_creation_task.get(), &Task::status, this, &InstanceImportTask::setStatus);
    connect(inst_creation_task.get(), &Task::details, this, &InstanceImportTask::setDetails);

    connect(inst_creation_task.get(), &Task::aborted, this, &InstanceImportTask::emitAborted);
    connect(inst_creation_task.get(), &Task::abortStatusChanged, this, &Task::setAbortable);
    connect(inst_creation_task.get(), &Task::abortButtonTextChanged, this, &Task::setAbortButtonText);

    connect(inst_creation_task.get(), &Task::warningLogged, this, [this](const QString& line) { m_Warnings.append(line); });

    m_task.reset(inst_creation_task);
    setAbortable(true);
    m_task->start();
}
