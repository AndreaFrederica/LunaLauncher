/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
 */
#include "SchematicResource.h"
#include <QFileInfo>
#include <QRegularExpression>
#include <fstream>
#include <sstream>
#include <io/stream_reader.h>
#include <tag_compound.h>
#include <value.h>
#include "GZip.h"
#include <QFile>
#include <tag_array.h>

static std::unique_ptr<nbt::tag_compound> readRootCompoundFromBytes(const QByteArray& bytes)
{
    std::istringstream ss(std::string(bytes.constData(), bytes.size()), std::ios::binary);
    nbt::io::stream_reader reader(ss);
    auto pair = reader.read_compound();
    return std::move(pair.second);
}

static std::unique_ptr<nbt::tag_compound> readRootCompoundAuto(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return nullptr;
    QByteArray raw = file.readAll();
    QByteArray unzipped;
    // Try gzip first
    if (GZip::unzip(raw, unzipped)) {
        if (!unzipped.isEmpty()) {
            try {
                return readRootCompoundFromBytes(unzipped);
            } catch (...) {
                // fall through
            }
        }
    }
    try {
        return readRootCompoundFromBytes(raw);
    } catch (...) {
        return nullptr;
    }
}

SchematicResource::SchematicResource(QFileInfo file_info) : Resource(file_info)
{
    parseMetadata();
}

QString SchematicResource::name() const
{
    if (!m_internal_name.isEmpty())
        return m_internal_name;
    return Resource::name();
}

QString SchematicResource::formatString() const
{
    switch (m_format) {
        case SchematicFormat::Schematic:
            return "Schematic";
        case SchematicFormat::Schem:
            return "Sponge Schem";
        case SchematicFormat::Litematic:
            return "Litematic";
        default:
            return "Unknown";
    }
}

QString SchematicResource::dimensionsString() const
{
    if (m_width <= 0 || m_height <= 0 || m_length <= 0)
        return {};
    return QString("%1 × %2 × %3").arg(m_width).arg(m_height).arg(m_length);
}

void SchematicResource::parseMetadata()
{
    auto fname = m_file_info.fileName().toLower();
    if (fname.endsWith(".schematic"))
        m_format = SchematicFormat::Schematic;
    else if (fname.endsWith(".schem"))
        m_format = SchematicFormat::Schem;
    else if (fname.endsWith(".litematic"))
        m_format = SchematicFormat::Litematic;
    else
        m_format = SchematicFormat::Unknown;

    switch (m_format) {
        case SchematicFormat::Schematic:
            parseSchematicNBT();
            break;
        case SchematicFormat::Schem:
            parseSchemNBT();
            break;
        case SchematicFormat::Litematic:
            parseLitematicNBT();
            break;
        default:
            break;
    }
}

void SchematicResource::parseSchematicNBT()
{
    auto root = readRootCompoundAuto(m_file_info.absoluteFilePath());
    if (!root) return;
    try {
        if (root->has_key("Width"))
            m_width = static_cast<int16_t>(root->at("Width"));
        if (root->has_key("Height"))
            m_height = static_cast<int16_t>(root->at("Height"));
        if (root->has_key("Length"))
            m_length = static_cast<int16_t>(root->at("Length"));
        if (root->has_key("Materials"))
            m_internal_name = QString::fromStdString(static_cast<const std::string&>(root->at("Materials")));
    } catch (...) {
    }
}

void SchematicResource::parseSchemNBT()
{
    auto root = readRootCompoundAuto(m_file_info.absoluteFilePath());
    if (!root) return;
    try {
        if (root->has_key("DataVersion"))
            m_data_version = static_cast<int32_t>(root->at("DataVersion"));
        if (root->has_key("Width"))
            m_width = static_cast<int16_t>(root->at("Width"));
        if (root->has_key("Height"))
            m_height = static_cast<int16_t>(root->at("Height"));
        if (root->has_key("Length"))
            m_length = static_cast<int16_t>(root->at("Length"));
        if (root->has_key("Version"))
            m_format_version = static_cast<int32_t>(root->at("Version"));
        if (root->has_key("Palette") && root->has_key("Palette", nbt::tag_type::Compound)) {
            auto& pal = root->at("Palette").as<nbt::tag_compound>();
            m_palette_size = static_cast<int>(pal.size());
        }
        if (root->has_key("BlockData") && root->has_key("BlockData", nbt::tag_type::Byte_Array)) {
            auto& arr = root->at("BlockData").as<nbt::tag_byte_array>();
            m_blockdata_count = static_cast<int>(arr.size());
        }
        if (root->has_key("Metadata") && root->has_key("Metadata", nbt::tag_type::Compound)) {
            auto& meta = root->at("Metadata");
            if (meta.as<nbt::tag_compound>().has_key("Name"))
                m_internal_name = QString::fromStdString(static_cast<const std::string&>(meta.at("Name")));
            if (meta.as<nbt::tag_compound>().has_key("Author"))
                m_author = QString::fromStdString(static_cast<const std::string&>(meta.at("Author")));
        }
    } catch (...) {
    }
}

void SchematicResource::parseLitematicNBT()
{
    auto root = readRootCompoundAuto(m_file_info.absoluteFilePath());
    if (!root) return;
    try {
        if (root->has_key("Metadata") && root->has_key("Metadata", nbt::tag_type::Compound)) {
            auto& metaVal = root->at("Metadata");
            auto& meta = metaVal.as<nbt::tag_compound>();
            if (meta.has_key("Name"))
                m_internal_name = QString::fromStdString(static_cast<const std::string&>(meta.at("Name")));
            if (meta.has_key("Author"))
                m_author = QString::fromStdString(static_cast<const std::string&>(meta.at("Author")));
            if (meta.has_key("EnclosingSize") && meta.has_key("EnclosingSize", nbt::tag_type::Compound)) {
                auto& enc = meta.at("EnclosingSize").as<nbt::tag_compound>();
                if (enc.has_key("x")) m_width = static_cast<int32_t>(enc.at("x"));
                if (enc.has_key("y")) m_height = static_cast<int32_t>(enc.at("y"));
                if (enc.has_key("z")) m_length = static_cast<int32_t>(enc.at("z"));
                if (m_width && m_height && m_length) return;
            }
        }
        if (root->has_key("Regions") && root->has_key("Regions", nbt::tag_type::Compound)) {
            auto& regions = root->at("Regions").as<nbt::tag_compound>();
            int maxX = 0, maxY = 0, maxZ = 0;
            for (auto it = regions.begin(); it != regions.end(); ++it) {
                const auto& val = it->second;
                if (val.get_type() != nbt::tag_type::Compound) continue;
                const auto& region = val.as<nbt::tag_compound>();
                if (region.has_key("Size") && region.has_key("Size", nbt::tag_type::Compound)) {
                    const auto& sz = region.at("Size").as<nbt::tag_compound>();
                    int rx = sz.has_key("x") ? static_cast<int32_t>(sz.at("x")) : 0;
                    int ry = sz.has_key("y") ? static_cast<int32_t>(sz.at("y")) : 0;
                    int rz = sz.has_key("z") ? static_cast<int32_t>(sz.at("z")) : 0;
                    maxX = std::max(maxX, rx);
                    maxY = std::max(maxY, ry);
                    maxZ = std::max(maxZ, rz);
                }
            }
            m_width = maxX;
            m_height = maxY;
            m_length = maxZ;
        }
    } catch (...) {
    }
}
