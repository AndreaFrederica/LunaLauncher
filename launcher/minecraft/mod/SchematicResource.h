/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
 */
#pragma once
#include "Resource.h"
#include <QString>

enum class SchematicFormat { Unknown, Schematic, Schem, Litematic };

class SchematicResource : public Resource {
    Q_OBJECT
public:
    explicit SchematicResource(QFileInfo file);
    ~SchematicResource() override = default;

    QString name() const override;
    QString homepage() const override { return {}; }

    SchematicFormat format() const { return m_format; }
    int width() const { return m_width; }
    int height() const { return m_height; }
    int length() const { return m_length; }
    QString author() const { return m_author; }
    int dataVersion() const { return m_data_version; }
    int formatVersion() const { return m_format_version; }
    int paletteSize() const { return m_palette_size; }
    int blockDataCount() const { return m_blockdata_count; }
    QString formatString() const;
    QString dimensionsString() const;

private:
    void parseMetadata();
    void parseSchematicNBT();
    void parseSchemNBT();
    void parseLitematicNBT();

private:
    SchematicFormat m_format = SchematicFormat::Unknown;
    int m_width = 0;
    int m_height = 0;
    int m_length = 0;
    QString m_author;
    QString m_internal_name;
    int m_data_version = 0;
    int m_format_version = 0;
    int m_palette_size = 0;
    int m_blockdata_count = 0;
};
