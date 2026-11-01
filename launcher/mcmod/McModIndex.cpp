// SPDX-License-Identifier: GPL-3.0-only
#include "McModIndex.h"

#include "LauncherResources.h"

#include <QFile>
#include <QUrl>
#include <QUrlQuery>

namespace McMod {

const Index& Index::instance()
{
    static const Index index;
    return index;
}

Index::Index()
{
    load();
}

QString Index::normalizeName(QString value)
{
    return value.simplified().toCaseFolded();
}

QString Index::normalizeModId(QString value)
{
    return value.trimmed().toCaseFolded();
}

QStringList Index::parseCsvLine(const QByteArray& line)
{
    const auto text = QString::fromUtf8(line);
    QStringList fields;
    QString field;
    bool escaped = false;
    for (qsizetype i = 0; i < text.size(); ++i) {
        const auto ch = text.at(i);
        if (escaped) {
            switch (ch.unicode()) {
                case 'n': field += QLatin1Char('\n'); break;
                case 'r': field += QLatin1Char('\r'); break;
                case 't': field += QLatin1Char('\t'); break;
                case '0': field += QChar(0); break;
                // HMCL calls the comma escape `\\c`; accept `\\,` too so
                // exports produced by the companion Python tool remain valid.
                case 'c':
                case ',': field += QLatin1Char(','); break;
                case 'u': {
                    if (i + 4 < text.size()) {
                        bool ok = false;
                        const auto code = text.mid(i + 1, 4).toUInt(&ok, 16);
                        if (ok) {
                            field += QChar(static_cast<ushort>(code));
                            i += 4;
                            break;
                        }
                    }
                    field += ch;
                    break;
                }
                default: field += ch; break;
            }
            escaped = false;
        } else if (ch == QLatin1Char('\\')) {
            escaped = true;
        } else if (ch == QLatin1Char(',')) {
            fields.append(field);
            field.clear();
        } else {
            field += ch;
        }
    }
    if (escaped)
        field += QLatin1Char('\\');
    fields.append(field);
    return fields;
}

bool Index::containsChinese(const QString& value)
{
    for (const auto code : value.toUcs4()) {
        if ((code >= 0x3400 && code <= 0x4DBF) ||   // CJK extension A
            (code >= 0x4E00 && code <= 0x9FFF) ||   // CJK unified ideographs
            (code >= 0xF900 && code <= 0xFAFF) ||   // CJK compatibility ideographs
            (code >= 0x20000 && code <= 0x2FA1F))   // supplementary CJK
            return true;
    }
    return false;
}

void Index::load()
{
    const auto path = LauncherResources::locateFile(QStringLiteral("mcmod/mcmod.csv"));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return;

    while (!file.atEnd()) {
        auto raw = file.readLine();
        while (raw.endsWith('\n') || raw.endsWith('\r'))
            raw.chop(1);
        const auto trimmed = raw.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith('#'))
            continue;

        const auto fields = parseCsvLine(raw);
        if (fields.size() < 7)
            continue;

        bool ok = false;
        const auto id = fields.at(0).trimmed().toLongLong(&ok);
        if (!ok || id <= 0 || fields.at(3).trimmed().isEmpty())
            continue;

        Project project;
        project.id = id;
        project.curseForgeId = fields.at(1).trimmed();
        project.modrinthId = fields.at(2).trimmed();
        project.mainName = fields.at(3).trimmed();
        project.secondaryName = fields.at(4).trimmed();
        project.abbreviation = fields.at(5).trimmed();
        for (auto modId : fields.at(6).split(';', Qt::SkipEmptyParts)) {
            modId = modId.trimmed();
            if (!modId.isEmpty())
                project.modIds.append(modId);
        }

        const auto index = m_projects.size();
        m_projects.append(std::move(project));
        const auto& loadedProject = m_projects.constLast();
        if (!loadedProject.curseForgeId.isEmpty() && !m_curseForgeIndex.contains(normalizeModId(loadedProject.curseForgeId)))
            m_curseForgeIndex.insert(normalizeModId(loadedProject.curseForgeId), index);
        if (!loadedProject.modrinthId.isEmpty() && !m_modrinthIndex.contains(normalizeModId(loadedProject.modrinthId)))
            m_modrinthIndex.insert(normalizeModId(loadedProject.modrinthId), index);
        for (const auto& modId : loadedProject.modIds) {
            const auto key = normalizeModId(modId);
            if (!m_modIdIndex.contains(key))
                m_modIdIndex.insert(key, index);
        }
        for (const auto& name : { loadedProject.mainName, loadedProject.secondaryName, loadedProject.abbreviation }) {
            const auto key = normalizeName(name);
            if (!key.isEmpty() && !m_nameIndex.contains(key))
                m_nameIndex.insert(key, index);
        }
    }
    m_loaded = true;
}

