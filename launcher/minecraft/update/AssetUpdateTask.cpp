#include "AssetUpdateTask.h"

#include "BuildConfig.h"
#include "FileSystem.h"
#include "launch/LaunchStep.h"
#include "minecraft/AssetsUtils.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/MirrorDownload.h"
#include "minecraft/PackProfile.h"
#include "net/ChecksumValidator.h"

#include "Application.h"

#include "net/ApiDownload.h"

#include <QFileInfo>

AssetUpdateTask::AssetUpdateTask(MinecraftInstance* inst, bool forceIntegrityCheck)
    : m_inst(inst), m_forceIntegrityCheck(forceIntegrityCheck)
{
}

void AssetUpdateTask::executeTask()
{
    setStatus(tr("Updating assets index..."));
    auto components = m_inst->getPackProfile();
    auto profile = components->getProfile();
    auto assets = profile->getMinecraftAssets();
    if (m_inst->usesLocalAssets()) {
        const auto indexPath = FS::PathCombine(m_inst->assetsRoot(), "indexes", assets->id + ".json");
        const auto virtualPath = FS::PathCombine(m_inst->assetsRoot(), "virtual", assets->id);
        if (!QFileInfo::exists(indexPath) && !QFileInfo::exists(virtualPath)) {
            emitFailed(tr("The PCL pack's instance-local assets are missing the %1 index and virtual directory.").arg(assets->id));
            return;
        }
        setStatus(tr("Using assets bundled with the PCL pack"));
        emitSucceeded();
        return;
    }
    QUrl indexUrl = assets->url;

    // Rewrite the asset index URL through the shared mirror implementation so
    // both the BMCLAPI preset and custom mirrors use the same host mapping.
    const QUrl officialIndexUrl = indexUrl;
    indexUrl = QUrl(MirrorDownload::rewriteUrl(indexUrl.toString(), APPLICATION->settings()));

    QString localPath = assets->id + ".json";
    auto job = makeShared<NetJob>(tr("Asset index for %1").arg(m_inst->name()), APPLICATION->network());

    m_verificationMode = m_inst->settings()->get("AssetVerificationMode").toInt();
    if (m_verificationMode < AssetVerificationMode::AlwaysVerify || m_verificationMode > AssetVerificationMode::SkipVerification)
        m_verificationMode = AssetVerificationMode::CheckExistence;
    m_cacheExpiryDays = qBound(1, m_inst->settings()->get("AssetCacheExpiryDays").toInt(), 365);
    if (!m_forceIntegrityCheck && m_inst->settings()->get("AssetIntegrityTrusted").toBool())
        m_verificationMode = AssetVerificationMode::SkipVerification;

    auto metacache = APPLICATION->metacache();
    if (m_verificationMode == AssetVerificationMode::CacheWithExpiry) {
        // resolveEntry() checks age immediately; apply the policy first.
        if (auto cachedEntry = metacache->getEntry("asset_indexes", localPath)) {
            cachedEntry->setMaximumAge(static_cast<qint64>(m_cacheExpiryDays) * 86400LL);
            metacache->updateEntry(cachedEntry);
        }
    }
    auto entry = metacache->resolveEntry("asset_indexes", localPath);

    if (m_verificationMode == AssetVerificationMode::AlwaysVerify) {
        entry->setStale(true);
    } else if (m_verificationMode == AssetVerificationMode::SkipVerification && QFileInfo(entry->getFullPath()).isFile()) {
        // Skip all asset checks when the local index is available.
        emitSucceeded();
        return;
    } else if (m_verificationMode == AssetVerificationMode::SkipVerification) {
        // Without an index there is nothing safe to skip; repair it normally.
        m_verificationMode = AssetVerificationMode::CheckExistence;
    }
    auto hexSha1 = assets->sha1.toLatin1();
    qDebug() << "Asset index SHA1:" << hexSha1;
    auto dl = Net::ApiDownload::makeCached(indexUrl, entry);
    dl->addValidator(new Net::ChecksumValidator(QCryptographicHash::Sha1, assets->sha1));
    if (officialIndexUrl != indexUrl)
        dl->setFallbackUrl(officialIndexUrl);
    job->addNetAction(dl);

    downloadJob.reset(job);

    connect(downloadJob.get(), &NetJob::succeeded, this, &AssetUpdateTask::assetIndexFinished);
    connect(downloadJob.get(), &NetJob::failed, this, &AssetUpdateTask::assetIndexFailed);
    connect(downloadJob.get(), &NetJob::aborted, this, &AssetUpdateTask::emitAborted);
    connect(downloadJob.get(), &NetJob::progress, this, &AssetUpdateTask::setProgress);
    connect(downloadJob.get(), &NetJob::stepProgress, this, &AssetUpdateTask::propagateStepProgress);
    connect(downloadJob.get(), &NetJob::transferRate, this, &AssetUpdateTask::setTransferRate);

    qDebug() << "Starting asset index download for" << m_inst->name();
    downloadJob->start();
}

bool AssetUpdateTask::canAbort() const
{
    return true;
}

void AssetUpdateTask::assetIndexFinished()
{
    if (m_verificationMode == AssetVerificationMode::CacheWithExpiry) {
        auto assets = m_inst->getPackProfile()->getProfile()->getMinecraftAssets();
        if (auto entry = APPLICATION->metacache()->getEntry("asset_indexes", assets->id + ".json")) {
            entry->setMaximumAge(static_cast<qint64>(m_cacheExpiryDays) * 86400LL);
            entry->setCurrentAge(0);
            entry->setStale(false);
            APPLICATION->metacache()->updateEntry(entry);
        }
    }

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

    auto job = index.getDownloadJob(m_forceIntegrityCheck || m_verificationMode == AssetVerificationMode::AlwaysVerify);
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
        connect(downloadJob.get(), &NetJob::progress, this, &AssetUpdateTask::setProgress);
        connect(downloadJob.get(), &NetJob::stepProgress, this, &AssetUpdateTask::propagateStepProgress);
        connect(downloadJob.get(), &NetJob::transferRate, this, &AssetUpdateTask::setTransferRate);
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
    auto settings = APPLICATION->settings();

    // A manually configured assets base always wins.
    if (const QString urlOverride = settings->get("ResourceURLOverride").toString(); !urlOverride.isEmpty()) {
        return urlOverride;
    }

    // Otherwise let the mirror resolve it (BMCLAPI preset or a custom base).
    const auto mirror = MirrorDownload::resolve(settings);
    if (!mirror.assetsBase.isEmpty())
        return mirror.assetsBase;

    return BuildConfig.DEFAULT_RESOURCE_BASE;
}
