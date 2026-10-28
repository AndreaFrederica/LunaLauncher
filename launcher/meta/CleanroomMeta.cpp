// SPDX-License-Identifier: GPL-3.0-only

#include "CleanroomMeta.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QSaveFile>
#include <QSet>
#include <QTemporaryDir>
#include <QXmlStreamReader>
#include <utility>

#include "Application.h"
#include "Exception.h"
#include "FileSystem.h"
#include "Json.h"
#include "archive/ArchiveReader.h"
#include "meta/BaseEntity.h"
#include "meta/Version.h"
#include "meta/VersionList.h"
#include "net/ApiDownload.h"
#include "net/NetJob.h"

namespace {
constexpr auto CLEANROOM_NAME = "Cleanroom";
constexpr auto CLEANROOM_MINECRAFT_VERSION = "1.12.2";
constexpr auto CLEANROOM_MAVEN_METADATA =
    "https://repo.cleanroommc.com/releases/com/cleanroommc/cleanroom/maven-metadata.xml";
constexpr auto CLEANROOM_MAVEN_BASE = "https://repo.cleanroommc.com/releases/com/cleanroommc/cleanroom/";
constexpr auto CLEANROOM_GITHUB_RELEASE_BASE = "https://github.com/CleanroomMC/Cleanroom/releases/download/";
QString cleanroomZipUrl(const QString& version)
{
    // The 0.3 releases predate the Maven zip publication and use the old GitHub asset name.
    if (version.startsWith("0.3.")) {
        return QString("%1%2/Cleanroom-MMC-instance-%2.zip").arg(CLEANROOM_GITHUB_RELEASE_BASE, version);
    }
    return QString("%1%2/cleanroom-%2.zip").arg(CLEANROOM_MAVEN_BASE, version);
}

QString metaPath(const QString& filename)
{
    return QDir("meta").absoluteFilePath(filename);
}

bool writeFile(const QString& filename, const QByteArray& data)
{
    if (!FS::ensureFilePathExists(filename)) {
        return false;
    }

    QSaveFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    if (file.write(data) != data.size()) {
        return false;
    }
    return file.commit();
}

bool writeJsonFile(const QString& filename, const QJsonDocument& doc)
{
    return writeFile(filename, doc.toJson(QJsonDocument::Indented));
}

QJsonArray cleanroomRequires()
{
    QJsonObject minecraft;
    minecraft.insert("uid", "net.minecraft");
    minecraft.insert("equals", CLEANROOM_MINECRAFT_VERSION);
    return { minecraft };
}

QString cleanroomVersionType(const QString& version)
{
    if (version.contains("-alpha", Qt::CaseInsensitive)) {
        return QStringLiteral("alpha");
    }
    if (version.contains("-beta", Qt::CaseInsensitive)) {
        return QStringLiteral("beta");
    }
    return QStringLiteral("release");
}

QDateTime parseLastUpdated(const QString& value)
{
    auto time = QDateTime::fromString(value, "yyyyMMddHHmmss");
    if (time.isValid()) {
        time.setTimeSpec(Qt::UTC);
        return time;
    }
    return QDateTime::currentDateTimeUtc();
}

QJsonDocument makeListDocument(const QByteArray& xmlData)
{
    QXmlStreamReader xml(xmlData);
    QString latest;
    QString lastUpdated;
    QStringList versions;

    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement()) {
            continue;
        }
        if (xml.name() == "latest") {
            latest = xml.readElementText();
        } else if (xml.name() == "lastUpdated") {
            lastUpdated = xml.readElementText();
        } else if (xml.name() == "version") {
            versions.append(xml.readElementText());
        }
    }
    if (xml.hasError()) {
        throw Exception(QObject::tr("Failed to parse Cleanroom Maven metadata: %1").arg(xml.errorString()));
    }
    if (versions.isEmpty()) {
        throw Exception(QObject::tr("Cleanroom Maven metadata did not contain any versions."));
    }

    const auto baseTime = parseLastUpdated(lastUpdated);
    QJsonArray versionArray;
    for (int i = 0; i < versions.size(); ++i) {
        const auto& versionString = versions.at(i);
        QJsonObject version;
        version.insert("version", versionString);
        version.insert("type", cleanroomVersionType(versionString));
        version.insert("releaseTime", baseTime.addSecs(i - versions.size()).toString(Qt::ISODate));
        version.insert("requires", cleanroomRequires());
        version.insert("recommended", versionString == latest);
        versionArray.append(version);
    }

    QJsonObject root;
    root.insert("formatVersion", 1);
    root.insert("uid", Meta::Cleanroom::uid);
    root.insert("name", CLEANROOM_NAME);
    root.insert("versions", versionArray);
    return QJsonDocument(root);
}