std::optional<Match> Index::findByProviderId(const QString& provider, const QString& projectId) const
{
    const auto key = normalizeModId(projectId);
    if (key.isEmpty())
        return std::nullopt;

    const auto normalizedProvider = provider.trimmed().toCaseFolded();
    const auto* index = normalizedProvider == QStringLiteral("curseforge") ? &m_curseForgeIndex
                     : normalizedProvider == QStringLiteral("modrinth") ? &m_modrinthIndex
                                                                          : nullptr;
    if (!index)
        return std::nullopt;
    const auto it = index->constFind(key);
    if (it == index->constEnd())
        return std::nullopt;
    return Match{ m_projects.at(it.value()), QStringLiteral("%1-id").arg(normalizedProvider) };
}

std::optional<Match> Index::find(const QString& modId, const QString& name) const
{
    const auto normalizedId = normalizeModId(modId);
    if (!normalizedId.isEmpty()) {
        const auto it = m_modIdIndex.constFind(normalizedId);
        if (it != m_modIdIndex.constEnd())
            return Match{ m_projects.at(it.value()), QStringLiteral("mod-id") };
    }

    const auto normalizedName = normalizeName(name);
    if (!normalizedName.isEmpty()) {
        const auto it = m_nameIndex.constFind(normalizedName);
        if (it != m_nameIndex.constEnd())
            return Match{ m_projects.at(it.value()), QStringLiteral("name") };
    }
    return std::nullopt;
}

QString Index::translateSearchTerm(const QString& query) const
{
    const auto original = query.trimmed();
    if (original.isEmpty() || !containsChinese(original))
        return query;

    const auto normalized = normalizeName(original);
    const Project* matched = nullptr;

    // Exact aliases are the common path and avoid ambiguous partial matches.
    if (const auto it = m_nameIndex.constFind(normalized); it != m_nameIndex.constEnd())
        matched = &m_projects.at(it.value());

    // Preserve provider-friendly suffixes such as a Minecraft version while
    // translating a Chinese title embedded in a longer search expression.
    if (!matched) {
        for (const auto& project : m_projects) {
            const auto title = project.mainName.trimmed();
            if (!title.isEmpty() && original.contains(title, Qt::CaseInsensitive)) {
                matched = &project;
                break;
            }
        }
    }

    // Also support a Chinese query containing only part of the MCMod title,
    // which is useful for names such as "机械动力" followed by a version tag.
    if (!matched) {
        for (const auto& project : m_projects) {
            const auto mainName = normalizeName(project.mainName);
            if (!mainName.isEmpty() && mainName.contains(normalized)) {
                matched = &project;
                break;
            }
        }
    }

    if (!matched)
        return query;

    // MCMod's secondary name is normally the English name used by the
    // download providers. Fall back to the abbreviation, then preserve the
    // original query when no useful translation was exported.
    for (const auto& candidate : { matched->secondaryName, matched->abbreviation }) {
        const auto value = candidate.trimmed();
        if (!value.isEmpty() && normalizeName(value) != normalized) {
            const auto title = matched->mainName.trimmed();
            const auto position = title.isEmpty() ? -1 : original.indexOf(title, 0, Qt::CaseInsensitive);
            if (position >= 0)
                return original.left(position) + value + original.mid(position + title.size());
            return value;
        }
    }
    return query;
}

QString Index::pageUrl(qint64 id)
{
    return QStringLiteral("https://www.mcmod.cn/class/%1.html").arg(id);
}

QString Index::searchUrl(const QString& query)
{
    QUrl url(QStringLiteral("https://search.mcmod.cn/s"));
    QUrlQuery params;
    params.addQueryItem(QStringLiteral("key"), query);
    params.addQueryItem(QStringLiteral("site"), QStringLiteral("all"));
    params.addQueryItem(QStringLiteral("filter"), QStringLiteral("0"));
    url.setQuery(params);
    return url.toString(QUrl::FullyEncoded);
}

} // namespace McMod
