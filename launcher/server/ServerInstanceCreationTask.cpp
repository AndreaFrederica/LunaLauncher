/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
*/
#include "ServerInstanceCreationTask.h"
#include "settings/INIFile.h"
#include "FileSystem.h"

ServerInstanceCreationTask::ServerInstanceCreationTask(const QString &exePath, const QStringList &args)
    : m_exePath(exePath), m_args(args)
{
}

bool ServerInstanceCreationTask::createInstance()
{
    QDir rootDir(m_stagingPath);
    if (!FS::ensureFolderPathExists(rootDir.absolutePath()))
    {
        setError("Failed to create instance directory");
        return false;
    }

    INIFile ini;
    ini.set("InstanceType", "Server");

    QString finalExePath = m_exePath;

    // Copy executable if provided
    if (!m_exePath.isEmpty() && QFileInfo::exists(m_exePath)) {
        QFileInfo exeInfo(m_exePath);
        QString destName = exeInfo.fileName();
        QString destPath = FS::PathCombine(m_stagingPath, destName);

        if (QFile::copy(m_exePath, destPath)) {
            // If copied successfully, use the relative path (filename)
            // But we should store it such that it can be found.
            // For now, let's just store the filename if we intend to run it from working dir,
            // or absolute path if not.
            // To be safe and compliant with user request "must be in instance directory",
            // we use the local file.
            finalExePath = destName;
        } else {
             qWarning() << "Failed to copy server executable from" << m_exePath << "to" << destPath;
        }
    }

    // Detect if it's a jar file and configure java invocation
    QString exeToSet = finalExePath;
    QStringList argsToSet = m_args;

    if (finalExePath.endsWith(".jar", Qt::CaseInsensitive)) {
        exeToSet = "$java";
        argsToSet.prepend(finalExePath);
        argsToSet.prepend("-jar");
        /*
        if (!argsToSet.contains("nogui", Qt::CaseInsensitive)) {
            argsToSet.append("nogui");
        }
        */
    }

    ini.set("ExecutablePath", exeToSet);
    ini.set("Arguments", argsToSet);
    ini.set("name", name());

    if (!ini.saveFile(FS::PathCombine(m_stagingPath, "instance.cfg")))
    {
        setError("Failed to save instance.cfg");
        return false;
    }

    // Create mmc-pack.json for valid instance identification
    QFile packFile(FS::PathCombine(m_stagingPath, "mmc-pack.json"));
    if (packFile.open(QIODevice::WriteOnly)) {
        packFile.write(R"({
    "components": [
        {
            "cachedName": "Server",
            "cachedVersion": "1.0.0",
            "uid": "org.prismlauncher.server",
            "version": "1.0.0"
        }
    ],
    "formatVersion": 1
})");
        packFile.close();
    }

    return true;
}