void loadLocalJson(Meta::BaseEntity* entity)
{
    const auto filename = metaPath(entity->localFilename());
    const auto doc = Json::requireDocument(FS::read(filename), filename);
    entity->parse(Json::requireObject(doc, filename));
    entity->setLoadStatus(Meta::BaseEntity::LoadStatus::Remote);
}

class CleanroomListLoadTask : public Task {
   public:
    CleanroomListLoadTask(Meta::VersionList* list, Net::Mode mode) : m_list(list), m_mode(mode) {}

   private:
    void executeTask() override
    {
        setStatus(QObject::tr("Loading Cleanroom versions"));
        if (QFile::exists(metaPath(m_list->localFilename()))) {
            try {
                loadLocalJson(m_list);
                if (m_mode == Net::Mode::Offline) {
                    emitSucceeded();
                    return;
                }
            } catch (const Exception& e) {
                qWarning() << "Failed to load cached Cleanroom version list:" << e.cause();
            }
        } else if (m_mode == Net::Mode::Offline) {
            emitFailed(QObject::tr("Cleanroom version metadata is not cached."));
            return;
        }

        auto pair = Net::ApiDownload::makeByteArray(QUrl(CLEANROOM_MAVEN_METADATA));
        m_response = pair.second;
        m_job.reset(new NetJob(QObject::tr("Download Cleanroom version metadata"), APPLICATION->network()));
        m_job->addNetAction(pair.first);
        m_job->setAskRetry(false);
        connect(m_job.get(), &Task::succeeded, this, [this] {
            try {
                const auto doc = makeListDocument(*m_response);
                const auto path = metaPath(m_list->localFilename());
                if (!writeJsonFile(path, doc)) {
                    throw Exception(QObject::tr("Failed to write Cleanroom version metadata cache."));
                }
                m_list->parse(doc.object());
                m_list->setLoadStatus(Meta::BaseEntity::LoadStatus::Remote);
                emitSucceeded();
            } catch (const Exception& e) {
                emitFailed(e.cause());
            }
        });
        connect(m_job.get(), &Task::failed, this, [this](const QString& reason) { emitFailed(reason); });
        connect(m_job.get(), &Task::progress, this, &Task::setProgress);
        m_job->start();
    }

    bool abort() override { return m_job ? m_job->abort() : Task::abort(); }

    Meta::VersionList* m_list = nullptr;
    Net::Mode m_mode;
    QByteArray* m_response = nullptr;
    NetJob::Ptr m_job;
};

struct CleanroomPackage {
    QJsonArray components;
    QMap<QString, QJsonObject> patches;
};

QByteArray readArchiveFile(MMCZip::ArchiveReader& archive, const QString& path)
{
    auto file = archive.goToFile(path);
    if (!file) {
        throw Exception(QObject::tr("Cleanroom package does not contain %1.").arg(path));
    }

    int readStatus = 0;
    auto data = file->readAll(&readStatus);
    if (readStatus < 0 || data.isEmpty()) {
        throw Exception(QObject::tr("Failed to read %1 from the Cleanroom package.").arg(path));
    }
    return data;
}

QJsonObject readArchiveJson(MMCZip::ArchiveReader& archive, const QString& path)
{
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(readArchiveFile(archive, path), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        throw Exception(QObject::tr("Cleanroom package contains invalid JSON in %1: %2").arg(path, error.errorString()));
    }
    return document.object();
}

