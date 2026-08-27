// SPDX-License-Identifier: GPL-3.0-only

#include "CurseForgeExternalTool.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>

namespace CurseForgeExternalTool {

QString resolveExecutable(const QString& configuredPath)
{
    const auto trimmed = configuredPath.trimmed();
    if (trimmed.isEmpty())
        return {};

    const QFileInfo configured(trimmed);
    if (configured.isFile() && configured.isExecutable())
        return configured.absoluteFilePath();

    return QStandardPaths::findExecutable(trimmed);
}

bool probe(const QString& configuredPath, QString* error, bool* supportsHeadless)
{
    if (supportsHeadless)
        *supportsHeadless = false;

    const auto executable = resolveExecutable(configuredPath);
    if (executable.isEmpty()) {
        if (error)
            *error = QCoreApplication::translate("CurseForgeExternalTool", "The configured file is not an executable.");
        return false;
    }

    QProcess process;
    process.setProgram(executable);
    process.setArguments({ "--probe" });
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start();
    if (!process.waitForStarted(3000)) {
        if (error)
            *error = process.errorString();
        return false;
    }
    if (!process.waitForFinished(5000)) {
        process.kill();
        process.waitForFinished(1000);
        if (error) {
            *error = QCoreApplication::translate("CurseForgeExternalTool", "The tool did not finish its protocol check within 5 seconds.");
        }
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (error) {
            const auto stderrOutput = QString::fromUtf8(process.readAllStandardError()).trimmed();
            *error = stderrOutput.isEmpty()
                         ? QCoreApplication::translate("CurseForgeExternalTool", "The tool protocol check failed with exit code %1.")
                               .arg(process.exitCode())
                         : stderrOutput;
        }
        return false;
    }

    QJsonParseError parseError;
    const auto response = QJsonDocument::fromJson(process.readAllStandardOutput().trimmed(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !response.isObject()) {
        if (error)
            *error = QCoreApplication::translate("CurseForgeExternalTool", "The tool returned an invalid JSON protocol response.");
        return false;
    }

    const auto object = response.object();
    bool supportsRestrictedDownloads = false;
    for (const auto& capability : object.value("capabilities").toArray()) {
        if (capability.toString() == "curseforgeRestrictedDownload") {
            supportsRestrictedDownloads = true;
            break;
        }
    }
    if (object.value("protocolVersion").toInt() != ProtocolVersion || !supportsRestrictedDownloads) {
        if (error) {
            *error = QCoreApplication::translate("CurseForgeExternalTool",
                                                 "The tool does not support CurseForge external download protocol version %1.")
                         .arg(ProtocolVersion);
        }
        return false;
    }

    if (supportsHeadless)
        *supportsHeadless = object.value("headless").toBool();
    return true;
}

}  // namespace CurseForgeExternalTool
