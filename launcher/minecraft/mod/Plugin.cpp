// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
 */

#include "Plugin.h"
#include "FileSystem.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QRegularExpression>

#include "archive/ArchiveReader.h"

Plugin::Plugin(const QFileInfo& file) : Resource(file)
{
    // Resource constructor calls parseFile(), which sets m_name, m_type, m_enabled etc.
    // We only need to parse plugin-specific metadata if it's a valid JAR

    // Resource::parseFile() sets type to ZIPFILE for .jar
    // It also handles .disabled suffix

    if (m_type == ResourceType::ZIPFILE) {
        // It's a jar file (enabled or disabled)
        parsePluginInfo();
    }
}

// sizeStr, setEnabled, enable, disable are removed (inherited from Resource)


namespace PluginUtils {

static const QRegularExpression s_newlineRegex("\r\n|\n|\r");

struct PluginDetails {
    QString name;
    QString version;
    QString description;
    QString website;
    QStringList authors;
};

static QString stripInlineComment(QString line)
{
    bool inSingle = false;
    bool inDouble = false;
    for (int i = 0; i < line.size(); i++) {
        const auto ch = line.at(i);
        if (ch == '\'' && !inDouble) {
            inSingle = !inSingle;
            continue;
        }
        if (ch == '"' && !inSingle) {
            inDouble = !inDouble;
            continue;
        }
        if (ch == '#' && !inSingle && !inDouble) {
            return line.left(i);
        }
    }
    return line;
}

static QString unquote(QString s)
{
    s = s.trimmed();
    if (s.size() >= 2) {
        const auto first = s.front();
        const auto last = s.back();
        if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
            return s.mid(1, s.size() - 2);
        }
    }
    return s;
}

static QStringList parseInlineList(QString s)
{
    s = s.trimmed();
    if (!s.startsWith('[') || !s.endsWith(']')) {
        return {};
    }
    s = s.mid(1, s.size() - 2);

    QStringList out;
    QString current;
    bool inSingle = false;
    bool inDouble = false;
    for (int i = 0; i < s.size(); i++) {
        const auto ch = s.at(i);
        if (ch == '\'' && !inDouble) {
            inSingle = !inSingle;
            current += ch;
            continue;
        }
        if (ch == '"' && !inSingle) {
            inDouble = !inDouble;
            current += ch;
            continue;
        }
        if (ch == ',' && !inSingle && !inDouble) {
            auto item = unquote(current.trimmed());
            if (!item.isEmpty())
                out.append(item);
            current.clear();
            continue;
        }
        current += ch;
    }
    auto item = unquote(current.trimmed());
    if (!item.isEmpty())
        out.append(item);
    return out;
}

static PluginDetails readBukkitStyleYml(QByteArray contents)
{
    PluginDetails details;

    auto text = QString::fromUtf8(contents);
    auto lines = text.split(s_newlineRegex);

    bool collectingAuthors = false;
    int authorsIndent = 0;

    bool collectingDescriptionBlock = false;
    int descriptionIndent = 0;
    QString descriptionBlock;

    int i = 0;
    while (i < lines.size()) {
        QString line = lines.at(i);

        int leadingSpaces = 0;
        while (leadingSpaces < line.size() && line.at(leadingSpaces) == ' ')
            leadingSpaces++;

        auto trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith('#')) {
            i++;
            continue;
        }

        if (collectingAuthors) {
            if (leadingSpaces > authorsIndent && trimmed.startsWith('-')) {
                auto item = trimmed.mid(1).trimmed();
                item = unquote(stripInlineComment(item).trimmed());
                if (!item.isEmpty())
                    details.authors.append(item);
                i++;
                continue;
            }
            collectingAuthors = false;
            continue;
        }

        if (collectingDescriptionBlock) {
            if (leadingSpaces > descriptionIndent) {
                descriptionBlock += trimmed;
                descriptionBlock += "\n";
                i++;
                continue;
            }
            collectingDescriptionBlock = false;
            details.description = descriptionBlock.trimmed();
            continue;
        }

        if (leadingSpaces != 0) {
            i++;
            continue;
        }

        line = stripInlineComment(line).trimmed();
        if (line.isEmpty()) {
            i++;
            continue;
        }

        auto colonPos = line.indexOf(':');
        if (colonPos <= 0) {
            i++;
            continue;
        }

        const auto key = line.left(colonPos).trimmed();
        auto value = line.mid(colonPos + 1).trimmed();

        if (key.compare("name", Qt::CaseInsensitive) == 0) {
            auto v = unquote(value);
            if (!v.isEmpty())
                details.name = v;
        } else if (key.compare("version", Qt::CaseInsensitive) == 0) {
            auto v = unquote(value);
            if (!v.isEmpty())
                details.version = v;
        } else if (key.compare("website", Qt::CaseInsensitive) == 0) {
            auto v = unquote(value);
            if (!v.isEmpty())
                details.website = v;
        } else if (key.compare("description", Qt::CaseInsensitive) == 0) {
            if (value == "|" || value == ">" || value.startsWith("|") || value.startsWith(">")) {
                collectingDescriptionBlock = true;
                descriptionIndent = leadingSpaces;
                descriptionBlock.clear();
            } else {
                details.description = unquote(value);
            }
        } else if (key.compare("author", Qt::CaseInsensitive) == 0) {
            auto v = unquote(value);
            if (!v.isEmpty())
                details.authors = { v };
        } else if (key.compare("authors", Qt::CaseInsensitive) == 0) {
            if (value.isEmpty()) {
                collectingAuthors = true;
                authorsIndent = leadingSpaces;
            } else {
                auto list = parseInlineList(value);
                if (!list.isEmpty())
                    details.authors = list;
                else {
                    auto v = unquote(value);
                    if (!v.isEmpty())
                        details.authors = { v };
                }
            }
        }

        i++;
    }

    if (collectingDescriptionBlock) {
        details.description = descriptionBlock.trimmed();
    }

    return details;
}

