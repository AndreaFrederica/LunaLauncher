#include "AssetUpdateTask.h"

#include "BuildConfig.h"
#include "launch/LaunchStep.h"
#include "minecraft/AssetsUtils.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/MirrorDownload.h"
#include "minecraft/PackProfile.h"
#include "net/ChecksumValidator.h"

#include "Application.h"

#include "net/ApiDownload.h"

AssetUpdateTask::AssetUpdateTask(MinecraftInstance* inst)
{
    m_inst = inst;
}

void AssetUpdateTask::executeTask()
{
    setStatus(tr("Updating assets index..."));
    auto components = m_inst->getPackProfile();
    auto profile = components->getProfile();
    auto assets = profile->getMinecraftAssets();
    QUrl indexUrl = assets->url;

    // Rewrite URL for mirror support
    int mirrorType = APPLICATION->settings()->get("DownloadMirrorType").toInt();
    QString replacementUrl;

    if (mirrorType == MirrorDownload::MirrorType::BMCLAPI) {
        replacementUrl = "https://bmclapi2.bangbang93.com";
    } else if (mirrorType == MirrorDownload::MirrorType::Custom) {
        replacementUrl = APPLICATION->settings()->get("MojangDownloadsMirrorURL").toString();
    }

    if (!replacementUrl.isEmpty()) {
        QString urlString = indexUrl.toString();
        // Replace Mojang URLs with mirror URL using QString::replace
        // Ensure replacementUrl ends with / to avoid issues when replacing URLs that include /
        if (!replacementUrl.endsWith('/')) {
            replacementUrl += '/';
        }
        urlString.replace("https://launchermeta.mojang.com/", replacementUrl);
        urlString.replace("https://launcher.mojang.com/", replacementUrl);
        urlString.replace("https://piston-meta.mojang.com/", replacementUrl);
        urlString.replace("http://launchermeta.mojang.com/", replacementUrl);
        urlString.replace("http://launcher.mojang.com/", replacementUrl);
        urlString.replace("http://piston-meta.mojang.com/", replacementUrl);
        urlString.replace("https://s3.amazonaws.com/Minecraft.Download/", replacementUrl);
        urlString.replace("http://s3.amazonaws.com/Minecraft.Download/", replacementUrl);
        indexUrl = QUrl(urlString);
    }

    QString localPath = assets->id + ".json";
    auto job = makeShared<NetJob>(tr("Asset index for %1").arg(m_inst->name()), APPLICATION->network());

    auto metacache = APPLICATION->metacache();
    auto entry = metacache->resolveEntry("asset_indexes", localPath);
    entry->setStale(true);
    auto hexSha1 = assets->sha1.toLatin1();
    qDebug() << "Asset index SHA1:" << hexSha1;
    auto dl = Net::ApiDownload::makeCached(indexUrl, entry);
    dl->addValidator(new Net::ChecksumValidator(QCryptographicHash::Sha1, assets->sha1));
    job->addNetAction(dl);

    downloadJob.reset(job);

    connect(downloadJob.get(), &NetJob::succeeded, this, &AssetUpdateTask::assetIndexFinished);
    connect(downloadJob.get(), &NetJob::failed, this, &AssetUpdateTask::assetIndexFailed);
    connect(downloadJob.get(), &NetJob::aborted, this, &AssetUpdateTask::emitAborted);
    connect(downloadJob.get(), &NetJob::progress, this, &AssetUpdateTask::progress);
    connect(downloadJob.get(), &NetJob::stepProgress, this, &AssetUpdateTask::propagateStepProgress);

    qDebug() << "Starting asset index download for" << m_inst->name();
    downloadJob->start();
}

bool AssetUpdateTask::canAbort() const
{
    return true;
}

void AssetUpdateTask::assetIndexFinished()
{
    AssetsIndex index;
    qDebug() << "Finished asset index download for" << m_inst->name();

    auto components = m_inst->getPackProfile();
    auto profile = components->getProfile();
    auto assets = profile->getMinecraftAssets();

    QString asset_fname = "assets/indexes/" + assets->id + ".json";
    // FIXME: this looks like a job for a generic validator based on json schema?
    if (!AssetsUtils::loadAssetsIndexJson(assets->id, asset_fname, index)) {
        auto metacache = APPLICATION->metacache();
        auto entry = metacache->resolveEntry("asset_indexes", assets->id + ".json");
        metacache->evictEntry(entry);
        emitFailed(tr("Failed to read the assets index!"));
        return;
    }

    auto job = index.getDownloadJob();
    if (job) {
        QString resourceURL = resourceUrl();
        QString source = tr("Mojang");
        if (resourceURL != BuildConfig.DEFAULT_RESOURCE_BASE) {
            source = QUrl(resourceURL).host();
        }
        setStatus(tr("Getting the asset files from %1...").arg(source));
        downloadJob = job;
        connect(downloadJob.get(), &NetJob::succeeded, this, &AssetUpdateTask::emitSucceeded);
        connect(downloadJob.get(), &NetJob::failed, this, &AssetUpdateTask::assetsFailed);
        connect(downloadJob.get(), &NetJob::aborted, this, &AssetUpdateTask::emitAborted);
        connect(downloadJob.get(), &NetJob::progress, this, &AssetUpdateTask::progress);
        connect(downloadJob.get(), &NetJob::stepProgress, this, &AssetUpdateTask::propagateStepProgress);
        downloadJob->start();
        return;
    }
    emitSucceeded();
}

void AssetUpdateTask::assetIndexFailed(QString reason)
{
    qDebug() << m_inst->name() << ": Failed asset index download";
    emitFailed(tr("Failed to download the assets index:\n%1").arg(reason));
}

void AssetUpdateTask::assetsFailed(QString reason)
{
    emitFailed(tr("Failed to download assets:\n%1").arg(reason));
}

bool AssetUpdateTask::abort()
{
    if (downloadJob) {
        return downloadJob->abort();
    } else {
        qWarning() << "Prematurely aborted AssetUpdateTask";
    }
    return true;
}

QString AssetUpdateTask::resourceUrl()
{
    if (const QString urlOverride = APPLICATION->settings()->get("ResourceURLOverride").toString(); !urlOverride.isEmpty()) {
        return urlOverride;
    }

    return BuildConfig.DEFAULT_RESOURCE_BASE;
}
