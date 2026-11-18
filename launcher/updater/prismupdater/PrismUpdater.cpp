// SPDX-FileCopyrightText: 2022 Rachel Powers <508861+Ryex@users.noreply.github.com>
//
// SPDX-License-Identifier: GPL-3.0-only

/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Rachel Powers <508861+Ryex@users.noreply.github.com>
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
 */

#include "PrismUpdater.h"
#include "WindowsUpdateManifest.h"
#include "AppImageUpdate.h"
#include "PortableUpdate.h"
#include "BuildConfig.h"
#include "ui/dialogs/ProgressDialog.h"

#include <cstdlib>
#include <iostream>

#include <QDebug>

#include <QAccessible>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QMessageBox>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProgressDialog>
#include <QElapsedTimer>
#include <QTimer>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#else
#include <signal.h>
#include <cerrno>
#endif
#include <memory>

#include <filesystem>
namespace fs = std::filesystem;

#include "DesktopServices.h"

#include "updater/prismupdater/UpdaterDialogs.h"

#include "FileSystem.h"
#include "LauncherDataPath.h"
#include "Json.h"
#include "StringUtils.h"

#include "net/Download.h"
#include "net/RawHeaderProxy.h"

#include "MMCZip.h"

/** output to the log file */
void appDebugOutput(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    static std::mutex loggerMutex;
    const std::lock_guard<std::mutex> lock(loggerMutex);  // synchronized, QFile logFile is not thread-safe

    QString out = qFormatLogMessage(type, context, msg);
    out += QChar::LineFeed;

    PrismUpdaterApp* app = static_cast<PrismUpdaterApp*>(QCoreApplication::instance());
    if (app->logFile) {
        app->logFile->write(out.toUtf8());
        app->logFile->flush();
    }
    if (app->logToConsole) {
        QTextStream(stderr) << out.toLocal8Bit();
        fflush(stderr);
    }
}

PrismUpdaterApp::PrismUpdaterApp(int& argc, char** argv) : QApplication(argc, argv)
{
    setOrganizationName(BuildConfig.LAUNCHER_NAME);
    setOrganizationDomain(BuildConfig.LAUNCHER_DOMAIN);
    setApplicationName(BuildConfig.LAUNCHER_NAME + "Updater");
    setApplicationVersion(BuildConfig.printableVersionString() + "\n" + BuildConfig.GIT_COMMIT);

    // Command line parsing
    QCommandLineParser parser;
    parser.setApplicationDescription(QObject::tr("An auto-updater for Prism Launcher"));

    parser.addOptions(
        { { { "d", "dir" }, tr("Use a custom path as application root (use '.' for current directory)."), tr("directory") },
          { { "V", "prism-version" },
            tr("Use this version as the installed launcher version. (provided because stdout can not be reliably captured on windows)"),
            tr("installed launcher version") },
          { { "I", "install-version" }, "Install a specific version.", tr("version name") },
          { { "U", "update-url" }, tr("Update from the specified repo."), tr("github repo url") },
          { { "c", "check-only" },
            tr("Only check if an update is needed. Exit status 100 if true, 0 if false (or non 0 if there was an error).") },
          { { "p", "pre-release" }, tr("Allow updating to pre-release releases") },
          { { "F", "force" }, tr("Force an update, even if one is not needed.") },
          { { "l", "list" }, tr("List available releases.") },
          { "debug", tr("Log debug to console.") },
          { "headless", tr("Run without dialogs or restarting the launcher. Ambiguous assets require --asset.") },
          { "asset", tr("Select an exact release asset name."), tr("name") },
          { "json-releases", tr("Print releases and compatible assets as JSON, without installing.") },
          { "wait-pid", tr("Wait for this frontend/backend process to exit before changing installation files."), tr("pid") },
          { "restart-entry", tr("UI to restart after an update: classic or neo."), tr("entry"), "classic" },
          { "portable-target", tr("Portable directory to update from an isolated updater runtime."), tr("directory") },
          { { "S", "select-ui" }, tr("Select the version to install with a GUI.") },
          { { "D", "allow-downgrade" }, tr("Allow the updater to downgrade to previous versions.") } });

    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(arguments());

    m_jsonReleases = parser.isSet("json-releases");
    m_headless = parser.isSet("headless") || parser.isSet("check-only") || parser.isSet("list") || m_jsonReleases;
    if (parser.isSet("wait-pid")) {
        bool valid;
        m_waitPid = parser.value("wait-pid").toLongLong(&valid);
        if (!valid || m_waitPid <= 0 || m_waitPid > 2147483647 || m_waitPid == QCoreApplication::applicationPid()) {
            m_status = Failed; return;
        }
    }
    m_checkOnly = parser.isSet("check-only");
    m_assetName = parser.value("asset");
    m_restartEntry = parser.value("restart-entry");
    if (m_restartEntry != "classic" && m_restartEntry != "neo") { m_status = Failed; return; }

    logToConsole = parser.isSet("debug") || m_headless;

    QString origCwdPath = QDir::currentPath();
    QString binPath = applicationDirPath();

    {  // find data director
       // Root path is used for updates and portable data
#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD) || defined(Q_OS_OPENBSD)
        QDir foo(FS::PathCombine(binPath, ".."));  // typically portable-root or /usr
        m_rootPath = foo.absolutePath();
#elif defined(Q_OS_WIN32)
        m_rootPath = binPath;
#elif defined(Q_OS_MAC)
        QDir foo(FS::PathCombine(binPath, "../.."));
        m_rootPath = foo.absolutePath();
        // on macOS, touch the root to force Finder to reload the .app metadata (and fix any icon change issues)
        FS::updateTimestamp(m_rootPath);
#endif
    }

    QString adjustedBy;
    // change folder
    QString dirParam = parser.value("dir");
    if (!dirParam.isEmpty()) {
        // the dir param. it makes prism launcher data path point to whatever the user specified
        // on command line
        adjustedBy = "Command line";
        m_dataPath = dirParam;
#ifndef Q_OS_MACOS
        if (QDir(FS::PathCombine(m_rootPath, "UserData")).exists()) {
            m_isPortable = true;
        }
        if (QFile::exists(FS::PathCombine(m_rootPath, "portable.txt"))) {
            m_isPortable = true;
        }
#endif
    } else if (auto dataDirEnv =
                   QProcessEnvironment::systemEnvironment().value(QString("%1_DATA_DIR").arg(BuildConfig.LAUNCHER_NAME.toUpper()));
               !dataDirEnv.isEmpty()) {
        adjustedBy = "System environment";
        m_dataPath = dataDirEnv;
#ifndef Q_OS_MACOS
        if (QFile::exists(FS::PathCombine(m_rootPath, "portable.txt"))) {
            m_isPortable = true;
        }
#endif
    } else {
        QDir foo(FS::PathCombine(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation), ".."));
        m_dataPath = foo.absolutePath();
        adjustedBy = "Persistent data path";

        QString configuredDataPath;
        QString dataPathError;
        if (!LauncherDataPath::configuredDirectory(m_rootPath, configuredDataPath, dataPathError)) {
            showFatalErrorMessage(tr("Invalid launcher data directory."), dataPathError);
            return;
        }
        if (!configuredDataPath.isEmpty()) {
            m_dataPath = configuredDataPath;
            adjustedBy = "Installer data path";
        }

#ifndef Q_OS_MACOS
        const bool forceSystemData = QFile::exists(FS::PathCombine(m_rootPath, ".luna-use-system-data"));
        if (configuredDataPath.isEmpty() && !forceSystemData) {
            if (auto portableUserData = FS::PathCombine(m_rootPath, "UserData"); QDir(portableUserData).exists()) {
                m_dataPath = portableUserData;
                adjustedBy = "Portable user data path";
                m_isPortable = true;
            } else if (QFile::exists(FS::PathCombine(m_rootPath, "portable.txt"))) {
                m_dataPath = m_rootPath;
                adjustedBy = "Portable data path";
                m_isPortable = true;
            }
        }
#endif
    }

#ifdef Q_OS_WIN
    m_isPortable = !QFile::exists(FS::PathCombine(m_rootPath, ".luna-use-system-data")) &&
        !QFile::exists(FS::PathCombine(m_rootPath, ".luna-data-dir")) &&
        (QDir(FS::PathCombine(m_rootPath, "UserData")).exists() || QFile::exists(FS::PathCombine(m_rootPath, "portable.txt")));
