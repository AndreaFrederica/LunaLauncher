/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
*/
#include "PropertiesFile.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>

bool PropertiesFile::loadFile(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    clear();
    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#') || line.startsWith('!'))
            continue;

        int splitIndex = line.indexOf('=');
        if (splitIndex == -1)
            splitIndex = line.indexOf(':');

        if (splitIndex != -1) {
            QString key = line.left(splitIndex).trimmed();
            QString value = line.mid(splitIndex + 1).trimmed();
            insert(key, value);
        }
    }
    return true;
}

bool PropertiesFile::saveFile(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
    out << "#Minecraft server properties\n";
    out << "#(File modified by Prism Launcher)\n";
    
    for (auto it = begin(); it != end(); ++it) {
        out << it.key() << "=" << it.value() << "\n";
    }
    return true;
}