static PluginDetails readVelocityPluginJson(QByteArray contents)
{
    PluginDetails details;

    QJsonParseError jsonError;
    auto doc = QJsonDocument::fromJson(contents, &jsonError);
    if (!doc.isObject())
        return details;

    auto obj = doc.object();

    details.name = obj.value("name").toString(details.name);
    details.version = obj.value("version").toString(details.version);
    details.description = obj.value("description").toString(details.description);

    auto authorsVal = obj.value("authors");
    if (authorsVal.isArray()) {
        for (auto v : authorsVal.toArray()) {
            auto s = v.toString().trimmed();
            if (!s.isEmpty())
                details.authors.append(s);
        }
    } else if (authorsVal.isString()) {
        auto s = authorsVal.toString().trimmed();
        if (!s.isEmpty())
            details.authors = { s };
    }

    if (obj.contains("url") && obj.value("url").isString())
        details.website = obj.value("url").toString();
    if (details.website.isEmpty() && obj.contains("website") && obj.value("website").isString())
        details.website = obj.value("website").toString();

    return details;
}

static PluginDetails readSpongePluginsJson(QByteArray contents)
{
    PluginDetails details;

    QJsonParseError jsonError;
    auto doc = QJsonDocument::fromJson(contents, &jsonError);
    if (!doc.isObject())
        return details;

    auto root = doc.object();
    auto pluginsVal = root.value("plugins");
    if (!pluginsVal.isArray() || pluginsVal.toArray().isEmpty())
        return details;

    auto plugin = pluginsVal.toArray().at(0).toObject();
    details.name = plugin.value("name").toString(details.name);
    details.version = plugin.value("version").toString(details.version);
    details.description = plugin.value("description").toString(details.description);

    auto authorsVal = plugin.value("authors");
    if (authorsVal.isArray()) {
        for (auto v : authorsVal.toArray()) {
            auto s = v.toString().trimmed();
            if (!s.isEmpty())
                details.authors.append(s);
        }
    } else if (authorsVal.isString()) {
        auto s = authorsVal.toString().trimmed();
        if (!s.isEmpty())
            details.authors = { s };
    }

    auto links = plugin.value("links");
    if (links.isObject()) {
        auto linksObj = links.toObject();
        details.website = linksObj.value("homepage").toString(details.website);
    }

    return details;
}

}  // namespace PluginUtils

void Plugin::parsePluginInfo()
{
    m_version = "Unknown";
    m_description = "";
    m_website = "";
    m_authors.clear();

    MMCZip::ArchiveReader zip(fileinfo().absoluteFilePath());

    auto applyDetails = [&](const PluginUtils::PluginDetails& details) {
        if (!details.name.isEmpty())
            m_name = details.name;
        if (!details.version.isEmpty())
            m_version = details.version;
        if (!details.description.isEmpty())
            m_description = details.description;
        if (!details.website.isEmpty())
            m_website = details.website;
        if (!details.authors.isEmpty())
            m_authors = details.authors;
    };

    auto tryReadYml = [&](const QString& path) -> bool {
        if (auto f = zip.goToFile(path); f) {
            applyDetails(PluginUtils::readBukkitStyleYml(f->readAll()));
            return true;
        }
        return false;
    };

    auto tryReadJson = [&](const QString& path, const auto& reader) -> bool {
        if (auto f = zip.goToFile(path); f) {
            applyDetails(reader(f->readAll()));
            return true;
        }
        return false;
    };

    if (tryReadYml("paper-plugin.yml"))
        return;
    if (tryReadYml("plugin.yml"))
        return;
    if (tryReadYml("bungee.yml"))
        return;
    if (tryReadJson("velocity-plugin.json", PluginUtils::readVelocityPluginJson))
        return;
    if (tryReadJson("META-INF/sponge_plugins.json", PluginUtils::readSpongePluginsJson))
        return;
}
