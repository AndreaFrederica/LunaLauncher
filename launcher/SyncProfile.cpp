// SPDX-License-Identifier: GPL-3.0-only
#include "SyncProfile.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QSaveFile>
#include <QSet>

namespace {
void setError(QString* error, const QString& value) { if (error) *error = value; }
QStringList stringList(const QJsonValue& value)
{
    QStringList result;
    for (const auto& item : value.toArray()) if (item.isString()) result.append(item.toString());
    return result;
}
QJsonArray array(const QStringList& values) { QJsonArray result; for (const auto& value : values) result.append(value); return result; }
QString hashFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return QString::fromLatin1(QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256).toHex());
}
QByteArray readFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return file.readAll();
}
bool safeRelativePath(const QString& value)
{
    const auto relative = QDir::cleanPath(value);
    return !relative.isEmpty() && !relative.startsWith("../") && relative != ".." && !QDir::isAbsolutePath(relative);
}
QString threeWayTextMerge(const QByteArray& base, const QByteArray& client, const QByteArray& server, bool* merged)
{
    *merged = false;
    if (client == server) { *merged = true; return QString::fromUtf8(client); }
    if (client == base) { *merged = true; return QString::fromUtf8(server); }
    if (server == base) { *merged = true; return QString::fromUtf8(client); }
    const auto b = QString::fromUtf8(base).split('\n');
    const auto c = QString::fromUtf8(client).split('\n');
    const auto s = QString::fromUtf8(server).split('\n');
    if (b.size() != c.size() || b.size() != s.size()) return {};
    QStringList output;
    for (int i = 0; i < b.size(); ++i) {
        if (c.at(i) == s.at(i)) output.append(c.at(i));
        else if (c.at(i) == b.at(i)) output.append(s.at(i));
        else if (s.at(i) == b.at(i)) output.append(c.at(i));
        else return {};
    }
    *merged = true;
    return output.join('\n');
}
bool copyOne(const QString& source, const QString& target, QString* error)
{
    if (!QFileInfo::exists(source)) { QFile::remove(target); return true; }
    if (!QDir().mkpath(QFileInfo(target).absolutePath())) { setError(error, QStringLiteral("Could not create %1").arg(QFileInfo(target).absolutePath())); return false; }
    QSaveFile file(target);
    if (!file.open(QIODevice::WriteOnly)) { setError(error, file.errorString()); return false; }
    QFile input(source);
    if (!input.open(QIODevice::ReadOnly)) { setError(error, input.errorString()); return false; }
    const auto data = input.readAll();
    if (file.write(data) != data.size() || !file.commit()) { setError(error, file.errorString()); return false; }
    return true;
}
void appendCategory(QList<SyncFileChange>& changes, const QStringList& values, const QString& category, const QString& from, const QString& to, const QString& srcRoot, const QString& dstRoot, QStringList& conflicts)
{
    for (const auto& value : values) {
        const auto relative = QDir::cleanPath(value);
        if (!safeRelativePath(relative)) { conflicts.append(relative); continue; }
        const auto source = QDir(srcRoot).filePath(relative);
        const auto target = QDir(dstRoot).filePath(relative);
        const bool sourceExists = QFileInfo(source).isFile();
        const bool targetExists = QFileInfo(target).isFile();
        if (!sourceExists && !targetExists) continue;
        const auto sourceHash = sourceExists ? hashFile(source) : QString();
        const auto targetHash = targetExists ? hashFile(target) : QString();
        if (sourceHash == targetHash) continue;
        if (sourceExists && targetExists) conflicts.append(relative);
        else changes.append({ relative, from + "->" + to, category, sourceExists ? "source-only" : "target-only" });
    }
}