#endif
    m_updateLogPath = FS::PathCombine(m_dataPath, "logs", "prism_launcher_update.log");

    {  // setup logging
        FS::ensureFolderPathExists(FS::PathCombine(m_dataPath, "logs"));
        static const QString baseLogFile = BuildConfig.LAUNCHER_NAME + "Updater" + (m_checkOnly ? "-CheckOnly" : "") + "-%0.log";
        static const QString logBase = FS::PathCombine(m_dataPath, "logs", baseLogFile);

        if (FS::ensureFolderPathExists("logs")) {  // enough history to track both launches of the updater during a portable install
            FS::move(logBase.arg(1), logBase.arg(2));
            FS::move(logBase.arg(0), logBase.arg(1));
        }

        logFile = std::unique_ptr<QFile>(new QFile(logBase.arg(0)));
        if (!logFile->open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            showFatalErrorMessage(tr("The launcher data folder is not writable!"),
                                  tr("The updater couldn't create a log file - %1.\n"
                                     "\n"
                                     "Make sure you have write permissions to the data folder.\n"
                                     "(%2)\n"
                                     "\n"
                                     "The updater cannot continue until you fix this problem.")
                                      .arg(logFile->errorString())
                                      .arg(m_dataPath));
            return;
        }
        qInstallMessageHandler(appDebugOutput);

        qSetMessagePattern(
            "%{time process}"
            " "
            "%{if-debug}D%{endif}"
            "%{if-info}I%{endif}"
            "%{if-warning}W%{endif}"
            "%{if-critical}C%{endif}"
            "%{if-fatal}F%{endif}"
            " "
            "|"
            " "
            "%{if-category}[%{category}]: %{endif}"
            "%{message}");

        bool foundLoggingRules = false;

        auto logRulesFile = QStringLiteral("qtlogging.ini");
        auto logRulesPath = FS::PathCombine(m_dataPath, logRulesFile);

        qDebug() << "Testing" << logRulesPath << "...";
        foundLoggingRules = QFile::exists(logRulesPath);

        // search the dataPath()
        // seach app data standard path
        if (!foundLoggingRules && !isPortable() && dirParam.isEmpty()) {
            logRulesPath = QStandardPaths::locate(QStandardPaths::AppDataLocation, FS::PathCombine("..", logRulesFile));
            if (!logRulesPath.isEmpty()) {
                qDebug() << "Found" << logRulesPath << "...";
                foundLoggingRules = true;
            }
        }
        // seach root path
        if (!foundLoggingRules) {
            logRulesPath = FS::PathCombine(m_rootPath, logRulesFile);
            qDebug() << "Testing" << logRulesPath << "...";
            foundLoggingRules = QFile::exists(logRulesPath);
        }

        if (foundLoggingRules) {
            // load and set logging rules
            qDebug() << "Loading logging rules from:" << logRulesPath;
            QSettings loggingRules(logRulesPath, QSettings::IniFormat);
            loggingRules.beginGroup("Rules");
            QStringList rule_names = loggingRules.childKeys();
            QStringList rules;
            qDebug() << "Setting log rules:";
            for (auto rule_name : rule_names) {
                auto rule = QString("%1=%2").arg(rule_name).arg(loggingRules.value(rule_name).toString());
                rules.append(rule);
                qDebug() << "    " << rule;
            }
            auto rules_str = rules.join("\n");
            QLoggingCategory::setFilterRules(rules_str);
        }

        qDebug() << "<> Log initialized.";
    }

    {  // log debug program info
        qDebug() << qPrintable(BuildConfig.LAUNCHER_DISPLAYNAME + " Updater, " +
                               QString(BuildConfig.LAUNCHER_COPYRIGHT).replace("\n", ", "));
        qDebug() << "Version                    :" << BuildConfig.printableVersionString();
        qDebug() << "Git commit                 :" << BuildConfig.GIT_COMMIT;
        qDebug() << "Git refspec                :" << BuildConfig.GIT_REFSPEC;
        qDebug() << "Compiled for               :" << BuildConfig.systemID();
        qDebug() << "Compiled by                :" << BuildConfig.compilerID();
        qDebug() << "Build Artifact             :" << BuildConfig.BUILD_ARTIFACT;
        if (adjustedBy.size()) {
            qDebug() << "Data dir before adjustment :" << origCwdPath;
            qDebug() << "Data dir after adjustment  :" << m_dataPath;
            qDebug() << "Adjusted by                :" << adjustedBy;
        } else {
            qDebug() << "Data dir                   :" << QDir::currentPath();
        }
        qDebug() << "Work dir                   :" << QDir::currentPath();
        qDebug() << "Binary path                :" << binPath;
        qDebug() << "Application root path      :" << m_rootPath;
        qDebug() << "Portable install           :" << m_isPortable;
        qDebug() << "<> Paths set.";
    }

    {  // network
        m_network.reset(new QNetworkAccessManager());
        qDebug() << "Detecting proxy settings...";
        QNetworkProxy proxy = QNetworkProxy::applicationProxy();
        m_network->setProxy(proxy);
    }

#ifdef Q_OS_MACOS
    showFatalErrorMessage(tr("MacOS Not Supported"), tr("The updater does not support installations on MacOS"));
#endif

    m_appimagePath = AppImageUpdate::runningImage(binPath);
    m_isAppimage = !m_appimagePath.isEmpty();
    m_portableRoot = LinuxPortable::runningRoot(binPath);
    if (parser.isSet("portable-target")) {
        m_portableTarget = QDir::cleanPath(parser.value("portable-target"));
        if (m_portableRoot.isEmpty() || !QDir::isAbsolutePath(m_portableTarget) ||
            QFileInfo(m_portableTarget).canonicalFilePath() != m_portableTarget || m_portableTarget == m_portableRoot ||
            LinuxPortable::metadata(m_portableTarget).isEmpty()) {
            showFatalErrorMessage(tr("Unsupported portable installation"), tr("The portable update target or isolated runtime is invalid."));
            return;
        }
    }

    m_isFlatpak = DesktopServices::isFlatpak();

    QString prism_executable = FS::PathCombine(binPath, BuildConfig.LAUNCHER_APP_BINARY_NAME);
#if defined Q_OS_WIN32
    prism_executable.append(".exe");
#endif

    if (!QFileInfo(prism_executable).isFile()) {
        showFatalErrorMessage(tr("Unsupported Installation"), tr("The updater can not find the main executable."));
    }

    m_prismExecutable = prism_executable;

    auto prism_update_url = parser.value("update-url");
    if (prism_update_url.isEmpty())
        prism_update_url = BuildConfig.UPDATER_GITHUB_REPO;

    m_prismRepoUrl = QUrl::fromUserInput(prism_update_url);

    m_checkOnly = parser.isSet("check-only");
    m_forceUpdate = parser.isSet("force");
    m_printOnly = parser.isSet("list");
    auto user_version = parser.value("install-version");
    if (!user_version.isEmpty()) {
        m_userSelectedVersion = Version(WindowsUpdate::normalizedVersion(user_version));
    }
    m_selectUI = parser.isSet("select-ui");
    m_allowDowngrade = parser.isSet("allow-downgrade");

    auto version = parser.value("prism-version");
    if (!version.isEmpty()) {
        m_prismVersion = WindowsUpdate::normalizedVersion(version);
        if (version.contains('-')) {
            auto index = version.indexOf('-');
            m_prsimVersionChannel = version.mid(index + 1);
            version = version.left(index);
        } else {
            m_prsimVersionChannel = "stable";
        }
        auto version_parts = version.split('.');
        m_prismVersionMajor = version_parts.takeFirst().toInt();
        m_prismVersionMinor = version_parts.takeFirst().toInt();
        if (!version_parts.isEmpty())
            m_prismVersionPatch = version_parts.takeFirst().toInt();
        else
            m_prismVersionPatch = 0;
    }

    m_allowPreRelease = parser.isSet("pre-release");

    auto marker_file_path = QDir(m_rootPath).absoluteFilePath(".prism_launcher_updater_unpack.marker");
    auto marker_file = QFileInfo(marker_file_path);
    if (marker_file.exists()) {
        auto target_dir = QString(FS::read(marker_file_path)).trimmed();
        if (target_dir.isEmpty()) {
            qWarning() << "Empty updater marker file contains no install target. making best guess of parent dir";
            target_dir = QDir(m_rootPath).absoluteFilePath("..");
        }

        QMetaObject::invokeMethod(this, [this, target_dir]() { moveAndFinishUpdate(target_dir); }, Qt::QueuedConnection);

    } else {
        QMetaObject::invokeMethod(this, &PrismUpdaterApp::loadReleaseList, Qt::QueuedConnection);
    }
}

PrismUpdaterApp::~PrismUpdaterApp()
{
    qDebug() << "updater shutting down";
    // Shut down logger by setting the logger function to nothing
    qInstallMessageHandler(nullptr);
}

void PrismUpdaterApp::fail(const QString& reason)
{
    qCritical() << qPrintable(reason);
    m_status = Failed;
    exit(1);
}

void PrismUpdaterApp::abort(const QString& reason)
{
    qCritical() << qPrintable(reason);
    m_status = Aborted;
    exit(2);
}