CleanroomPackage parseCleanroomPackage(const QString& requestedVersion, const QByteArray& zipData)
{
    QTemporaryDir tempDir(QDir::temp().absoluteFilePath("cleanroom-meta-XXXXXX"));
    if (!tempDir.isValid()) {
        throw Exception(QObject::tr("Failed to create temp dir for Cleanroom zip."));
    }
    const auto zipPath = tempDir.filePath(QStringLiteral("cleanroom.zip"));
    {
        QFile zipFile(zipPath);
        if (!zipFile.open(QIODevice::WriteOnly) || zipFile.write(zipData) != zipData.size()) {
            throw Exception(QObject::tr("Failed to write Cleanroom zip to temp file."));
        }
        zipFile.close();
    }

    MMCZip::ArchiveReader archive(zipPath);
    const auto packRoot = readArchiveJson(archive, QStringLiteral("mmc-pack.json"));
    const auto componentValues = packRoot.value("components");
    if (!componentValues.isArray()) {
        throw Exception(QObject::tr("Cleanroom package has no valid component list."));
    }

    CleanroomPackage package;
    bool hasMinecraft = false;
    bool hasLwjgl3 = false;
    bool hasCleanroom = false;
    for (const auto& componentValue : componentValues.toArray()) {
        if (!componentValue.isObject()) {
            throw Exception(QObject::tr("Cleanroom package contains an invalid component entry."));
        }

        auto component = componentValue.toObject();
        const auto packageUid = component.value("uid").toString();
        if (packageUid.isEmpty()) {
            throw Exception(QObject::tr("Cleanroom package contains a component without a UID."));
        }

        auto normalizedUid = packageUid;
        if (packageUid == QLatin1String("net.minecraftforge")) {
            normalizedUid = QLatin1String(Meta::Cleanroom::uid);
        }
        if (package.patches.contains(normalizedUid)) {
            throw Exception(QObject::tr("Cleanroom package contains duplicate component %1.").arg(normalizedUid));
        }

        auto patch = readArchiveJson(archive, QStringLiteral("patches/%1.json").arg(packageUid));
        component.insert("uid", normalizedUid);
        patch.insert("uid", normalizedUid);

        if (normalizedUid == QLatin1String(Meta::Cleanroom::uid)) {
            const auto packageVersion = component.value("version").toString();
            if (packageVersion != requestedVersion) {
                throw Exception(QObject::tr("Cleanroom package version %1 does not match requested version %2.")
                                    .arg(packageVersion, requestedVersion));
            }
            component.insert("cachedName", QLatin1String(CLEANROOM_NAME));
            patch.insert("name", QLatin1String(CLEANROOM_NAME));
            patch.insert("type", cleanroomVersionType(requestedVersion));
            hasCleanroom = true;
        } else if (normalizedUid == QLatin1String("net.minecraft")) {
            hasMinecraft = true;
        } else if (normalizedUid == QLatin1String("org.lwjgl3")) {
            hasLwjgl3 = true;
        }

        package.components.append(component);
        package.patches.insert(normalizedUid, patch);
    }

    if (!hasMinecraft || !hasLwjgl3 || !hasCleanroom) {
        throw Exception(QObject::tr("Cleanroom package is missing its Minecraft, LWJGL 3, or Cleanroom component."));
    }
    return package;
}

QJsonDocument makeVersionDocumentFromPatch(const QString& versionString, const QByteArray& zipData)
{
    const auto package = parseCleanroomPackage(versionString, zipData);
    return QJsonDocument(package.patches.value(QLatin1String(Meta::Cleanroom::uid)));
}

struct FileBackup {
    QString path;
    bool existed = false;
    QByteArray data;
};

FileBackup backupFile(const QString& path)
{
    FileBackup backup{ path, QFile::exists(path), {} };
    if (!backup.existed) {
        return backup;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        throw Exception(QObject::tr("Failed to back up %1: %2").arg(path, file.errorString()));
    }
    backup.data = file.readAll();
    return backup;
}

