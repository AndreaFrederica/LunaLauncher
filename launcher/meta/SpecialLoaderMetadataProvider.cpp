// SPDX-License-Identifier: GPL-3.0-only

#include "SpecialLoaderMetadataProvider.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <functional>
#include <utility>

#include "api/LauncherApiSpecial.h"
#include "meta/Version.h"
#include "meta/VersionList.h"

namespace {

class SpecialLoaderTask final : public Task {
   public:
    explicit SpecialLoaderTask(std::function<void(SpecialLoaderTask*)> action) : m_action(std::move(action)) {}

   protected:
    void executeTask() override
    {
        if (m_action)
            m_action(this);
    }

   public:
    void succeed() { emitSucceeded(); }
    void fail(const QString& error) { emitFailed(error); }

   private:
    std::function<void(SpecialLoaderTask*)> m_action;
};

QStringList descriptorParts(const QString& descriptor)
{
    return descriptor.split('|', Qt::KeepEmptyParts);
}

}  // namespace

namespace Meta {

bool SpecialLoaderMetadataProvider::handlesUid(const QString& uid) const
{
    return uid == QLatin1String("net.legacyfabric") || uid == QLatin1String("net.labymod");
}

QStringList SpecialLoaderMetadataProvider::knownUids() const
{
    return { QLatin1String("net.legacyfabric"), QLatin1String("net.labymod") };
}

Task::Ptr SpecialLoaderMetadataProvider::loadVersionListTask(VersionList*) const
{
    // The official endpoints require a Minecraft version for a useful list.
    // The native UI owns the filtered list; component updates only need the
    // version detail task below.
    return makeShared<SpecialLoaderTask>([](SpecialLoaderTask* task) { task->succeed(); });
}

Task::Ptr SpecialLoaderMetadataProvider::loadVersionTask(Version* version) const
{
    return makeShared<SpecialLoaderTask>([version](SpecialLoaderTask* task) {
        if (!version) {
            task->fail(QObject::tr("The special loader version is unavailable."));
            return;
        }
        const auto parts = descriptorParts(version->version());
        if (parts.size() < 2 || parts.at(0).isEmpty() || parts.at(1).isEmpty()) {
            task->fail(QObject::tr("The special loader version does not contain a Minecraft version."));
            return;
        }
        const auto minecraft = parts.at(0);
        const auto loaderVersion = parts.at(1);
        const auto commit = parts.size() > 2 ? parts.at(2) : QString();
        const auto loader = version->uid() == QLatin1String("net.legacyfabric")
                                ? QStringLiteral("legacyfabric")
                                : (parts.size() > 3 && parts.at(3) == QLatin1String("labymod-snapshot") ? QStringLiteral("labymod-snapshot")
                                                                                                             : QStringLiteral("labymod-production"));
        QString error;
        auto profile = SpecialLoader::profile(loader, minecraft, loaderVersion, commit, &error);
        if (!error.isEmpty()) {
            task->fail(error);
            return;
        }
        profile.insert("uid", version->uid());
        profile.insert("version", version->version());
        profile.insert("name", loader == QLatin1String("legacyfabric") ? QStringLiteral("Legacy Fabric") : QStringLiteral("LabyMod"));
        profile.insert("type", "release");
        profile.insert("releaseTime", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        profile.insert("requires", QJsonArray{ QJsonObject{ { "uid", "net.minecraft" }, { "equals", minecraft } } });
        try {
            version->parse(profile);
        } catch (const std::exception& e) {
            task->fail(QString::fromUtf8(e.what()));
            return;
        }
        task->succeed();
    });
}

}  // namespace Meta

