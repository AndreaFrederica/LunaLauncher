// SPDX-License-Identifier: GPL-3.0-only

#include "InstanceStore.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QSaveFile>

namespace {

QString formatToString(InstanceStoreFormat format)
{
    return format == InstanceStoreFormat::VanillaCompatible ? QStringLiteral("vanilla-compatible") : QStringLiteral("prism");
}

InstanceStoreFormat formatFromString(const QString& value)
{
    return value.compare(QStringLiteral("vanilla-compatible"), Qt::CaseInsensitive) == 0 ? InstanceStoreFormat::VanillaCompatible
                                                                                          : InstanceStoreFormat::Prism;
}

QString originToString(InstanceStoreOrigin origin)
{
    return origin == InstanceStoreOrigin::PortableDerived ? QStringLiteral("portable-derived") : QStringLiteral("user");
}

InstanceStoreOrigin originFromString(const QString& value)
{
    return value.compare(QStringLiteral("portable-derived"), Qt::CaseInsensitive) == 0 ? InstanceStoreOrigin::PortableDerived
                                                                                         : InstanceStoreOrigin::User;
}

QJsonObject storeToJson(const InstanceStore& store)
{
    return QJsonObject{
        { "id", store.id },
        { "name", store.name },
        { "path", store.path },
        { "exists", QFileInfo(store.path).isDir() },
        { "format", formatToString(store.format) },
        { "origin", originToString(store.origin) },
        { "enabled", store.enabled },
        { "writable", store.writable },
        { "defaultForNewInstances", store.defaultForNewInstances },
        { "autoEnableWhenPortable", store.autoEnableWhenPortable },
    };
}

InstanceStore storeFromJson(const QJsonObject& object)
{
    InstanceStore store;
    store.id = object.value("id").toString();
    store.name = object.value("name").toString();
    store.path = object.value("path").toString();
    store.format = formatFromString(object.value("format").toString());
    store.origin = originFromString(object.value("origin").toString());
    store.enabled = object.value("enabled").toBool(true);
    store.writable = object.value("writable").toBool(store.isPrism());
    store.defaultForNewInstances = object.value("defaultForNewInstances").toBool(false);
    store.autoEnableWhenPortable = object.value("autoEnableWhenPortable").toBool(false);
    return store;
}

void setError(QString* error, const QString& message)
{
    if (error)
        *error = message;
}

}  // namespace

InstanceStoreRegistry InstanceStoreRegistry::fromJson(const QJsonDocument& document, QString* error)
{
    InstanceStoreRegistry registry;
    if (document.isNull() || !document.isObject()) {
        setError(error, QStringLiteral("Instance store registry must be a JSON object."));
        return registry;
    }

    const auto root = document.object();
    const auto version = root.value("formatVersion").toInt(CurrentFormatVersion);
    if (version != CurrentFormatVersion) {
        setError(error, QStringLiteral("Unsupported instance store registry format version %1.").arg(version));
        return registry;
    }

    const auto stores = root.value("stores").toArray(root.value("InstanceStores").toArray());
    for (const auto& value : stores) {
        if (!value.isObject())
            continue;
        registry.m_stores.append(storeFromJson(value.toObject()));
    }
    return registry;
}

InstanceStoreRegistry InstanceStoreRegistry::load(const QString& filePath, QString* error)
{
    QFile file(filePath);
    if (!file.exists())
        return {};
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, QStringLiteral("Could not open instance store registry: %1").arg(file.errorString()));
        return {};
    }

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        setError(error, QStringLiteral("Could not parse instance store registry: %1").arg(parseError.errorString()));
        return {};
    }
    return fromJson(document, error);
}

bool InstanceStoreRegistry::save(const QString& filePath, QString* error) const
{
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        setError(error, QStringLiteral("Could not write instance store registry: %1").arg(file.errorString()));
        return false;
    }

    const auto data = toJson().toJson(QJsonDocument::Indented);
    if (file.write(data) != data.size() || !file.commit()) {
        setError(error, QStringLiteral("Could not commit instance store registry: %1").arg(file.errorString()));
        return false;
    }
    return true;
}

QJsonDocument InstanceStoreRegistry::toJson() const
{
    QJsonArray stores;
    for (const auto& store : m_stores)
        stores.append(storeToJson(store));

    return QJsonDocument(QJsonObject{ { "formatVersion", CurrentFormatVersion }, { "InstanceStores", stores } });
}