void restoreFiles(const QList<FileBackup>& backups)
{
    for (auto it = backups.crbegin(); it != backups.crend(); ++it) {
        bool restored = false;
        if (it->existed) {
            restored = writeFile(it->path, it->data);
        } else {
            restored = !QFile::exists(it->path) || QFile::remove(it->path);
        }
        if (!restored) {
            qCritical() << "Failed to restore Cleanroom update backup:" << it->path;
        }
    }
}

void applyCleanroomPackage(const QString& instanceRoot, const CleanroomPackage& package)
{
    const auto packPath = FS::PathCombine(instanceRoot, "mmc-pack.json");
    QFile packFile(packPath);
    if (!packFile.open(QIODevice::ReadOnly)) {
        throw Exception(QObject::tr("Failed to read %1: %2").arg(packPath, packFile.errorString()));
    }

    QJsonParseError error;
    const auto packData = packFile.readAll();
    packFile.close();
    auto packDocument = QJsonDocument::fromJson(packData, &error);
    if (error.error != QJsonParseError::NoError || !packDocument.isObject()) {
        throw Exception(QObject::tr("Failed to parse %1: %2").arg(packPath, error.errorString()));
    }

    auto packRoot = packDocument.object();
    const auto currentComponentValue = packRoot.value("components");
    if (!currentComponentValue.isArray()) {
        throw Exception(QObject::tr("Instance has no valid component list."));
    }

    QMap<QString, QJsonObject> incomingComponents;
    QStringList incomingOrder;
    for (const auto& componentValue : package.components) {
        const auto component = componentValue.toObject();
        const auto componentUid = component.value("uid").toString();
        incomingComponents.insert(componentUid, component);
        incomingOrder.append(componentUid);
    }

    QJsonArray mergedComponents;
    QSet<QString> replacedComponents;
    for (const auto& componentValue : currentComponentValue.toArray()) {
        if (!componentValue.isObject()) {
            throw Exception(QObject::tr("Instance contains an invalid component entry."));
        }

        const auto currentComponent = componentValue.toObject();
        const auto currentUid = currentComponent.value("uid").toString();
        if (currentUid == QLatin1String("org.lwjgl")) {
            continue;
        }

        if (incomingComponents.contains(currentUid)) {
            auto replacement = incomingComponents.value(currentUid);
            if (currentUid == QLatin1String(Meta::Cleanroom::uid) && currentComponent.value("disabled").toBool()) {
                replacement.insert("disabled", true);
            }
            if (!replacedComponents.contains(currentUid)) {
                mergedComponents.append(replacement);
                replacedComponents.insert(currentUid);
            }
        } else {
            mergedComponents.append(currentComponent);
        }
    }
    for (const auto& uid : incomingOrder) {
        if (!replacedComponents.contains(uid)) {
            mergedComponents.append(incomingComponents.value(uid));
        }
    }
    packRoot.insert("components", mergedComponents);

    QList<FileBackup> backups;
    backups.append(backupFile(packPath));
    for (auto it = package.patches.cbegin(); it != package.patches.cend(); ++it) {
        backups.append(backupFile(FS::PathCombine(instanceRoot, "patches", it.key() + ".json")));
    }
    const auto legacyLwjglPatch = FS::PathCombine(instanceRoot, "patches", "org.lwjgl.json");
    if (!package.patches.contains(QStringLiteral("org.lwjgl"))) {
        backups.append(backupFile(legacyLwjglPatch));
    }

    try {
        for (auto it = package.patches.cbegin(); it != package.patches.cend(); ++it) {
            const auto path = FS::PathCombine(instanceRoot, "patches", it.key() + ".json");
            if (!writeJsonFile(path, QJsonDocument(it.value()))) {
                throw Exception(QObject::tr("Failed to write %1.").arg(path));
            }
        }
        if (!writeJsonFile(packPath, QJsonDocument(packRoot))) {
            throw Exception(QObject::tr("Failed to write %1.").arg(packPath));
        }
        if (QFile::exists(legacyLwjglPatch) && !QFile::remove(legacyLwjglPatch)) {
            throw Exception(QObject::tr("Failed to remove obsolete patch %1.").arg(legacyLwjglPatch));
        }
    } catch (const Exception&) {
        restoreFiles(backups);
        throw;
    }
}