void PrismUpdaterApp::showFatalErrorMessage(const QString& title, const QString& content)
{
    m_status = Failed;
    if (!m_checkOnly && !m_jsonReleases && !m_dataPath.isEmpty()) {
        QFile marker(QDir(m_dataPath).filePath(".prism_launcher_update.fail"));
        if (marker.open(QIODevice::WriteOnly)) marker.write((title + ": " + content).toUtf8());
        logUpdate(title + ": " + content);
    }
    if (m_headless) {
        qCritical().noquote() << title << ":" << content;
        QMetaObject::invokeMethod(this, [this]() { exit(1); }, Qt::QueuedConnection);
        return;
    }
    auto msgBox = new QMessageBox();
    msgBox->setWindowTitle(title);
    msgBox->setText(content);
    msgBox->setStandardButtons(QMessageBox::Ok);
    msgBox->setDefaultButton(QMessageBox::Ok);
    msgBox->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextBrowserInteraction);
    msgBox->setIcon(QMessageBox::Critical);
    msgBox->setMinimumWidth(460);
    msgBox->adjustSize();
    msgBox->exec();
    exit(1);
}

void PrismUpdaterApp::run()
{
    if (m_status == Failed) return exit(1);
    qDebug() << "found" << m_releases.length() << "releases on github";
    qDebug() << "loading exe at" << m_prismExecutable;

    if (m_jsonReleases) {
        QJsonArray result;
        for (const auto& release : m_releases) {
            if (release.draft || (release.prerelease && !m_allowPreRelease)) continue;
            QJsonArray assets;
            for (const auto& asset : validReleaseArtifacts(release)) assets.append(QJsonObject{ { "name", asset.name }, { "url", asset.browser_download_url }, { "size", asset.size } });
            if (assets.isEmpty()) continue;
            result.append(QJsonObject{ { "versionTag", release.tag_name }, { "name", release.name }, { "notes", release.body },
                { "publishedAt", release.published_at.toString(Qt::ISODate) }, { "prerelease", release.prerelease }, { "assets", assets } });
        }
        if (!m_manifestError.isEmpty()) return showFatalErrorMessage(tr("Update check failed"), m_manifestError);
        QTextStream out(stdout); out << QJsonDocument(result).toJson(QJsonDocument::Compact) << '\n'; out.flush();
        m_status = Succeeded; return exit(0);
    }
    if (m_printOnly) {
        printReleases();
        m_status = Succeeded;
        return exit(0);
    }

    if (m_prismVersion.isEmpty() && !loadPrismVersionFromExe(m_prismExecutable)) {
        m_prismVersion = BuildConfig.printableVersionString();
        m_prismVersionMajor = BuildConfig.VERSION_MAJOR;
        m_prismVersionMinor = BuildConfig.VERSION_MINOR;
        m_prismVersionPatch = BuildConfig.VERSION_PATCH;
        m_prsimVersionChannel = BuildConfig.VERSION_CHANNEL;
        m_prismGitCommit = BuildConfig.GIT_COMMIT;
    }
    m_status = Succeeded;

    qDebug() << "Executable reports as:" << m_prismBinaryName << "version:" << m_prismVersion;
    qDebug() << "Version major:" << m_prismVersionMajor;
    qDebug() << "Version minor:" << m_prismVersionMinor;
    qDebug() << "Version minor:" << m_prismVersionPatch;
    qDebug() << "Version channel:" << m_prsimVersionChannel;
    qDebug() << "Git Commit:" << m_prismGitCommit;

    auto latest = getLatestRelease();
    if (!m_manifestError.isEmpty()) return showFatalErrorMessage(tr("Update check failed"), m_manifestError);
    qDebug() << "Latest release" << latest.version;
    auto need_update = needUpdate(latest);

    if (m_checkOnly) {
        if (need_update) {
            QTextStream stdOutStream(stdout);
            stdOutStream << "Name: " << latest.name << "\n";
            stdOutStream << "Version: " << latest.tag_name << "\n";
            stdOutStream << "TimeStamp: " << latest.created_at.toString(Qt::ISODate) << "\n";
            stdOutStream << latest.body << "\n";
            stdOutStream.flush();

            return exit(100);
        } else {
            return exit(0);
        }
    }

    if (m_isFlatpak) {
        showFatalErrorMessage(tr("Updating flatpack not supported"), tr("Actions outside of checking if an update is available are not "
                                                                        "supported when running the flatpak version of Prism Launcher."));
        return;
    }
    if (need_update || m_forceUpdate || !m_userSelectedVersion.isEmpty()) {
        GitHubRelease update_release = latest;
        if (!m_userSelectedVersion.isEmpty()) {
            bool found = false;
            for (auto rls : m_releases) {
                if (rls.version == m_userSelectedVersion) {
                    found = true;
                    update_release = rls;
                    break;
                }
            }
            if (!found) {
                showFatalErrorMessage(
                    "No release for version!",
                    QString("Can not find a github release for specified version %1").arg(m_userSelectedVersion.toString()));
                return;
            }
        } else if (m_selectUI && !m_headless) {
            update_release = selectRelease();
            if (!update_release.isValid()) {
                showFatalErrorMessage("No version selected.", "No version was selected.");
                return;
            }
        }

        return performUpdate(update_release);
    }

    exit(0);
}

