// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QJsonObject>

struct SyncProfile {
    QString id;
    QString name;
    QString clientInstanceId;
    QString serverInstanceId;
    QStringList serverInstanceIds;
    QStringList commonMods;
    QStringList clientOnlyMods;
    QStringList serverOnlyMods;
    QStringList commonConfigs;
    QStringList clientOnlyConfigs;
    QStringList serverOnlyConfigs;
    QString mode = QStringLiteral("bidirectional");
    bool autoAfterUpdate = false;
    QString conflictPolicy = QStringLiteral("ask");
    // Last synchronized content hashes, keyed by relative path. An empty
    // value represents a file that did not exist at the last sync.
    QJsonObject baseHashes;
    // Base file contents are retained for text three-way merges. Values are
    // base64 encoded UTF-8; binary files remain hash-only and conflict.
    QJsonObject baseContents;

    static SyncProfile fromJson(const QJsonObject& object, QString* error = nullptr);
    QJsonObject toJson() const;
};

struct SyncFileChange {
    QString relativePath;
    QString direction;
    QString category;
    QString reason;
    QByteArray content;
};

struct SyncPlan {
    bool valid = true;
    QStringList errors;
    QList<SyncFileChange> changes;
    QStringList conflicts;
    QJsonObject toJson() const;
};

class SyncEngine {
   public:
    static SyncPlan plan(const SyncProfile& profile, const QString& clientRoot, const QString& serverRoot);
    static bool apply(SyncProfile& profile, const SyncPlan& plan, const QString& clientRoot, const QString& serverRoot, QString* error = nullptr);
    static QString profilePath(const QString& dataRoot, const QString& id);
    static bool saveProfile(const QString& dataRoot, const SyncProfile& profile, QString* error = nullptr);
    static SyncProfile loadProfile(const QString& dataRoot, const QString& id, QString* error = nullptr);
};

