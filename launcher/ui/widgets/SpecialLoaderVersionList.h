// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QJsonArray>

#include "BaseVersionList.h"

class SpecialLoaderVersion final : public BaseVersion {
   public:
    SpecialLoaderVersion(QString loader,
                         QString minecraftVersion,
                         QString loaderVersion,
                         QString type,
                         bool recommended,
                         QString commitRef = {});

    QString descriptor() const override;
    QString name() const override;
    QString typeString() const override;

    QString loader() const { return m_loader; }
    QString minecraftVersion() const { return m_minecraftVersion; }
    QString loaderVersion() const { return m_loaderVersion; }
    QString commitRef() const { return m_commitRef; }
    bool recommended() const { return m_recommended; }

   private:
    QString m_loader;
    QString m_minecraftVersion;
    QString m_loaderVersion;
    QString m_type;
    QString m_commitRef;
    bool m_recommended = false;
};

class SpecialLoaderVersionList final : public BaseVersionList {
    Q_OBJECT
   public:
    explicit SpecialLoaderVersionList(QString loader, QString minecraftVersion, QObject* parent = nullptr);

    Task::Ptr getLoadTask(bool forceReload = false) override;
    bool isLoaded() override { return m_loaded; }
    const BaseVersion::Ptr at(int i) const override { return m_versions.at(i); }
    int count() const override { return m_versions.size(); }
    QVariant data(const QModelIndex& index, int role) const override;
    RoleList providesRoles() const override;
    void sortVersions() override;

    QString loader() const { return m_loader; }
    QString minecraftVersion() const { return m_minecraftVersion; }

    SpecialLoaderVersion* specialVersion(int row) const;

   public slots:
    void setMinecraftVersion(const QString& minecraftVersion);
    void setEntries(const QJsonArray& entries);

   protected slots:
    void updateListData(QList<BaseVersion::Ptr> versions) override;

   private:
    QString m_loader;
    QString m_minecraftVersion;
    QList<BaseVersion::Ptr> m_versions;
    bool m_loaded = false;
};

