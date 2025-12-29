/*
 *  Luna Launcher - Minecraft Launcher 
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
 */
#include "CustomPlayerModel.h"
#include "Json.h"
#include "archive/ArchiveReader.h"
#include <QDebug>

CustomPlayerModel::CustomPlayerModel(QFileInfo file) : Resource(file)
{
    readMetadata();
}

void CustomPlayerModel::readMetadata()
{
    QByteArray data;
    if (m_type == ResourceType::FOLDER) {
        QFile file(m_file_info.absoluteDir().filePath(m_file_info.fileName() + "/model.json"));
        if (file.open(QIODevice::ReadOnly)) {
            data = file.readAll();
        }
    } else {
        // Zip
        MMCZip::ArchiveReader reader(m_file_info.absoluteFilePath());
        auto file = reader.goToFile("model.json");
        if (file) {
            data = file->readAll();
        }
    }

    if (data.isEmpty()) {
        return;
    }

    try {
        auto doc = Json::requireDocument(data);
        auto obj = Json::requireObject(doc);
        
        if (obj.contains("modelName"))
            m_name = Json::requireString(obj, "modelName");
        else
            m_name = m_file_info.completeBaseName();
        if (obj.contains("version"))
            m_version = Json::requireString(obj, "version");
        else
            m_version = "";
        if (obj.contains("author"))
            m_author = Json::requireString(obj, "author");
        else
            m_author = "";

    } catch (const Json::JsonException& e) {
        qWarning() << "Failed to parse model.json for" << m_file_info.fileName() << ":" << e.what();
    }
}