void appendBidirectional(QList<SyncFileChange>& changes, const QStringList& values, const QString& category, const QString& clientRoot, const QString& serverRoot, const QJsonObject& baseHashes, const QJsonObject& baseContents, QStringList& conflicts)
{
    for (const auto& value : values) {
        const auto relative = QDir::cleanPath(value);
        if (!safeRelativePath(relative)) { conflicts.append(relative); continue; }
        const auto clientPath = QDir(clientRoot).filePath(relative);
        const auto serverPath = QDir(serverRoot).filePath(relative);
        const bool clientExists = QFileInfo(clientPath).isFile();
        const bool serverExists = QFileInfo(serverPath).isFile();
        if (!clientExists && !serverExists) continue;
        const auto clientHash = clientExists ? hashFile(clientPath) : QString();
        const auto serverHash = serverExists ? hashFile(serverPath) : QString();
        if (clientHash == serverHash)
            continue;

        // A recorded base lets us distinguish a one-sided update from a real
        // concurrent edit. Without a base, retain the conservative legacy
        // behavior and report two existing, different files as a conflict.
        const auto base = baseHashes.value(relative).toString();
        if (!base.isEmpty() || baseHashes.contains(relative)) {
            if (clientHash == base && serverHash != base) {
                changes.append({ relative, QStringLiteral("server->client"), category, QStringLiteral("server-changed") });
            } else if (serverHash == base && clientHash != base) {
                changes.append({ relative, QStringLiteral("client->server"), category, QStringLiteral("client-changed") });
            } else if (category == QStringLiteral("config") && baseContents.contains(relative)) {
                bool merged = false;
                const auto result = threeWayTextMerge(QByteArray::fromBase64(baseContents.value(relative).toString().toLatin1()), readFile(clientPath), readFile(serverPath), &merged);
                if (merged)
                    changes.append({ relative, QStringLiteral("merge"), category, QStringLiteral("three-way-merge"), result.toUtf8() });
                else
                    conflicts.append(relative);
            } else {
                conflicts.append(relative);
            }
        } else if (clientExists && serverExists) {
            conflicts.append(relative);
        } else if (clientExists) {
            changes.append({ relative, QStringLiteral("client->server"), category, QStringLiteral("client-only") });
        } else {
            changes.append({ relative, QStringLiteral("server->client"), category, QStringLiteral("server-only") });
        }
    }
}
}

SyncProfile SyncProfile::fromJson(const QJsonObject& object, QString* error)
{
    SyncProfile profile;
    if (object.value("id").toString().trimmed().isEmpty()) { setError(error, QStringLiteral("Sync profile id is required.")); return {}; }
    profile.id = object.value("id").toString(); profile.name = object.value("name").toString(profile.id);
    profile.clientInstanceId = object.value("client").toObject().value("instanceId").toString();
    profile.serverInstanceId = object.value("server").toObject().value("instanceId").toString();
    if (profile.clientInstanceId.isEmpty() || profile.serverInstanceId.isEmpty()) { setError(error, QStringLiteral("Both client and server instance IDs are required.")); return {}; }
    const auto mods = object.value("mods").toObject(); profile.commonMods = stringList(mods.value("common")); profile.clientOnlyMods = stringList(mods.value("clientOnly")); profile.serverOnlyMods = stringList(mods.value("serverOnly"));
    const auto configs = object.value("configs").toObject(); profile.commonConfigs = stringList(configs.value("common")); profile.clientOnlyConfigs = stringList(configs.value("clientOnly")); profile.serverOnlyConfigs = stringList(configs.value("serverOnly"));
    const auto serverObject = object.value("server").toObject();
    for (const auto& value : serverObject.value("instanceIds").toArray()) if (value.isString()) profile.serverInstanceIds.append(value.toString());
    if (profile.serverInstanceIds.isEmpty() && !profile.serverInstanceId.isEmpty()) profile.serverInstanceIds.append(profile.serverInstanceId);
    const auto sync = object.value("sync").toObject(); profile.mode = sync.value("mode").toString(profile.mode); profile.autoAfterUpdate = sync.value("autoAfterUpdate").toBool(false); profile.conflictPolicy = sync.value("conflictPolicy").toString(profile.conflictPolicy); profile.baseHashes = sync.value("baseHashes").toObject(); profile.baseContents = sync.value("baseContents").toObject();
    return profile;
}