void PrismUpdaterApp::moveAndFinishUpdate(QDir target)
{
    logUpdate("Finishing update process");

    logUpdate("Waiting 2 seconds for resources to free");
    this->thread()->sleep(2);

    auto manifest_path = FS::PathCombine(m_rootPath, "manifest.txt");
    QFileInfo manifest(manifest_path);

    auto app_dir = QDir(m_rootPath);

    QStringList file_list;
    if (manifest.isFile()) {
        // load manifest from file
        logUpdate(tr("Reading manifest from %1").arg(manifest.absoluteFilePath()));
        try {
            auto contents = QString::fromUtf8(FS::read(manifest.absoluteFilePath()));
            auto files = contents.split('\n');
            for (auto file : files) {
                file_list.append(file.trimmed());
            }
        } catch (FS::FileSystemException&) {
        }
    }

    if (file_list.isEmpty()) {
        logUpdate(tr("Manifest empty, making best guess of the directory contents of %1").arg(m_rootPath));
        auto entries = target.entryInfoList(QDir::NoDotAndDotDot | QDir::Files | QDir::Dirs);
        for (auto entry : entries) {
            file_list.append(entry.fileName());
        }
    }
    logUpdate(tr("Installing the following to %1 :\n %2").arg(target.absolutePath()).arg(file_list.join(",\n  ")));

    bool error = false;

    QProgressDialog progress(tr("Installing from %1").arg(m_rootPath), "", 0, file_list.length());
    progress.setCancelButton(nullptr);
    progress.setMinimumWidth(400);
    progress.adjustSize();
    if (!m_headless) progress.show();
    else progress.setMinimumDuration(INT_MAX);
    QCoreApplication::processEvents();

    logUpdate(tr("Installing from %1").arg(m_rootPath));

    auto copy = [this, app_dir, target](QString to_install_file) {
        auto rel_path = app_dir.relativeFilePath(to_install_file);
        auto install_path = FS::PathCombine(target.absolutePath(), rel_path);
        logUpdate(tr("Installing %1 from %2").arg(install_path).arg(to_install_file));
        FS::ensureFilePathExists(install_path);
        auto result = FS::copy(to_install_file, install_path).overwrite(true)();
        if (!result) {
            logUpdate(tr("Failed copy %1 to %2").arg(to_install_file).arg(install_path));
            return true;
        }
        return false;
    };

    int i = 0;
    for (auto glob : file_list) {
        QDirIterator iter(m_rootPath, QStringList({ glob }), QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
        progress.setValue(i);
        QCoreApplication::processEvents();
        if (!iter.hasNext() && !glob.isEmpty()) {
            if (auto file_info = QFileInfo(FS::PathCombine(m_rootPath, glob)); file_info.exists()) {
                error |= copy(file_info.absoluteFilePath());
            } else {
                logUpdate(tr("File doesn't exist, ignoring: %1").arg(FS::PathCombine(m_rootPath, glob)));
            }
        } else {
            while (iter.hasNext()) {
                error |= copy(iter.next());
            }
        }
        i++;
    }
    progress.setValue(i);
    QCoreApplication::processEvents();

    if (error) {
        logUpdate(tr("There were errors installing the update."));
        auto fail_marker = FS::PathCombine(m_dataPath, ".prism_launcher_update.fail");
        FS::copy(m_updateLogPath, fail_marker).overwrite(true)();
    } else {
        logUpdate(tr("Update succeed."));
        auto success_marker = FS::PathCombine(m_dataPath, ".lunalauncher_update.success");
        FS::copy(m_updateLogPath, success_marker).overwrite(true)();
    }
    auto update_lock_path = FS::PathCombine(m_dataPath, ".prism_launcher_update.lock");
    FS::deletePath(update_lock_path);

    QProcess proc;
    auto app_exe_name = BuildConfig.LAUNCHER_APP_BINARY_NAME;
#if defined Q_OS_WIN32
    app_exe_name.append(".exe");

    auto env = QProcessEnvironment::systemEnvironment();
    env.insert("__COMPAT_LAYER", "RUNASINVOKER");
    proc.setProcessEnvironment(env);
#else
    app_exe_name.prepend("bin/");
#endif

    auto app_exe_path = target.absoluteFilePath(app_exe_name);
    if (!m_headless) proc.startDetached(app_exe_path);

    exit(error ? 1 : 0);
}

void PrismUpdaterApp::printReleases()
{
    for (auto release : m_releases) {
        std::cout << release.name.toStdString() << " Version: " << release.tag_name.toStdString() << std::endl;
    }
}

QList<GitHubRelease> PrismUpdaterApp::nonDraftReleases()
{
    QList<GitHubRelease> nonDraft;
    for (auto rls : m_releases) {
        if (rls.isValid() && !rls.draft && (!rls.prerelease || m_allowPreRelease) && !validReleaseArtifacts(rls).isEmpty())
            nonDraft.append(rls);
    }
    return nonDraft;
}

QList<GitHubRelease> PrismUpdaterApp::newerReleases()
{
    QList<GitHubRelease> newer;
    for (auto rls : nonDraftReleases()) {
        if (needUpdate(rls))
            newer.append(rls);
    }
    return newer;
}

GitHubRelease PrismUpdaterApp::selectRelease()
{
    QList<GitHubRelease> releases;

    if (m_allowDowngrade) {
        releases = nonDraftReleases();
    } else {
        releases = newerReleases();
    }

    if (releases.isEmpty())
        return {};

    SelectReleaseDialog dlg(Version(m_prismVersion), releases);
    auto result = dlg.exec();

    if (result == QDialog::Rejected) {
        return {};
    }
    GitHubRelease release = dlg.selectedRelease();

    return release;
}

QJsonObject PrismUpdaterApp::windowsManifest(const GitHubRelease& release)
{
    if (m_windowsManifests.contains(release.tag_name)) return m_windowsManifests.value(release.tag_name);
    QJsonObject manifest;
    const auto arch = QSysInfo::buildCpuArchitecture().contains("arm64") ? "arm64" : "x64";
    for (const auto& asset : release.assets) {
        if (asset.name != QString("windows-%1-update.json").arg(arch) || asset.size <= 0 || asset.size > 65536) continue;
        const auto file = downloadAsset(asset);
        QFile input(file.absoluteFilePath());
        if (file.exists() && input.open(QIODevice::ReadOnly)) manifest = QJsonDocument::fromJson(input.read(65537)).object();
        if (manifest.isEmpty()) m_manifestError = tr("Could not download or parse the Windows update manifest for %1.").arg(release.tag_name);
        break;
    }
    m_windowsManifests.insert(release.tag_name, manifest);
    return manifest;
}

QJsonObject PrismUpdaterApp::appImageManifest(const GitHubRelease& release)
{
    if (m_appImageManifests.contains(release.tag_name)) return m_appImageManifests.value(release.tag_name);
    QJsonObject manifest;
    const auto arch = QSysInfo::buildCpuArchitecture() == "x86_64" ? "x64" : "arm64";
    int matches = 0;
    for (const auto& asset : release.assets) {
        if (asset.name != QString("linux-%1-appimage-update.json").arg(arch)) continue;
        ++matches;
        if (asset.size <= 0 || asset.size > 65536) break;
        const auto file = downloadAsset(asset);
        QFile input(file.absoluteFilePath());
        if (file.exists() && input.open(QIODevice::ReadOnly)) manifest = QJsonDocument::fromJson(input.read(65537)).object();
    }
    if (matches && (matches != 1 || AppImageUpdate::artifact(manifest, arch, false).isEmpty())) {
        m_manifestError = tr("Invalid AppImage update manifest for %1.").arg(release.tag_name);
        manifest = {};
    }
    m_appImageManifests.insert(release.tag_name, manifest);
    return manifest;
}

QJsonObject PrismUpdaterApp::updateManifest(const GitHubRelease& release)
{
#ifdef Q_OS_WIN
    return windowsManifest(release);
#else
    return m_isAppimage ? appImageManifest(release) : !m_portableRoot.isEmpty() ? portableManifest(release) : QJsonObject{};
#endif
}

QJsonObject PrismUpdaterApp::portableManifest(const GitHubRelease& release)
{
    if (m_portableManifests.contains(release.tag_name)) return m_portableManifests.value(release.tag_name);
    QJsonObject manifest; int matches = 0;
    for (const auto& asset : release.assets) {
        if (asset.name != QString("linux-%1-portable-update.json").arg(LinuxPortable::architecture())) continue;
        ++matches;
        if (asset.size <= 0 || asset.size > 65536) break;
        const auto file = downloadAsset(asset);
        QFile input(file.absoluteFilePath());
        if (file.exists() && input.open(QIODevice::ReadOnly)) manifest = QJsonDocument::fromJson(input.read(65537)).object();
    }
    // A valid manifest with no compatible glibc artifact is simply unavailable
    // on this machine; do not offer it as an installable update.
    if (matches && (matches != 1 || !ReleaseUpdate::validManifest(manifest))) {
        m_manifestError = tr("Invalid portable Linux update manifest for %1.").arg(release.tag_name);
        manifest = {};
    }
    m_portableManifests.insert(release.tag_name, manifest);
    return manifest;
}

QList<GitHubReleaseAsset> PrismUpdaterApp::validReleaseArtifacts(const GitHubRelease& release)
{
    QList<GitHubReleaseAsset> valid;
#ifdef Q_OS_WIN
    const auto arch = QSysInfo::buildCpuArchitecture().contains("arm64") ? "arm64" : "x64";
    const auto item = WindowsUpdate::artifact(windowsManifest(release), arch,
        QFileInfo::exists(QDir(root()).filePath("luna-neo-ui.exe")));
    if (item.isEmpty()) return valid;
    for (const auto& asset : release.assets)
        if (asset.name == item.value("asset").toString() && asset.size == item.value("size").toInteger()) valid.append(asset);
    return valid;
#else
    if (!m_isAppimage && m_portableRoot.isEmpty()) return valid;
    const auto arch = QSysInfo::buildCpuArchitecture() == "x86_64" ? "x64" : "arm64";
    const bool neo = QFileInfo::exists(QDir(root()).filePath("bin/luna-neo-ui"));
    const auto item = m_isAppimage ? AppImageUpdate::artifact(appImageManifest(release), arch, neo)
                                 : LinuxPortable::artifact(portableManifest(release), arch, neo);
    if (item.isEmpty()) return valid;
    for (const auto& asset : release.assets)
        if (asset.name == item.value("asset").toString() && asset.size == item.value("size").toInteger()) valid.append(asset);
    return valid;
#endif
}

GitHubReleaseAsset PrismUpdaterApp::selectAsset(const QList<GitHubReleaseAsset>& assets)
{
    if (!m_assetName.isEmpty()) {
        for (const auto& asset : assets) if (asset.name == m_assetName) return asset;
        return {};
    }
    if (m_headless) return {};
    SelectReleaseAssetDialog dlg(assets);
    auto result = dlg.exec();

    if (result == QDialog::Rejected) {
        return {};
    }

    GitHubReleaseAsset asset = dlg.selectedAsset();
    return asset;
}

void PrismUpdaterApp::performUpdate(const GitHubRelease& release)
{
    if (!release.isValid() || release.draft || (release.prerelease && !m_allowPreRelease))
        return showFatalErrorMessage(tr("Unsupported release"), tr("The selected release is not available on this update channel."));
    if (!m_allowDowngrade && !needUpdate(release) && !m_forceUpdate)
        return showFatalErrorMessage(tr("Update refused"), tr("The selected package is not newer. Explicitly allow downgrade to install it."));
    m_install_release = release;
    qDebug() << "Updating to" << release.tag_name;
    auto valid_assets = validReleaseArtifacts(release);
    qDebug() << "valid release assets:" << valid_assets;

    GitHubReleaseAsset selected_asset;
    if (valid_assets.isEmpty()) {
        return showFatalErrorMessage(
            tr("No Valid Release Assets"),
            tr("Github release %1 has no valid assets for this platform: %2")
                .arg(release.tag_name)
                .arg(tr("%1 portable: %2").arg(BuildConfig.BUILD_ARTIFACT).arg(m_isPortable ? tr("yes") : tr("no"))));
    } else if (valid_assets.length() > 1 || !m_assetName.isEmpty()) {
        selected_asset = selectAsset(valid_assets);
    } else {
        selected_asset = valid_assets.takeFirst();
    }

    if (!selected_asset.isValid()) {
        return showFatalErrorMessage(tr("No version selected."), tr("No version was selected."));
    }

    qDebug() << "will install" << selected_asset;
    if (!m_portableRoot.isEmpty() && m_portableTarget.isEmpty()) {
        startPortableWorker(release, selected_asset.name);
        return;
    }
    clearUpdateLog();
    FS::deletePath(QDir(m_dataPath).filePath(".lunalauncher_update.success"));
    FS::deletePath(QDir(m_dataPath).filePath(".prism_launcher_update.fail"));
    logUpdate(tr("Updating from %1 to %2; downloading %3").arg(m_prismVersion, release.tag_name, selected_asset.name));
    FS::write(QDir(m_dataPath).filePath(".prism_launcher_update.changelog"), release.body.toUtf8());
    auto file = downloadAsset(selected_asset);

    if (!file.exists()) {
        return showFatalErrorMessage(tr("Failed to Download"), tr("Failed to download the selected asset."));
    }

#ifdef Q_OS_WIN
    const auto manifest = windowsManifest(release);
    const auto arch = QSysInfo::buildCpuArchitecture().contains("arm64") ? "arm64" : "x64";
    const auto item = WindowsUpdate::artifact(manifest, arch, QFileInfo::exists(QDir(root()).filePath("luna-neo-ui.exe")));
    if (item.value("asset").toString() != selected_asset.name || !WindowsUpdate::verifyPackage(file.absoluteFilePath(), item))
        return showFatalErrorMessage(tr("Update verification failed"), tr("The Windows package size or SHA-256 does not match the release manifest. No installation files were changed."));
#endif

    if (m_isAppimage) {
        const auto arch = QSysInfo::buildCpuArchitecture() == "x86_64" ? "x64" : "arm64";
        const auto item = AppImageUpdate::artifact(appImageManifest(release), arch,
            QFileInfo::exists(QDir(root()).filePath("bin/luna-neo-ui")));
        if (item.value("asset").toString() != selected_asset.name || !WindowsUpdate::verifyPackage(file.absoluteFilePath(), item) ||
            !AppImageUpdate::validImage(file.absoluteFilePath(), arch))
            return showFatalErrorMessage(tr("Update verification failed"), tr("AppImage size, SHA-256 or architecture does not match the release manifest. Original kept."));
    }
    if (!m_portableRoot.isEmpty()) {
        const auto item = LinuxPortable::artifact(portableManifest(release), LinuxPortable::architecture(),
            QFileInfo::exists(QDir(root()).filePath("bin/luna-neo-ui")));
        if (item.value("asset").toString() != selected_asset.name || !ReleaseUpdate::verifyPackage(file.absoluteFilePath(), item))
            return showFatalErrorMessage(tr("Update verification failed"), tr("The portable package checksum, size or system requirements do not match."));
    }

    performInstall(file);
}

QFileInfo PrismUpdaterApp::downloadAsset(const GitHubReleaseAsset& asset)
{
    if (!m_downloadDirectory) m_downloadDirectory = std::make_unique<QTemporaryDir>(QDir::tempPath() + "/luna-update-XXXXXX");
    if (!m_downloadDirectory->isValid()) return {};
    auto temp_dir = m_downloadDirectory->path();
    auto file_url = QUrl(asset.browser_download_url);
    if (file_url.scheme() != "https" || asset.name.contains('/') || asset.name.contains('\\') || asset.name.contains(':')) return {};
    auto out_file_path = FS::PathCombine(temp_dir, asset.name);

    qDebug() << "downloading" << file_url << "to" << out_file_path;
    auto download = Net::Download::makeFile(file_url, out_file_path);
    download->setNetwork(m_network);
    if (m_headless) {
        QEventLoop loop;
        connect(download.get(), &Task::finished, &loop, &QEventLoop::quit);
        download->start();
        if (!download->isFinished()) loop.exec();
    } else {
        auto progress_dialog = ProgressDialog();
        progress_dialog.adjustSize();
        progress_dialog.execWithTask(download.get());
    }
    if (!download->wasSuccessful()) return {};

    qDebug() << "download complete";

    QFileInfo out_file(out_file_path);
    return out_file;
}

bool PrismUpdaterApp::startPortableWorker(const GitHubRelease& release, const QString& asset)
{
#ifdef Q_OS_LINUX
    QProgressDialog progress(tr("Preparing isolated updater runtime…"), QString(), 0, 100);
    progress.setCancelButton(nullptr); progress.setMinimumWidth(520);
    if (!m_headless) progress.show(); else progress.setMinimumDuration(INT_MAX);
    QTemporaryDir worker(QDir::tempPath() + "/luna-portable-worker-XXXXXX");
    QString error;
    if (!worker.isValid() || !PortableUpdate::copyProgram(m_portableRoot, worker.path(), error,
        [&](const QString& label, qint64 done, qint64 total) {
            progress.setLabelText(label); progress.setValue(total ? int(done * 100 / total) : 0); QCoreApplication::processEvents();
        })) {
        showFatalErrorMessage(tr("Cannot prepare portable updater"), error); return false;
    }
    auto env = AppImageUpdate::cleanEnvironment(); env.insert("LUNA_LAUNCHER_DATA", QDir(m_dataPath).absolutePath());
    QStringList arguments{"--luna-entry", "updater", "--portable-target", m_portableRoot,
        "--dir", QDir(m_dataPath).absolutePath(), "--prism-version", m_prismVersion,
        "--install-version", release.tag_name, "--asset", asset, "--restart-entry", m_restartEntry,
        "--update-url", m_prismRepoUrl.toString()};
    if (m_waitPid > 0) arguments.append({"--wait-pid", QString::number(m_waitPid)});
    if (m_allowPreRelease) arguments.append("--pre-release");
    if (m_allowDowngrade) arguments.append("--allow-downgrade");
    if (m_forceUpdate) arguments.append("--force");
    if (m_headless) arguments.append("--headless");
    QProcess process; process.setProcessEnvironment(env); process.setProgram(worker.filePath("AppRun"));
    process.setArguments(arguments); process.setWorkingDirectory(worker.path());
    if (!process.startDetached()) { showFatalErrorMessage(tr("Cannot start portable updater"), process.errorString()); return false; }
    worker.setAutoRemove(false); // Worker owns this isolated runtime until it exits; /tmp cleanup can reclaim it later.
    logUpdate("Isolated portable updater runtime: " + worker.path());
    m_status = Succeeded; exit(0); return true;
#else
    return false;
#endif
}

void PrismUpdaterApp::installPortable(const QFileInfo& file)
{
#ifdef Q_OS_LINUX
    if (m_portableTarget.isEmpty()) return showFatalErrorMessage(tr("Invalid portable update"), tr("The updater must run from an isolated runtime."));
    const auto manifest = portableManifest(m_install_release);
    const auto item = LinuxPortable::artifact(manifest, LinuxPortable::architecture(), QFileInfo::exists(QDir(root()).filePath("bin/luna-neo-ui")));
    const auto expected = ReleaseUpdate::normalizedVersion(manifest.value("version").toString());
    QProgressDialog progress(tr("Preparing portable update…"), QString(), 0, 100);
    progress.setCancelButton(nullptr); progress.setMinimumWidth(520);
    if (!m_headless) progress.show(); else progress.setMinimumDuration(INT_MAX);
    auto environment = AppImageUpdate::cleanEnvironment();
    environment.insert("LUNA_LAUNCHER_DATA", QDir(m_dataPath).absolutePath());
    const auto health = [&](const QString& directory) {
        progress.setLabelText(tr("Verifying portable launcher…")); QCoreApplication::processEvents();
        QProcess probe; auto env = environment; env.insert("QT_QPA_PLATFORM", "offscreen"); probe.setProcessEnvironment(env);
        QEventLoop loop; QTimer timeout; timeout.setSingleShot(true);
        connect(&probe, &QProcess::finished, &loop, &QEventLoop::quit);
        connect(&probe, &QProcess::errorOccurred, &loop, &QEventLoop::quit);
        connect(&timeout, &QTimer::timeout, &loop, [&] { probe.kill(); loop.quit(); });
        probe.start(QDir(directory).filePath("AppRun"), {"--luna-entry", "classic", "--version"});
        timeout.start(30000); loop.exec();
        if (probe.state() != QProcess::NotRunning) { probe.kill(); probe.waitForFinished(5000); }
        const auto output = QString::fromUtf8(probe.readAllStandardOutput());
        const auto match = QRegularExpression("(?:^|\\n)LunaLauncher ([^\\s]+)").match(output);
        if (probe.exitCode() || !match.hasMatch()) logUpdate("Portable health check: " + QString::fromUtf8(probe.readAllStandardError()));
        return probe.exitStatus() == QProcess::NormalExit && probe.exitCode() == 0 && match.hasMatch() &&
            ReleaseUpdate::normalizedVersion(match.captured(1)) == expected;
    };
    const auto result = PortableUpdate::replace(m_portableTarget, file.absoluteFilePath(), item, manifest.value("version").toString(), m_dataPath, health,
        [&](const QString& label, qint64 done, qint64 total) {
            progress.setLabelText(label);
            if (total > 0) { progress.setRange(0, 100); progress.setValue(int(done * 100 / total)); }
            else progress.setRange(0, 0);
            QCoreApplication::processEvents();
        });
    if (!result.success) return showFatalErrorMessage(tr("Portable update failed"), result.error);
    logUpdate("Portable installation updated; program backup: " + result.backup);
    if (!result.conflicts.isEmpty()) logUpdate("Conflicting unmanaged files retained in backup: " + result.conflicts.join(", "));
    FS::write(QDir(m_dataPath).filePath(".lunalauncher_update.success"), result.backup.toUtf8());
    if (!m_headless) {
        QProcess restart; restart.setProcessEnvironment(environment); restart.setWorkingDirectory(m_portableTarget);
        restart.setProgram(QDir(m_portableTarget).filePath("AppRun")); restart.setArguments({"--luna-entry", m_restartEntry});
        if (!restart.startDetached()) return showFatalErrorMessage(tr("Restart failed"), tr("The update succeeded; start AppRun manually. Backup: %1").arg(result.backup));
    }
    m_status = Succeeded; exit(0);
#endif
}

void PrismUpdaterApp::installAppImage(const QFileInfo& file)
{
#ifdef Q_OS_LINUX
    QProgressDialog progress(tr("Preparing AppImage update…"), QString(), 0, 100);
    progress.setCancelButton(nullptr); progress.setMinimumWidth(480);
    if (!m_headless) progress.show(); else progress.setMinimumDuration(INT_MAX);
    const auto manifest = appImageManifest(m_install_release);
    const auto arch = QSysInfo::buildCpuArchitecture() == "x86_64" ? "x64" : "arm64";
    const auto item = AppImageUpdate::artifact(manifest, arch, QFileInfo::exists(QDir(root()).filePath("bin/luna-neo-ui")));
    auto environment = AppImageUpdate::cleanEnvironment();
    const auto expected = WindowsUpdate::normalizedVersion(manifest.value("version").toString());
    const auto health = [&](const QString& path) {
        progress.setLabelText(tr("Verifying new AppImage…")); QCoreApplication::processEvents();
        QProcess probe;
        auto env = environment; env.insert("QT_QPA_PLATFORM", "offscreen");
        probe.setProcessEnvironment(env);
        QEventLoop loop; QTimer timeout; timeout.setSingleShot(true);
        connect(&probe, &QProcess::finished, &loop, &QEventLoop::quit);
        connect(&probe, &QProcess::errorOccurred, &loop, &QEventLoop::quit);
        connect(&timeout, &QTimer::timeout, &loop, [&] { probe.kill(); loop.quit(); });
        probe.start(path, { "--appimage-extract-and-run", "--luna-entry", "classic", "--version" });
        timeout.start(30000); loop.exec();
        if (probe.state() != QProcess::NotRunning) { probe.kill(); probe.waitForFinished(5000); }
        const auto output = QString::fromUtf8(probe.readAllStandardOutput());
        const auto version = QRegularExpression("(?:^|\\n)LunaLauncher ([^\\s]+)").match(output);
        return probe.exitStatus() == QProcess::NormalExit && probe.exitCode() == 0 && version.hasMatch() &&
            WindowsUpdate::normalizedVersion(version.captured(1)) == expected;
    };
    logUpdate(tr("Staging and atomically replacing %1").arg(m_appimagePath));
    const auto result = AppImageUpdate::replace(m_appimagePath, file.absoluteFilePath(), item, health, [&](qint64 done, qint64 total) {
        progress.setLabelText(tr("Copying AppImage: %1 / %2 MiB").arg(done / 1048576).arg(total / 1048576));
        progress.setValue(total ? int(done * 100 / total) : 0); QCoreApplication::processEvents();
    });
    if (!result.success) return showFatalErrorMessage(tr("AppImage update failed"), result.error);
    logUpdate(tr("AppImage updated; backup: %1").arg(result.backup));
    FS::write(QDir(m_dataPath).filePath(".lunalauncher_update.success"), result.backup.toUtf8());
    if (!m_headless) {
        progress.setLabelText(tr("Restarting launcher…")); QCoreApplication::processEvents();
        environment.insert("LUNA_LAUNCHER_DATA", QDir(m_dataPath).absolutePath());
        QProcess restart; restart.setProcessEnvironment(environment);
        restart.setWorkingDirectory(QFileInfo(m_appimagePath).absolutePath());
        restart.setProgram(m_appimagePath); restart.setArguments({ "--luna-entry", m_restartEntry });
        if (!restart.startDetached()) return showFatalErrorMessage(tr("Restart failed"), tr("The update succeeded. Open the AppImage manually. Backup: %1").arg(result.backup));
    }
    m_status = Succeeded; exit(0);
#endif
}

void PrismUpdaterApp::clearUpdateLog()
{
    FS::deletePath(m_updateLogPath);
}

void PrismUpdaterApp::logUpdate(const QString& msg)
{
    qDebug() << qUtf8Printable(msg);
    FS::append(m_updateLogPath, QStringLiteral("%1\n").arg(msg).toUtf8());
}

std::tuple<QDateTime, QString, QString, QString, QString> read_lock_File(const QString& path)
{
    auto contents = QString(FS::read(path));
    auto lines = contents.split('\n');

    QDateTime timestamp;
    QString from, to, target, data_path;
    for (auto line : lines) {
        auto index = line.indexOf("=");
        if (index < 0)
            continue;
        auto left = line.left(index);
        auto right = line.mid(index + 1);
        if (left.toLower() == "timestamp") {
            timestamp = QDateTime::fromString(right, Qt::ISODate);
        } else if (left.toLower() == "from") {
            from = right;
        } else if (left.toLower() == "to") {
            to = right;
        } else if (left.toLower() == "target") {
            target = right;
        } else if (left.toLower() == "data_path") {
            data_path = right;
        }
    }
    return std::make_tuple(timestamp, from, to, target, data_path);
}

bool write_lock_file(const QString& path, QDateTime timestamp, QString from, QString to, QString target, QString data_path)
{
    try {
        FS::write(path, QStringLiteral("TIMESTAMP=%1\nFROM=%2\nTO=%3\nTARGET=%4\nDATA_PATH=%5\n")
                            .arg(timestamp.toString(Qt::ISODate))
                            .arg(from)
                            .arg(to)
                            .arg(target)
                            .arg(data_path)
                            .toUtf8());
    } catch (FS::FileSystemException& err) {
        qWarning() << "Error writing lockfile:" << err.what() << "\n" << err.cause();
        return false;
    }
    return true;
}

void PrismUpdaterApp::performInstall(QFileInfo file)
{
#ifdef Q_OS_WIN
    // The verified SFX is the update worker for both installed and portable copies.
    // It waits for this exact installation to exit and performs a rollback transaction.
    QStringList args{ "--update", "--install-dir", QDir(m_rootPath).absolutePath(),
        "--data-root", QDir(m_dataPath).absolutePath(), "--restart-entry", m_restartEntry };
    QProcess worker;
    worker.setProgram(file.absoluteFilePath()); worker.setArguments(args);
    worker.setWorkingDirectory(m_downloadDirectory->path());
    if (!worker.startDetached()) return showFatalErrorMessage(tr("Update failed"), worker.errorString());
    m_downloadDirectory->setAutoRemove(false); // Worker executable must survive updater exit.
    m_status = Succeeded;
    return exit(0);
#endif
    if (m_waitPid > 0) {
        logUpdate(tr("Waiting for process %1 to exit before installing.").arg(m_waitPid));
        QProgressDialog waiting(tr("Waiting for the launcher to exit…"), QString(), 0, 0);
        waiting.setCancelButton(nullptr); waiting.setMinimumWidth(480);
        if (!m_headless) waiting.show(); else waiting.setMinimumDuration(INT_MAX);
#ifdef Q_OS_WIN
        HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, DWORD(m_waitPid));
        if (!process && GetLastError() != ERROR_INVALID_PARAMETER) return showFatalErrorMessage("Update handoff failed", "Could not inspect the running launcher process.");
        const auto running = [&] { return process && WaitForSingleObject(process, 0) == WAIT_TIMEOUT; };
#else
        const auto running = [&] { return kill(pid_t(m_waitPid), 0) == 0 || errno == EPERM; };
#endif
        QElapsedTimer elapsed; elapsed.start();
        while (running() && elapsed.elapsed() < 120000) {
            QEventLoop loop; QTimer::singleShot(200, &loop, &QEventLoop::quit); loop.exec();
        }
        const bool stillRunning = running();
#ifdef Q_OS_WIN
        if (process) CloseHandle(process);
#endif
        if (stillRunning) return showFatalErrorMessage("Update handoff timed out", "Close the launcher sidecar before applying the update.");
        m_waitPid = 0;
    }
    if (m_isAppimage) return installAppImage(file);
    if (!m_portableRoot.isEmpty()) return installPortable(file);
    qDebug() << "starting install";
    auto update_lock_path = FS::PathCombine(m_dataPath, ".prism_launcher_update.lock");
    QFileInfo update_lock(update_lock_path);
    if (update_lock.exists()) {
        if (m_headless) return showFatalErrorMessage(tr("Update locked"), tr("An update lock already exists. Inspect the update log before retrying."));
        auto [timestamp, from, to, target, data_path] = read_lock_File(update_lock_path);
        auto msg = tr("Update already in progress\n");
        auto infoMsg =
            tr("This installation has a update lock file present at: %1\n"
               "\n"
               "Timestamp: %2\n"
               "Updating from version %3 to %4\n"
               "Target install path: %5\n"
               "Data Path: %6"
               "\n"
               "This likely means that a previous update attempt failed. Please ensure your installation is in working order before "
               "proceeding.\n"
               "Check the Prism Launcher updater log at: \n"
               "%7\n"
               "for details on the last update attempt.\n"
               "\n"
               "To overwrite this lock and proceed with this update anyway, select \"Ignore\" below.")
                .arg(update_lock_path)
                .arg(timestamp.toString(Qt::ISODate), from, to, target, data_path)
                .arg(m_updateLogPath);
        QMessageBox msgBox;
        msgBox.setText(msg);
        msgBox.setInformativeText(infoMsg);
        msgBox.setStandardButtons(QMessageBox::Ignore | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Cancel);
        msgBox.setMinimumWidth(460);
        msgBox.adjustSize();
        switch (msgBox.exec()) {
            case QMessageBox::AcceptRole:
                break;
            case QMessageBox::RejectRole:
                [[fallthrough]];
            default:
                return showFatalErrorMessage(tr("Update Aborted"), tr("The update attempt was aborted"));
        }
    }
    clearUpdateLog();

    auto changelog_path = FS::PathCombine(m_dataPath, ".prism_launcher_update.changelog");
    FS::write(changelog_path, m_install_release.body.toUtf8());

    logUpdate(tr("Updating from %1 to %2").arg(m_prismVersion).arg(m_install_release.tag_name));
    if (m_isPortable || file.fileName().endsWith(".zip") || file.fileName().endsWith(".tar.gz")) {
        write_lock_file(update_lock_path, QDateTime::currentDateTime(), m_prismVersion, m_install_release.tag_name, m_rootPath, m_dataPath);
        logUpdate(tr("Updating portable install at %1").arg(m_rootPath));
        unpackAndInstall(file);
    } else {
        logUpdate(tr("Running installer file at %1").arg(file.absoluteFilePath()));
        QProcess proc = QProcess();
#if defined Q_OS_WIN
        auto env = QProcessEnvironment::systemEnvironment();
        env.insert("__COMPAT_LAYER", "RUNASINVOKER");
        proc.setProcessEnvironment(env);
#endif
        proc.setProgram(file.absoluteFilePath());
        bool result = proc.startDetached();
        logUpdate(tr("Process start result: %1").arg(result ? tr("yes") : tr("no")));
        exit(result ? 0 : 1);
    }
}