void InstanceStoreRegistry::normalize(const QString& launcherRoot, bool portable)
{
    QList<InstanceStore> normalized;
    QHash<QString, qsizetype> byPath;

    auto mergeStore = [&normalized, &byPath](InstanceStore store) {
        store.path = normalizePath(store.path);
        if (store.path.isEmpty())
            return;
        store.canonicalPath = normalizePath(store.path);
        if (store.id.isEmpty())
            store.id = makeId(store.canonicalPath);
        if (store.name.isEmpty())
            store.name = QFileInfo(store.path).fileName();
        if (store.name.isEmpty())
            store.name = store.id;

        const auto existing = byPath.find(store.canonicalPath);
        if (existing == byPath.end()) {
            byPath.insert(store.canonicalPath, normalized.size());
            normalized.append(std::move(store));
            return;
        }

        auto& previous = normalized[*existing];
        previous.enabled = previous.enabled || store.enabled;
        previous.writable = previous.writable || store.writable;
        previous.defaultForNewInstances = previous.defaultForNewInstances || store.defaultForNewInstances;
        previous.autoEnableWhenPortable = previous.autoEnableWhenPortable || store.autoEnableWhenPortable;
        if (previous.name.isEmpty() && !store.name.isEmpty())
            previous.name = store.name;
    };

    for (const auto& store : m_stores)
        mergeStore(store);

    if (!launcherRoot.isEmpty()) {
        const auto portablePath = portableInstancesPath(launcherRoot);
        bool found = false;
        for (auto& store : normalized) {
            if (store.canonicalPath == normalizePath(portablePath)) {
                found = true;
                // A derived portable store follows the selected mode while
                // autoEnableWhenPortable remains true. The settings UI clears
                // that flag when the user explicitly disables the candidate.
                if (store.autoEnableWhenPortable && portable) {
                    store.enabled = true;
                    store.defaultForNewInstances = true;
                }
                break;
            }
        }
        if (!found) {
            InstanceStore candidate;
            candidate.id = QStringLiteral("portable-userdata");
            candidate.name = QStringLiteral("便携版实例目录");
            candidate.path = portablePath;
            candidate.format = InstanceStoreFormat::Prism;
            candidate.origin = InstanceStoreOrigin::PortableDerived;
            candidate.enabled = portable;
            candidate.writable = true;
            candidate.defaultForNewInstances = portable;
            candidate.autoEnableWhenPortable = true;
            mergeStore(std::move(candidate));
        }
    }

    bool hasDefault = false;
    for (auto& store : normalized) {
        if (store.defaultForNewInstances && store.enabled && store.writable) {
            hasDefault = true;
            break;
        }
    }
    if (!hasDefault) {
        for (auto& store : normalized) {
            if (store.enabled && store.writable && store.isPrism()) {
                store.defaultForNewInstances = true;
                hasDefault = true;
                break;
            }
        }
    }

    m_stores = std::move(normalized);
}

void InstanceStoreRegistry::addOrUpdate(InstanceStore store)
{
    store.path = normalizePath(store.path);
    store.canonicalPath = normalizePath(store.path);
    if (store.id.isEmpty())
        store.id = makeId(store.canonicalPath);
    if (store.defaultForNewInstances) {
        for (auto& existing : m_stores)
            existing.defaultForNewInstances = false;
    }
    for (auto& existing : m_stores) {
        if (existing.id == store.id) {
            existing = std::move(store);
            return;
        }
        if (existing.canonicalPath == store.canonicalPath) {
            // A path alias must not replace the stable identity already
            // registered for this store. Merge user-visible capability flags
            // while retaining the first registration's id and name.
            existing.enabled = existing.enabled || store.enabled;
            existing.writable = existing.writable || store.writable;
            existing.defaultForNewInstances = existing.defaultForNewInstances || store.defaultForNewInstances;
            existing.autoEnableWhenPortable = existing.autoEnableWhenPortable || store.autoEnableWhenPortable;
            if (existing.name.isEmpty())
                existing.name = store.name;
            return;
        }
    }
    m_stores.append(std::move(store));
}

bool InstanceStoreRegistry::remove(const QString& id)
{
    for (auto it = m_stores.begin(); it != m_stores.end(); ++it) {
        if (it->id == id) {
            m_stores.erase(it);
            return true;
        }
    }
    return false;
}

InstanceStore* InstanceStoreRegistry::find(const QString& id)
{
    for (auto& store : m_stores) {
        if (store.id == id)
            return &store;
    }
    return nullptr;
}

const InstanceStore* InstanceStoreRegistry::find(const QString& id) const
{
    for (const auto& store : m_stores) {
        if (store.id == id)
            return &store;
    }
    return nullptr;
}

InstanceStore* InstanceStoreRegistry::defaultStore()
{
    for (auto& store : m_stores) {
        if (store.defaultForNewInstances && store.enabled && store.writable)
            return &store;
    }
    for (auto& store : m_stores) {
        if (store.enabled && store.writable && store.isPrism())
            return &store;
    }
    return nullptr;
}

const InstanceStore* InstanceStoreRegistry::defaultStore() const
{
    return const_cast<InstanceStoreRegistry*>(this)->defaultStore();
}

QString InstanceStoreRegistry::portableInstancesPath(const QString& launcherRoot)
{
    return QDir::cleanPath(QDir(launcherRoot).filePath(QStringLiteral("UserData/instances")));
}

QString InstanceStoreRegistry::normalizePath(const QString& path)
{
    if (path.trimmed().isEmpty())
        return {};

    const QFileInfo info(path);
    const auto canonical = info.exists() ? info.canonicalFilePath() : info.absoluteFilePath();
    return QDir::cleanPath(canonical);
}

QString InstanceStoreRegistry::makeId(const QString& canonicalPath)
{
    const auto digest = QCryptographicHash::hash(canonicalPath.toUtf8(), QCryptographicHash::Sha256).toHex().left(16);
    return QStringLiteral("store-%1").arg(QString::fromLatin1(digest));
}

