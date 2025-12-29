/*
 *  Luna Launcher - Minecraft Launcher 
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
 */
#pragma once
#include "Resource.h"

class YesSteveModel : public Resource {
    Q_OBJECT
 public:
     explicit YesSteveModel(QFileInfo file);
     virtual ~YesSteveModel() = default;
 
     QString author() const { return m_author; }
     QString homepage() const override { return m_homepage; }
 
 private:
     void readMetadata();
     QString m_author;
     QString m_homepage;
 };