class CleanroomVersionLoadTask : public Task {
   public:
    CleanroomVersionLoadTask(Meta::Version* version, Net::Mode mode) : m_version(version), m_mode(mode) {}

   private:
    void executeTask() override
    {
        setStatus(QObject::tr("Loading Cleanroom %1").arg(m_version->version()));
        if (QFile::exists(metaPath(m_version->localFilename()))) {
            try {
                loadLocalJson(m_version);
                if (m_mode == Net::Mode::Offline) {
                    emitSucceeded();
                    return;
                }
            } catch (const Exception& e) {
                qWarning() << "Failed to load cached Cleanroom version:" << e.cause();
            }
        } else if (m_mode == Net::Mode::Offline) {
            emitFailed(QObject::tr("Cleanroom %1 metadata is not cached.").arg(m_version->version()));
            return;
        }

        // Download the MMC zip from Maven and extract the patch file as
        // version metadata.  This gives ComponentUpdateTask the +libraries,
        // mainClass, and other fields it needs to install/update the loader.
        // TODO: Cache the downloaded zip to avoid re-downloading on every resolve.
        auto pair = Net::ApiDownload::makeByteArray(QUrl(cleanroomZipUrl(m_version->version())));
        m_response = pair.second;
        m_job.reset(new NetJob(QObject::tr("Download Cleanroom %1 package").arg(m_version->version()), APPLICATION->network()));
        m_job->addNetAction(pair.first);
        m_job->setAskRetry(false);
        connect(m_job.get(), &Task::succeeded, this, [this] {
            try {
                const auto doc = makeVersionDocumentFromPatch(m_version->version(), *m_response);
                const auto path = metaPath(m_version->localFilename());
                if (!writeJsonFile(path, doc)) {
                    throw Exception(QObject::tr("Failed to write Cleanroom metadata cache."));
                }
                m_version->parse(doc.object());
                m_version->setLoadStatus(Meta::BaseEntity::LoadStatus::Remote);
                emitSucceeded();
            } catch (const Exception& e) {
                emitFailed(e.cause());
            }
        });
        connect(m_job.get(), &Task::failed, this, [this](const QString& reason) { emitFailed(reason); });
        connect(m_job.get(), &Task::progress, this, &Task::setProgress);
        m_job->start();
    }

    bool abort() override { return m_job ? m_job->abort() : Task::abort(); }

    Meta::Version* m_version = nullptr;
    Net::Mode m_mode;
    QByteArray* m_response = nullptr;
    NetJob::Ptr m_job;
};

class CleanroomInstanceUpdateTask : public Task {
   public:
    CleanroomInstanceUpdateTask(QString instanceRoot, QString version)
        : m_instanceRoot(std::move(instanceRoot)), m_version(std::move(version))
    {}

   private:
    void executeTask() override
    {
        setStatus(QObject::tr("Downloading Cleanroom %1 package").arg(m_version));
        auto pair = Net::ApiDownload::makeByteArray(QUrl(cleanroomZipUrl(m_version)));
        m_response = pair.second;
        m_job.reset(new NetJob(QObject::tr("Download Cleanroom %1 package").arg(m_version), APPLICATION->network()));
        m_job->addNetAction(pair.first);
        m_job->setAskRetry(false);
        connect(m_job.get(), &Task::succeeded, this, [this] {
            try {
                setStatus(QObject::tr("Updating Cleanroom to %1").arg(m_version));
                applyCleanroomPackage(m_instanceRoot, parseCleanroomPackage(m_version, *m_response));
                emitSucceeded();
            } catch (const Exception& e) {
                emitFailed(e.cause());
            }
        });
        connect(m_job.get(), &Task::failed, this, [this](const QString& reason) { emitFailed(reason); });
        connect(m_job.get(), &Task::progress, this, &Task::setProgress);
        m_job->start();
    }

    bool abort() override { return m_job ? m_job->abort() : Task::abort(); }

    QString m_instanceRoot;
    QString m_version;
    QByteArray* m_response = nullptr;
    NetJob::Ptr m_job;
};
}  // namespace

