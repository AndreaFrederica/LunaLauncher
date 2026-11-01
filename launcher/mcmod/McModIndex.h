// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace McMod {

struct Project {
    qint64 id = 0;
    QString curseForgeId;
    QString modrinthId;
    QString mainName;
    QString secondaryName;
    QString abbreviation;
    QStringList modIds;
};

struct Match {
    Project project;
    QString matchedBy;
};

/**
 * Read-only index generated from an authorized MCMod export.
 *
 * MCMod is intentionally kept separate from ResourceAPI: this index provides
 * names and cross-links, while Modrinth/CurseForge remain the download APIs.
 */
class Index {
public:
    static const Index& instance();

    std::optional<Match> find(const QString& modId, const QString& name) const;
    /** Reverse lookup a MCMod title/alias from a user search expression. */
    std::optional<Match> findByName(const QString& query) const;
    /** Find an entry by its Modrinth or CurseForge project ID. */
    std::optional<Match> findByProviderId(const QString& provider, const QString& projectId) const;
    /**
     * Translate a Chinese MCMod name to the recorded English search name.
     * Queries that are not Chinese or do not match the index are returned as-is.
     */
    QString translateSearchTerm(const QString& query) const;
    static QString pageUrl(qint64 id);
    static QString searchUrl(const QString& query);
    bool isLoaded() const { return m_loaded; }
    int size() const { return m_projects.size(); }

private:
    Index();

    static QString normalizeName(QString value);
    static QString normalizeModId(QString value);
    static QStringList parseCsvLine(const QByteArray& line);
    static bool containsChinese(const QString& value);
    void load();

    QList<Project> m_projects;
    QHash<QString, int> m_modIdIndex;
    QHash<QString, int> m_curseForgeIndex;
    QHash<QString, int> m_modrinthIndex;
    QHash<QString, int> m_nameIndex;
    bool m_loaded = false;
};

} // namespace McMod