void PrismUpdaterApp::unpackAndInstall(QFileInfo archive)
{
    logUpdate(tr("Backing up install"));
    backupAppDir();

    if (auto loc = unpackArchive(archive)) {
        auto marker_file_path = loc.value().absoluteFilePath(".prism_launcher_updater_unpack.marker");
        FS::write(marker_file_path, m_rootPath.toUtf8());

        QProcess proc = QProcess();

        auto exe_name = QStringLiteral("%1_updater").arg(BuildConfig.LAUNCHER_APP_BINARY_NAME);
#if defined Q_OS_WIN32
        exe_name.append(".exe");

        auto env = QProcessEnvironment::systemEnvironment();
        env.insert("__COMPAT_LAYER", "RUNASINVOKER");
        proc.setProcessEnvironment(env);
#else
        exe_name.prepend("bin/");
#endif

        auto new_updater_path = loc.value().absoluteFilePath(exe_name);
        logUpdate(tr("Starting new updater at '%1'").arg(new_updater_path));
        QStringList workerArguments{ "-d", m_dataPath };
        if (m_headless) workerArguments.append("--headless");
        if (!proc.startDetached(new_updater_path, workerArguments, loc.value().absolutePath())) {
            logUpdate(tr("Failed to launch '%1' %2").arg(new_updater_path).arg(proc.errorString()));
            return exit(10);
        }
        return exit();  // up to the new updater now
    }
    return exit(1);  // unpack failure
}

