/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
*/
#pragma once
#include <QMap>
#include <QString>

class PropertiesFile : public QMap<QString, QString>
{
public:
    bool loadFile(const QString &filename);
    bool saveFile(const QString &filename);
};