QJsonObject SyncProfile::toJson() const
{
    return { { "formatVersion", 1 }, { "id", id }, { "name", name }, { "client", QJsonObject{ { "instanceId", clientInstanceId } } }, { "server", QJsonObject{ { "instanceId", serverInstanceId }, { "instanceIds", QJsonArray::fromStringList(serverInstanceIds.isEmpty() ? QStringList{ serverInstanceId } : serverInstanceIds) } } },
             { "mods", QJsonObject{ { "common", array(commonMods) }, { "clientOnly", array(clientOnlyMods) }, { "serverOnly", array(serverOnlyMods) } } },
             { "configs", QJsonObject{ { "common", array(commonConfigs) }, { "clientOnly", array(clientOnlyConfigs) }, { "serverOnly", array(serverOnlyConfigs) } } },
             { "sync", QJsonObject{ { "mode", mode }, { "autoAfterUpdate", autoAfterUpdate }, { "conflictPolicy", conflictPolicy }, { "baseHashes", baseHashes }, { "baseContents", baseContents } } } };
}

QJsonObject SyncPlan::toJson() const
{
    QJsonArray changesJson; for (const auto& change : changes) { auto item = QJsonObject{ { "path", change.relativePath }, { "direction", change.direction }, { "category", change.category }, { "reason", change.reason } }; if (!change.content.isEmpty()) item.insert("content", QString::fromUtf8(change.content)); changesJson.append(item); }
    QJsonArray conflictsJson; for (const auto& conflict : conflicts) conflictsJson.append(conflict);
    QJsonArray errorsJson; for (const auto& error : errors) errorsJson.append(error);
    return { { "valid", valid }, { "errors", errorsJson }, { "changes", changesJson }, { "conflicts", conflictsJson } };
}

SyncPlan SyncEngine::plan(const SyncProfile& profile, const QString& clientRoot, const QString& serverRoot)
{
    SyncPlan result;
    if (profile.mode != "bidirectional" && profile.mode != "client-to-server" && profile.mode != "server-to-client") { result.valid = false; result.errors.append(QStringLiteral("Unsupported sync mode.")); return result; }
    if (profile.mode == "bidirectional") {
        appendBidirectional(result.changes, profile.commonMods, "mod", clientRoot, serverRoot, profile.baseHashes, profile.baseContents, result.conflicts);
        appendBidirectional(result.changes, profile.commonConfigs, "config", clientRoot, serverRoot, profile.baseHashes, profile.baseContents, result.conflicts);
    } else if (profile.mode == "client-to-server") {
        appendCategory(result.changes, profile.commonMods, "mod", "client", "server", clientRoot, serverRoot, result.conflicts);
        appendCategory(result.changes, profile.commonConfigs, "config", "client", "server", clientRoot, serverRoot, result.conflicts);
    } else {
        appendCategory(result.changes, profile.commonMods, "mod", "server", "client", serverRoot, clientRoot, result.conflicts);
        appendCategory(result.changes, profile.commonConfigs, "config", "server", "client", serverRoot, clientRoot, result.conflicts);
    }
    // Explicitly account for one-sided declarations in the preview. They are
    // intentionally never copied to the other side, but showing them keeps a
    // clientOnly/serverOnly rule from silently disappearing from the plan.
    const auto annotateOwned = [&result](const QStringList& values, const QString& category, const QString& owner, const QString& root) {
        for (const auto& value : values) {
            if (!safeRelativePath(value) || !QFileInfo(QDir(root).filePath(value)).isFile()) continue;
            result.changes.append({ QDir::cleanPath(value), QStringLiteral("informational"), category, owner + QStringLiteral("-only") });
        }
    };
    annotateOwned(profile.clientOnlyMods, QStringLiteral("mod"), QStringLiteral("client"), clientRoot);
    annotateOwned(profile.clientOnlyConfigs, QStringLiteral("config"), QStringLiteral("client"), clientRoot);
    annotateOwned(profile.serverOnlyMods, QStringLiteral("mod"), QStringLiteral("server"), serverRoot);
    annotateOwned(profile.serverOnlyConfigs, QStringLiteral("config"), QStringLiteral("server"), serverRoot);
    if (!result.conflicts.isEmpty() && profile.conflictPolicy == "ask") result.valid = false;
    return result;
}

