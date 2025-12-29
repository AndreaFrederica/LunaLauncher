/*
 *  Luna Launcher - Minecraft Launcher 
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
 */
#pragma once
#include "Resource.h"

class CustomPlayerModel : public Resource {
    Q_OBJECT
public:
    explicit CustomPlayerModel(QFileInfo file);
    virtual ~CustomPlayerModel() = default;

    QString version() const { return m_version; }
    QString author() const { return m_author; }

private:
    void readMetadata();
    QString m_version;
    QString m_author;
};