void PrismUpdaterApp::backupAppDir()
{
    auto manifest_path = FS::PathCombine(m_rootPath, "manifest.txt");
    QFileInfo manifest(manifest_path);

    QStringList file_list;
    if (manifest.isFile()) {
        // load manifest from file

        logUpdate(tr("Reading manifest from %1").arg(manifest.absoluteFilePath()));
        try {
            auto contents = QString::fromUtf8(FS::read(manifest.absoluteFilePath()));
            auto files = contents.split('\n');
            for (auto file : files) {
                file_list.append(file.trimmed());
            }
        } catch (FS::FileSystemException&) {
        }
    }

    if (file_list.isEmpty()) {
        // best guess
        if (BuildConfig.BUILD_ARTIFACT.toLower().contains("linux")) {
            file_list.append({ "PrismLauncher", "bin", "share", "lib" });
        } else {  // windows by process of elimination
            file_list.append({
                "jars",
                "prismlauncher.exe",
                "prismlauncher_filelink.exe",
                "prismlauncher_updater.exe",
                "qtlogging.ini",
                "imageformats",
                "iconengines",
                "platforms",
                "styles",
                "tls",
                "qt.conf",
                "Qt*.dll",
            });
        }
        logUpdate("manifest.txt empty or missing. making best guess at files to back up.");
    }
    logUpdate(tr("Backing up:\n  %1").arg(file_list.join(",\n  ")));
    static const QRegularExpression s_replaceRegex("[" + QRegularExpression::escape("\\/:*?\"<>|") + "]");
    auto app_dir = QDir(m_rootPath);
    auto backup_dir =
        FS::PathCombine(app_dir.absolutePath(),
                        QStringLiteral("backup_") + QString(m_prismVersion).replace(s_replaceRegex, QString("_")) + "-" + m_prismGitCommit);
    FS::ensureFolderPathExists(backup_dir);
    auto backup_marker_path = FS::PathCombine(m_dataPath, ".prism_launcher_update_backup_path.txt");
    FS::write(backup_marker_path, backup_dir.toUtf8());

    QProgressDialog progress(tr("Backing up install at %1").arg(m_rootPath), "", 0, file_list.length());
    progress.setCancelButton(nullptr);
    progress.setMinimumWidth(400);
    progress.adjustSize();
    if (!m_headless) progress.show();
    else progress.setMinimumDuration(INT_MAX);
    QCoreApplication::processEvents();

    logUpdate(tr("Backing up install at %1").arg(m_rootPath));

    auto copy = [this, app_dir, backup_dir](QString to_bak_file) {
        auto rel_path = app_dir.relativeFilePath(to_bak_file);
        auto bak_path = FS::PathCombine(backup_dir, rel_path);
        logUpdate(tr("Backing up and then removing %1").arg(to_bak_file));
        FS::ensureFilePathExists(bak_path);
        auto result = FS::copy(to_bak_file, bak_path).overwrite(true)();
        if (!result) {
            logUpdate(tr("Failed to backup %1 to %2").arg(to_bak_file).arg(bak_path));
        } else {
            if (!FS::deletePath(to_bak_file))
                logUpdate(tr("Failed to remove %1").arg(to_bak_file));
        }
    };

    int i = 0;
    for (auto glob : file_list) {
        QDirIterator iter(app_dir.absolutePath(), QStringList({ glob }), QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
        progress.setValue(i);
        QCoreApplication::processEvents();
        if (!iter.hasNext() && !glob.isEmpty()) {
            if (auto file_info = QFileInfo(FS::PathCombine(app_dir.absolutePath(), glob)); file_info.exists()) {
                copy(file_info.absoluteFilePath());
            } else {
                logUpdate(tr("File doesn't exist, ignoring: %1").arg(FS::PathCombine(app_dir.absolutePath(), glob)));
            }
        } else {
            while (iter.hasNext()) {
                copy(iter.next());
            }
        }
        i++;
    }
    progress.setValue(i);
    QCoreApplication::processEvents();
}

std::optional<QDir> PrismUpdaterApp::unpackArchive(QFileInfo archive)
{
    auto temp_extract_path = FS::PathCombine(m_dataPath, "prism_launcher_update_release");
    FS::ensureFolderPathExists(temp_extract_path);
    auto tmp_extract_dir = QDir(temp_extract_path);

    auto result = MMCZip::extractDir(archive.absoluteFilePath(), tmp_extract_dir.absolutePath());
    if (result) {
        logUpdate(tr("Extracted the following to \"%1\":\n  %2").arg(tmp_extract_dir.absolutePath()).arg(result->join("\n  ")));
    } else {
        logUpdate(tr("Failed to extract %1 to %2").arg(archive.absoluteFilePath()).arg(tmp_extract_dir.absolutePath()));
        showFatalErrorMessage("Failed to extract archive",
                              tr("Failed to extract %1 to %2").arg(archive.absoluteFilePath()).arg(tmp_extract_dir.absolutePath()));
        return std::nullopt;
    }

    return tmp_extract_dir;
}

bool PrismUpdaterApp::loadPrismVersionFromExe(const QString& exe_path)
{
    QProcess proc = QProcess();
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.setReadChannel(QProcess::StandardOutput);
    proc.start(exe_path, { "--version" });
    if (!proc.waitForStarted(5000)) {
        showFatalErrorMessage(tr("Failed to Check Version"), tr("Failed to launch child process to read version."));
        return false;
    }  // wait 5 seconds to start
    if (!proc.waitForFinished(5000)) {
        showFatalErrorMessage(tr("Failed to Check Version"), tr("Child launcher process failed."));
        return false;
    }
    auto out = proc.readAllStandardOutput();
    auto lines = out.split('\n');
    lines.removeAll("");
    if (lines.length() < 2)
        return false;
    else if (lines.length() > 2) {
        auto line1 = lines.takeLast();
        auto line2 = lines.takeLast();
        lines = { line2, line1 };
    }
    auto first = lines.takeFirst();
    auto first_parts = first.split(' ');
    if (first_parts.length() < 2)
        return false;
    m_prismBinaryName = first_parts.takeFirst();
    auto version = first_parts.takeFirst().trimmed();
    m_prismVersion = version;
    if (version.contains('-')) {
        auto index = version.indexOf('-');
        m_prsimVersionChannel = version.mid(index + 1);
        version = version.left(index);
    } else {
        m_prsimVersionChannel = "stable";
    }
    auto version_parts = version.split('.');
    if (version_parts.length() < 2)
        return false;
    m_prismVersionMajor = version_parts.takeFirst().toInt();
    m_prismVersionMinor = version_parts.takeFirst().toInt();
    if (!version_parts.isEmpty())
        m_prismVersionPatch = version_parts.takeFirst().toInt();
    else
        m_prismVersionPatch = 0;
    m_prismGitCommit = lines.takeFirst().simplified();
    return true;
}

void PrismUpdaterApp::loadReleaseList()
{
    auto github_repo = m_prismRepoUrl;
    if (github_repo.host() != "github.com")
        return fail("updating from a non github url is not supported");

    auto path_parts = github_repo.path().split('/', Qt::SkipEmptyParts);
    if (path_parts.size() != 2) return fail("Update URL must identify a GitHub owner and repository.");
    auto repo_owner = path_parts.takeFirst();
    auto repo_name = path_parts.takeFirst();
    auto api_url = QString("https://api.github.com/repos/%1/%2/releases").arg(repo_owner, repo_name);

    qDebug() << "Fetching release list from" << api_url;

    downloadReleasePage(api_url, 1);
}

void PrismUpdaterApp::downloadReleasePage(const QString& api_url, int page)
{
    int per_page = 30;
    auto page_url = QString("%1?per_page=%2&page=%3").arg(api_url).arg(QString::number(per_page)).arg(QString::number(page));
    auto [download, response] = Net::Download::makeByteArray(page_url);
    download->setNetwork(m_network);
    m_current_url = page_url;

    auto github_api_headers = std::make_unique<Net::RawHeaderProxy>();
    github_api_headers->addHeaders({
        { "Accept", "application/vnd.github+json" },
        { "X-GitHub-Api-Version", "2022-11-28" },
    });
    download->addHeaderProxy(std::move(github_api_headers));

    connect(download.get(), &Net::Download::succeeded, this, [this, response, per_page, api_url, page]() {
        int num_found = parseReleasePage(response);
        if (!(num_found < per_page)) {  // there may be more, fetch next page
            downloadReleasePage(api_url, page + 1);
        } else {
            run();
        }
    });
    connect(download.get(), &Net::Download::failed, this, &PrismUpdaterApp::downloadError);

    m_current_task.reset(download);
    connect(download.get(), &Net::Download::finished, this, [this]() {
        qDebug() << "Download" << m_current_task->getUid().toString() << "finished";
    });

    QCoreApplication::processEvents();

    QMetaObject::invokeMethod(download.get(), &Task::start, Qt::QueuedConnection);
}

int PrismUpdaterApp::parseReleasePage(const QByteArray* response)
{
    if (response->isEmpty())  // empty page
        return 0;
    int num_releases = 0;
    try {
        auto doc = Json::requireDocument(*response);
        auto release_list = Json::requireArray(doc);
        for (auto release_json : release_list) {
            auto release_obj = Json::requireObject(release_json);

            GitHubRelease release = {};
            release.id = Json::requireInteger(release_obj, "id");
            release.name = release_obj["name"].toString();
            release.tag_name = Json::requireString(release_obj, "tag_name");
            release.created_at = QDateTime::fromString(Json::requireString(release_obj, "created_at"), Qt::ISODate);
            release.published_at = QDateTime::fromString(release_obj["published_at"].toString(), Qt::ISODate);
            release.draft = Json::requireBoolean(release_obj, "draft");
            release.prerelease = Json::requireBoolean(release_obj, "prerelease");
            release.body = release_obj["body"].toString();
            release.version = Version(WindowsUpdate::normalizedVersion(release.tag_name));

            auto release_assets_obj = Json::requireArray(release_obj, "assets");
            for (auto asset_json : release_assets_obj) {
                auto asset_obj = Json::requireObject(asset_json);
                GitHubReleaseAsset asset = {};
                asset.id = Json::requireInteger(asset_obj, "id");
                asset.name = Json::requireString(asset_obj, "name");
                asset.label = asset_obj["label"].toString();
                asset.content_type = Json::requireString(asset_obj, "content_type");
                asset.size = Json::requireInteger(asset_obj, "size");
                asset.created_at = QDateTime::fromString(Json::requireString(asset_obj, "created_at"), Qt::ISODate);
                asset.updated_at = QDateTime::fromString(Json::requireString(asset_obj, "updated_at"), Qt::ISODate);
                asset.browser_download_url = Json::requireString(asset_obj, "browser_download_url");
                release.assets.append(asset);
            }
            m_releases.append(release);
            num_releases++;
        }
    } catch (Json::JsonException& e) {
        auto err_msg =
            QString("Failed to parse releases from github: %1\n%2").arg(e.what()).arg(QString::fromStdString(response->toStdString()));
        fail(err_msg);
    }
    return num_releases;
}

GitHubRelease PrismUpdaterApp::getLatestRelease()
{
    GitHubRelease latest;
    for (auto release : m_releases) {
        if (release.draft)
            continue;
        if (release.prerelease && !m_allowPreRelease)
            continue;
        if (validReleaseArtifacts(release).isEmpty()) continue;
        auto candidateVersion = release.version;
        auto latestVersion = latest.version;
        const auto candidateManifest = updateManifest(release);
        if (!candidateManifest.isEmpty()) candidateVersion = Version(WindowsUpdate::normalizedVersion(candidateManifest.value("version").toString()));
        if (latest.isValid()) {
            const auto previousManifest = updateManifest(latest);
            if (!previousManifest.isEmpty()) latestVersion = Version(WindowsUpdate::normalizedVersion(previousManifest.value("version").toString()));
        }
        if (!latest.isValid() || (candidateVersion > latestVersion)) {
            latest = release;
        }
    }
    return latest;
}

bool PrismUpdaterApp::needUpdate(const GitHubRelease& release)
{
    if (!release.isValid()) return false;
    auto current_ver = Version(WindowsUpdate::normalizedVersion(m_prismVersion));
    const auto manifest = updateManifest(release);
    return current_ver < (manifest.isEmpty() ? release.version : Version(WindowsUpdate::normalizedVersion(manifest.value("version").toString())));
}

void PrismUpdaterApp::downloadError(QString reason)
{
    showFatalErrorMessage(tr("Update check failed"), QString("Network request failed: %1: %2").arg(m_current_url, reason));
}
