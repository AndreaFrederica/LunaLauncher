// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QJsonDocument>
#include <QList>
#include <QString>

enum class InstanceStoreFormat {
    Prism,
    VanillaCompatible,
};

enum class InstanceStoreOrigin {
    User,
    PortableDerived,
};

struct InstanceStore {
    QString id;
    QString name;
    QString path;
    QString canonicalPath;
    InstanceStoreFormat format = InstanceStoreFormat::Prism;
    InstanceStoreOrigin origin = InstanceStoreOrigin::User;
    bool enabled = true;
    bool writable = true;
    bool defaultForNewInstances = false;
    bool autoEnableWhenPortable = false;

    bool isPrism() const { return format == InstanceStoreFormat::Prism; }
    bool isVanillaCompatible() const { return format == InstanceStoreFormat::VanillaCompatible; }
};

class InstanceStoreRegistry {
   public:
    static constexpr int CurrentFormatVersion = 1;

    static InstanceStoreRegistry fromJson(const QJsonDocument& document, QString* error = nullptr);
    static InstanceStoreRegistry load(const QString& filePath, QString* error = nullptr);

    bool save(const QString& filePath, QString* error = nullptr) const;
    QJsonDocument toJson() const;

    void normalize(const QString& launcherRoot, bool portable);

    const QList<InstanceStore>& stores() const { return m_stores; }
    QList<InstanceStore>& stores() { return m_stores; }

    void addOrUpdate(InstanceStore store);
    bool remove(const QString& id);
    InstanceStore* find(const QString& id);
    const InstanceStore* find(const QString& id) const;
    InstanceStore* defaultStore();
    const InstanceStore* defaultStore() const;

    static QString portableInstancesPath(const QString& launcherRoot);
    static QString normalizePath(const QString& path);
    static QString makeId(const QString& canonicalPath);

   private:
    QList<InstanceStore> m_stores;
};

