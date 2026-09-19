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

inline bool resolveHeadlessBlockedMods(QList<BlockedMod>& mods, const QString& algorithm, QString& error)
{
    if (!activeUserInteraction) { error = "No interaction channel is available for restricted downloads."; return false; }
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
            QObject::connect(&cancellation, &QTimer::timeout, &loop, [&] { if (activeInteractionCancellation && activeInteractionCancellation()) loop.quit(); });
            QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
            QObject::connect(&service, &CurseForgeDownloadPageService::completed, &loop, &QEventLoop::quit);
            QObject::connect(&service, &CurseForgeDownloadPageService::failed, &loop, [&](const QString& reason) { activeUserInteraction->status(reason); loop.quit(); });
            QObject::connect(&service, &CurseForgeDownloadPageService::downloadProgress, &loop,
                [](const QString& file, int index, int count, qint64 bytes, qint64) { activeUserInteraction->status(QString("%1 (%2/%3): %4 bytes").arg(file).arg(index).arg(count).arg(bytes)); });
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
    for (auto& mod : mods) {
        if (activeInteractionCancellation && activeInteractionCancellation()) { error = "Restricted download cancelled."; return false; }
        if (mod.matched) continue;
        for (;;) {
            const auto path = activeUserInteraction->input(QString("Provide the downloaded local file for %1\nURL: %2\n%3: %4")
                .arg(mod.name, mod.websiteUrl, algorithm, mod.hash), false);
            if (!path || path->isEmpty()) { error = "Restricted file selection cancelled."; return false; }
            const auto url = QUrl(*path);
            const auto local = url.isLocalFile() ? url.toLocalFile() : *path;
            if (verify(local, mod.hash)) { mod.localPath = QFileInfo(local).absoluteFilePath(); mod.matched = true; break; }
            activeUserInteraction->status("The file is unreadable or its checksum does not match. Choose another file or cancel.");
        }
    }
    return true;
}