bool SyncEngine::apply(SyncProfile& profile, const SyncPlan& syncPlan, const QString& clientRoot, const QString& serverRoot, QString* error)
{
    if (!syncPlan.valid || !syncPlan.conflicts.isEmpty()) { setError(error, QStringLiteral("The sync plan contains conflicts and cannot be applied automatically.")); return false; }
    for (const auto& change : syncPlan.changes) {
        if (change.direction == QStringLiteral("merge")) {
            if (!safeRelativePath(change.relativePath) || change.content.isEmpty()) { setError(error, QStringLiteral("Invalid merge content.")); return false; }
            const auto clientTarget = QDir(clientRoot).filePath(change.relativePath);
            const auto serverTarget = QDir(serverRoot).filePath(change.relativePath);
            if (!copyOne(clientTarget, serverTarget, error)) return false;
            if (!QDir().mkpath(QFileInfo(clientTarget).absolutePath())) { setError(error, QStringLiteral("Could not create merge directory.")); return false; }
            QSaveFile merged(clientTarget);
            if (!merged.open(QIODevice::WriteOnly) || merged.write(change.content) != change.content.size() || !merged.commit()) { setError(error, merged.errorString()); return false; }
            if (!copyOne(clientTarget, serverTarget, error)) return false;
            continue;
        }
        const auto parts = change.direction.split("->"); if (parts.size() != 2 || parts[0] == parts[1]) continue;
        const auto sourceRoot = parts[0] == "client" ? clientRoot : serverRoot; const auto targetRoot = parts[1] == "client" ? clientRoot : serverRoot;
        if (!copyOne(QDir(sourceRoot).filePath(change.relativePath), QDir(targetRoot).filePath(change.relativePath), error)) return false;
    }
    // Record the post-sync state as the new common base. This makes the next
    // plan a three-way comparison and prevents silent overwrites after both
    // sides have changed since the previous synchronization.
    const auto record = [&profile, &clientRoot, &serverRoot](const QStringList& values) {
        for (const auto& value : values) {
            const auto relative = QDir::cleanPath(value);
            if (!safeRelativePath(relative))
                continue;
            const auto clientPath = QDir(clientRoot).filePath(relative);
            const auto serverPath = QDir(serverRoot).filePath(relative);
            const auto clientHash = QFileInfo(clientPath).isFile() ? hashFile(clientPath) : QString();
            const auto serverHash = QFileInfo(serverPath).isFile() ? hashFile(serverPath) : QString();
            Q_UNUSED(serverHash);
            profile.baseHashes.insert(relative, clientHash);
            profile.baseContents.insert(relative, QFileInfo(clientPath).isFile() ? QString::fromLatin1(readFile(clientPath).toBase64()) : QString());
        }
    };
    record(profile.commonMods);
    record(profile.commonConfigs);
    return true;
}

QString SyncEngine::profilePath(const QString& dataRoot, const QString& id) { return QDir(dataRoot).filePath(QStringLiteral("sync-profiles/%1.json").arg(id)); }

bool SyncEngine::saveProfile(const QString& dataRoot, const SyncProfile& profile, QString* error)
{
    if (profile.id.isEmpty()) { setError(error, QStringLiteral("Sync profile id is required.")); return false; }
    const auto path = profilePath(dataRoot, profile.id); if (!QDir().mkpath(QFileInfo(path).absolutePath())) { setError(error, QStringLiteral("Could not create sync profile directory.")); return false; }
    QSaveFile file(path); if (!file.open(QIODevice::WriteOnly)) { setError(error, file.errorString()); return false; }
    const auto data = QJsonDocument(profile.toJson()).toJson(QJsonDocument::Indented); if (file.write(data) != data.size() || !file.commit()) { setError(error, file.errorString()); return false; } return true;
}

SyncProfile SyncEngine::loadProfile(const QString& dataRoot, const QString& id, QString* error)
{
    QFile file(profilePath(dataRoot, id)); if (!file.open(QIODevice::ReadOnly)) { setError(error, file.errorString()); return {}; }
    QJsonParseError parseError; const auto document = QJsonDocument::fromJson(file.readAll(), &parseError); if (parseError.error != QJsonParseError::NoError || !document.isObject()) { setError(error, parseError.errorString()); return {}; }
    return SyncProfile::fromJson(document.object(), error);
}