namespace Meta::Cleanroom {
bool isUid(const QString& uid)
{
    return uid == QLatin1String(::Meta::Cleanroom::uid);
}

QString packageUrl(const QString& version)
{
    return cleanroomZipUrl(version);
}

bool normalizeInstance(const QString& root)
{
    constexpr auto legacyUid = "net.minecraftforge";

    const auto packPath = FS::PathCombine(root, "mmc-pack.json");
    QFile packFile(packPath);
    if (!packFile.open(QIODevice::ReadOnly)) {
        return false;
    }

    QJsonParseError packError;
    const auto packData = packFile.readAll();
    packFile.close();
    auto packDocument = QJsonDocument::fromJson(packData, &packError);
    if (packError.error != QJsonParseError::NoError || !packDocument.isObject()) {
        return false;
    }

    auto packRoot = packDocument.object();
    auto components = packRoot.value("components").toArray();
    qsizetype legacyComponentIndex = -1;
    bool componentIdentifiesCleanroom = false;
    for (qsizetype i = 0; i < components.size(); ++i) {
        const auto component = components.at(i).toObject();
        const auto componentUid = component.value("uid").toString();
        if (componentUid == QLatin1String(uid)) {
            return false;
        }
        if (componentUid == QLatin1String(legacyUid)) {
            legacyComponentIndex = i;
            componentIdentifiesCleanroom =
                component.value("cachedName").toString().compare(CLEANROOM_NAME, Qt::CaseInsensitive) == 0;
        }
    }
    if (legacyComponentIndex < 0) {
        return false;
    }

    const auto patchesDir = FS::PathCombine(root, "patches");
    const auto legacyPatchPath = FS::PathCombine(patchesDir, "net.minecraftforge.json");
    const auto cleanroomPatchPath = FS::PathCombine(patchesDir, "com.cleanroommc.cleanroom.json");

    QFile patchFile(legacyPatchPath);
    if (!patchFile.open(QIODevice::ReadOnly)) {
        return false;
    }

    QJsonParseError patchError;
    const auto patchData = patchFile.readAll();
    patchFile.close();
    auto patchDocument = QJsonDocument::fromJson(patchData, &patchError);
    if (patchError.error != QJsonParseError::NoError || !patchDocument.isObject()) {
        return false;
    }

    auto patchRoot = patchDocument.object();
    bool patchIdentifiesCleanroom = patchRoot.value("name").toString().compare(CLEANROOM_NAME, Qt::CaseInsensitive) == 0;
    for (const auto libraryValue : patchRoot.value("libraries").toArray()) {
        if (libraryValue.toObject().value("name").toString().startsWith("com.cleanroommc:cleanroom:")) {
            patchIdentifiesCleanroom = true;
            break;
        }
    }
    if (!componentIdentifiesCleanroom && !patchIdentifiesCleanroom) {
        return false;
    }

    auto component = components.at(legacyComponentIndex).toObject();
    component.insert("uid", QLatin1String(uid));
    components.replace(legacyComponentIndex, component);
    packRoot.insert("components", components);

    patchRoot.insert("uid", QLatin1String(uid));
    patchRoot.insert("name", CLEANROOM_NAME);

    // Write the new patch before switching the component UID so the instance
    // remains loadable if either atomic save fails.
    if (!writeJsonFile(cleanroomPatchPath, QJsonDocument(patchRoot))) {
        return false;
    }
    if (!writeJsonFile(packPath, QJsonDocument(packRoot))) {
        return false;
    }

    QFile::remove(legacyPatchPath);
    return true;
}

Task::Ptr loadVersionListTask(VersionList* list, Net::Mode mode)
{
    return makeShared<CleanroomListLoadTask>(list, mode);
}

Task::Ptr loadVersionTask(Version* version, Net::Mode mode)
{
    return makeShared<CleanroomVersionLoadTask>(version, mode);
}

Task::Ptr updateInstanceTask(const QString& instanceRoot, const QString& version)
{
    return makeShared<CleanroomInstanceUpdateTask>(instanceRoot, version);
}
}  // namespace Meta::Cleanroom
