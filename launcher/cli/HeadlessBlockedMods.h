// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "ScopedUserInteraction.h"
#include "Application.h"
#include "modplatform/BlockedMod.h"
#include "modplatform/flame/CurseForgeDownloadPageService.h"
#include "settings/SettingsObject.h"
#include <QCryptographicHash>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QSaveFile>
#include <QTimer>
#include <QJsonDocument>
#include <QStandardPaths>

inline bool resolveHeadlessBlockedMods(QList<BlockedMod>& mods, const QString& algorithm, QString& error)
{
    auto* interaction = activeUserInteraction;
    const auto cancelled = activeInteractionCancellation;
    if (!interaction) { error = "No interaction channel is available for restricted downloads."; return false; }
    if (!QStringList{ "md5", "sha1", "sha256", "sha512" }.contains(algorithm)) { error = "Unsupported restricted-file checksum."; return false; }
    const auto hashType = algorithm == "md5" ? QCryptographicHash::Md5 : algorithm == "sha512" ? QCryptographicHash::Sha512 : algorithm == "sha256" ? QCryptographicHash::Sha256 : QCryptographicHash::Sha1;
    auto verify = [hashType](const QString& path, const QString& expected) {
        QFile file(path);
        if (!QFileInfo(path).isFile() || !file.open(QIODevice::ReadOnly)) return false;
        QCryptographicHash hash(hashType);
        return hash.addData(&file) && !expected.isEmpty() && QString::fromLatin1(hash.result().toHex()).compare(expected, Qt::CaseInsensitive) == 0;
    };
    // Keep the existing automatic external helper path, without constructing a dialog.
    bool supportsHeadless = false;
    QString probeError;
    if (APPLICATION->settings()->get("CurseForgeDownloadBrowser").toString() == "External" &&
        CurseForgeDownloadPageService::probeExternalTool(APPLICATION->settings()->get("CurseForgeExternalToolPath").toString(), &probeError, &supportsHeadless) && supportsHeadless) {
        CurseForgeDownloadPageService service(CurseForgeDownloadPageService::Provider::External);
        QVector<CurseForgeDownloadPage> pages;
        for (const auto& mod : mods) if (!mod.matched && CurseForgeDownloadPageService::isSupportedUrl(mod.websiteUrl))
            pages.append({ mod.websiteUrl, mod.name, algorithm, mod.hash });
        if (!pages.isEmpty()) {
            QEventLoop loop;
            QTimer timeout, cancellation; timeout.setSingleShot(true); cancellation.setInterval(100);
            QObject::connect(&cancellation, &QTimer::timeout, &loop, [&] { if (cancelled && cancelled()) loop.quit(); });
            QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
            QObject::connect(&service, &CurseForgeDownloadPageService::completed, &loop, &QEventLoop::quit);
            QObject::connect(&service, &CurseForgeDownloadPageService::failed, &loop, [&](const QString& reason) { interaction->status(reason); loop.quit(); });
            QObject::connect(&service, &CurseForgeDownloadPageService::downloadProgress, &loop,
                [interaction](const QString& file, int index, int count, qint64 bytes, qint64) { interaction->status(QString("%1 (%2/%3): %4 bytes").arg(file).arg(index).arg(count).arg(bytes)); });
            QObject::connect(&service, &CurseForgeDownloadPageService::fileReady, &loop, [&](const QString& name, int, int) {
                const auto path = QDir(service.downloadDirectory()).filePath(name);
                for (auto& mod : mods) {
                    if (mod.matched || mod.name != name || !verify(path, mod.hash)) continue;
                    const auto verified = service.acceptDownloadedFile(path, mod.hash);
                    if (verified.isEmpty()) continue;
                    const auto destination = QDir(APPLICATION->dataRoot()).filePath("cache/restricted/" + QString::fromLatin1(QCryptographicHash::hash(mod.hash.toUtf8(), QCryptographicHash::Sha256).toHex()));
                    QDir().mkpath(QFileInfo(destination).absolutePath());
                    QFile source(verified); QSaveFile output(destination);
                    if (!source.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly)) continue;
                    bool copied = true;
                    while (!source.atEnd()) { const auto block = source.read(1024 * 1024); if (block.isEmpty() || output.write(block) != block.size()) { copied = false; break; } }
                    if (copied && output.commit()) { mod.localPath = destination; mod.matched = true; }
                }
            });
            if (service.open(pages, "headless")) { timeout.start(120000); cancellation.start(); loop.exec(); }
        }
    }
    QStringList directories{ APPLICATION->settings()->get("DownloadsDir").toString(),
                             QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) };
    directories.removeAll(QString());
    directories.removeDuplicates();
    auto matchFile = [&](const QString& path) {
        bool matched = false;
        for (auto& mod : mods) {
            if (!mod.matched && verify(path, mod.hash)) {
                mod.localPath = QFileInfo(path).absoluteFilePath();
                mod.matched = true;
                matched = true;
            }
        }
        return matched;
    };
    QString validationError;
    for (;;) {
        if (cancelled && cancelled()) { error = "Restricted download cancelled."; return false; }
        for (auto& mod : mods) {
            if (mod.matched && !verify(mod.localPath, mod.hash)) { mod.matched = false; mod.localPath.clear(); }
            if (mod.matched) continue;
            for (const auto& directory : directories) {
                // Only check expected file names in watched directories. Explicit
                // file selection below also supports renamed browser downloads.
                const auto path = QDir(directory).filePath(QFileInfo(mod.name).fileName());
                if (verify(path, mod.hash)) {
                    mod.localPath = QFileInfo(path).absoluteFilePath();
                    mod.matched = true;
                    break;
                }
            }
        }
        QJsonArray files, folders;
        bool complete = true;
        QStringList missing;
        for (const auto& directory : directories) folders.append(directory);
        for (const auto& mod : mods) {
            complete &= mod.matched;
            if (!mod.matched) missing.append(mod.name + "\n" + mod.websiteUrl);
            files.append(QJsonObject{ { "name", mod.name }, { "url", mod.websiteUrl }, { "hash", mod.hash },
                                      { "matched", mod.matched }, { "localPath", mod.localPath } });
        }
        const auto answer = interaction->inputWithContext(
            complete ? "All restricted files were verified. Type continue to install, or cancel."
                     : "Download the missing files, then enter a file/directory path or scan to check again:\n" + missing.join("\n"),
            { { "type", "restricted-downloads" }, { "files", files }, { "directories", folders },
              { "algorithm", algorithm }, { "complete", complete }, { "error", validationError } });
        if (!answer || answer->isEmpty()) { error = "Restricted file selection cancelled."; return false; }
        if (cancelled && cancelled()) { error = "Restricted download cancelled."; return false; }
        validationError.clear();
        const auto action = answer->trimmed();
        if (action.compare("cancel", Qt::CaseInsensitive) == 0) { error = "Restricted file selection cancelled."; return false; }
        if (action == "continue") {
            // Do not trust the UI's enabled state; files may have changed while
            // the confirmation was open.
            bool stillValid = complete;
            for (const auto& mod : mods) stillValid &= mod.matched && verify(mod.localPath, mod.hash);
            if (stillValid) return true;
            validationError = "Some files are missing or changed. Verify every file before continuing.";
            continue;
        }
        if (action == "scan") continue;
        const auto url = QUrl(action);
        const auto local = url.isLocalFile() ? url.toLocalFile() : action;
        if (QFileInfo(local).isDir()) {
            const auto path = QFileInfo(local).absoluteFilePath();
            if (!directories.contains(path)) directories.append(path);
        } else if (!matchFile(local)) {
            validationError = "The selected file is unreadable or its checksum does not match any missing file.";
        }
    }
}
