// SPDX-License-Identifier: GPL-3.0-only

#include "SpecialLoaderVersionList.h"

#include <algorithm>
#include <QPointer>

#include "api/LauncherApiSpecial.h"
#include "tasks/Task.h"

namespace {

class SpecialLoaderListTask final : public Task {
   public:
    explicit SpecialLoaderListTask(SpecialLoaderVersionList* list) : m_list(list) {}

   protected:
    void executeTask() override
    {
        if (!m_list) {
            emitFailed(QObject::tr("The special loader version list is unavailable."));
            return;
        }
        setStatus(QObject::tr("Loading %1 versions...").arg(m_list->loader()));
        QString error;
        const auto entries = SpecialLoader::versions(m_list->loader(), m_list->minecraftVersion(), &error);
        if (!error.isEmpty()) {
            emitFailed(error);
            return;
        }
        m_list->setEntries(entries);
        emitSucceeded();
    }

   private:
    QPointer<SpecialLoaderVersionList> m_list;
};

}  // namespace

SpecialLoaderVersion::SpecialLoaderVersion(QString loader,
                                           QString minecraftVersion,
                                           QString loaderVersion,
                                           QString type,
                                           bool recommended,
                                           QString commitRef)
    : m_loader(std::move(loader)),
      m_minecraftVersion(std::move(minecraftVersion)),
      m_loaderVersion(std::move(loaderVersion)),
      m_type(std::move(type)),
      m_commitRef(std::move(commitRef)),
      m_recommended(recommended)
{}

QString SpecialLoaderVersion::descriptor() const
{
    // Keep the Minecraft version in the persisted component version. This is
    // necessary because both official providers reuse the same loader version
    // number across multiple Minecraft versions.
    auto result = m_minecraftVersion + QLatin1Char('|') + m_loaderVersion;
    if (!m_commitRef.isEmpty())
        result += QLatin1Char('|') + m_commitRef + QLatin1Char('|') + m_loader;
    return result;
}

QString SpecialLoaderVersion::name() const
{
    return m_loaderVersion;
}

QString SpecialLoaderVersion::typeString() const
{
    return m_type;
}

SpecialLoaderVersionList::SpecialLoaderVersionList(QString loader, QString minecraftVersion, QObject* parent)
    : BaseVersionList(parent), m_loader(std::move(loader)), m_minecraftVersion(std::move(minecraftVersion))
{}

Task::Ptr SpecialLoaderVersionList::getLoadTask(bool forceReload)
{
    Q_UNUSED(forceReload);
    return makeShared<SpecialLoaderListTask>(this);
}

QVariant SpecialLoaderVersionList::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_versions.size())
        return {};
    const auto version = std::dynamic_pointer_cast<SpecialLoaderVersion>(m_versions.at(index.row()));
    if (!version)
        return {};
    switch (role) {
        case VersionPointerRole:
            return QVariant::fromValue(m_versions.at(index.row()));
        case VersionRole:
            return version->name();
        case VersionIdRole:
            return version->descriptor();
        case ParentVersionRole:
            return version->minecraftVersion();
        case RecommendedRole:
            return version->recommended();
        case TypeRole:
            return version->typeString();
        case SortRole:
            return index.row();
        default:
            return {};
    }
}

BaseVersionList::RoleList SpecialLoaderVersionList::providesRoles() const
{
    return { VersionPointerRole, VersionRole, VersionIdRole, ParentVersionRole, RecommendedRole, TypeRole, SortRole };
}

void SpecialLoaderVersionList::sortVersions()
{
    beginResetModel();
    std::sort(m_versions.begin(), m_versions.end(), [](const auto& a, const auto& b) { return a->name() > b->name(); });
    endResetModel();
}

SpecialLoaderVersion* SpecialLoaderVersionList::specialVersion(int row) const
{
    if (row < 0 || row >= m_versions.size())
        return nullptr;
    return dynamic_cast<SpecialLoaderVersion*>(m_versions.at(row).get());
}

void SpecialLoaderVersionList::setMinecraftVersion(const QString& minecraftVersion)
{
    if (m_minecraftVersion == minecraftVersion)
        return;
    m_minecraftVersion = minecraftVersion;
    m_loaded = false;
    beginResetModel();
    m_versions.clear();
    endResetModel();
}

void SpecialLoaderVersionList::setEntries(const QJsonArray& entries)
{
    QList<BaseVersion::Ptr> versions;
    versions.reserve(entries.size());
    for (const auto& value : entries) {
        const auto object = value.toObject();
        const auto minecraft = object.value("minecraftVersion").toString(m_minecraftVersion);
        const auto loaderVersion = object.value("loaderVersion").toString(object.value("version").toString());
        if (minecraft.isEmpty() || loaderVersion.isEmpty())
            continue;
        versions.append(std::make_shared<SpecialLoaderVersion>(m_loader,
                                                                minecraft,
                                                                loaderVersion,
                                                                object.value("type").toString(),
                                                                object.value("recommended").toBool(),
                                                                object.value("commitRef").toString()));
    }
    beginResetModel();
    m_versions = std::move(versions);
    m_loaded = true;
    endResetModel();
}

void SpecialLoaderVersionList::updateListData(QList<BaseVersion::Ptr> versions)
{
    beginResetModel();
    m_versions = std::move(versions);
    m_loaded = true;
    endResetModel();
}

