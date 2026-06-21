// SPDX-License-Identifier: GPL-3.0-only

#include "CleanroomMeta.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QXmlStreamReader>

#include "Application.h"
#include "Exception.h"
#include "FileSystem.h"
#include "Json.h"
#include "archive/ArchiveReader.h"
#include "meta/BaseEntity.h"
#include "meta/Version.h"
#include "meta/VersionList.h"
#include "minecraft/OneSixVersionFormat.h"
#include "net/ApiDownload.h"
#include "net/NetJob.h"

namespace {
constexpr auto CLEANROOM_NAME = "Cleanroom";
constexpr auto CLEANROOM_MINECRAFT_VERSION = "1.12.2";
constexpr auto CLEANROOM_MAVEN_METADATA =
    "https://repo.cleanroommc.com/releases/com/cleanroommc/cleanroom/maven-metadata.xml";
constexpr auto CLEANROOM_MAVEN_BASE = "https://repo.cleanroommc.com/releases/com/cleanroommc/cleanroom/";
QString cleanroomZipUrl(const QString& version)
{
    return QString("%1%2/cleanroom-%2.zip").arg(CLEANROOM_MAVEN_BASE, version);
}

QString metaPath(const QString& filename)
{
    return QDir("meta").absoluteFilePath(filename);
}

bool writeJsonFile(const QString& filename, const QJsonDocument& doc)
{
    if (!FS::ensureFilePathExists(filename)) {
        return false;
    }

    QSaveFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    const auto data = doc.toJson(QJsonDocument::Indented);
    if (file.write(data) != data.size()) {
        return false;
    }
    return file.commit();
}

QJsonArray cleanroomRequires()
{
    QJsonObject minecraft;
    minecraft.insert("uid", "net.minecraft");
    minecraft.insert("equals", CLEANROOM_MINECRAFT_VERSION);
    return { minecraft };
}

QJsonArray cleanroomConflicts()
{
    QJsonArray conflicts;
    for (const auto uid : { "net.minecraftforge", "net.neoforged", "net.fabricmc.fabric-loader", "org.quiltmc.quilt-loader" }) {
        QJsonObject conflict;
        conflict.insert("uid", uid);
        conflicts.append(conflict);
    }
    return conflicts;
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
        version.insert("type", "release");
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

QJsonDocument makeVersionDocument(const QString& versionString)
{
    QJsonObject root;
    root.insert("formatVersion", 1);
    root.insert("uid", Meta::Cleanroom::uid);
    root.insert("name", CLEANROOM_NAME);
    root.insert("version", versionString);
    root.insert("type", "release");
    root.insert("order", 5);
    root.insert("requires", cleanroomRequires());
    root.insert("conflicts", cleanroomConflicts());
    root.insert("releaseTime", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
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

        try {
            const auto doc = makeVersionDocument(m_version->version());
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
    }

    bool abort() override { return m_job ? m_job->abort() : Task::abort(); }

    Meta::Version* m_version = nullptr;
    Net::Mode m_mode;
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

Task::Ptr loadVersionListTask(VersionList* list, Net::Mode mode)
{
    return makeShared<CleanroomListLoadTask>(list, mode);
}

Task::Ptr loadVersionTask(Version* version, Net::Mode mode)
{
    return makeShared<CleanroomVersionLoadTask>(version, mode);
}
}  // namespace Meta::Cleanroom
