#pragma once
#include "net/NetJob.h"
#include "tasks/Task.h"
class MinecraftInstance;

/// Asset verification/cache mode enumeration
enum AssetVerificationMode {
    AlwaysVerify = 0,     ///< Always re-download index and verify all files (original behavior)
    CheckExistence = 1,   ///< Only download if file missing or size mismatch (new default)
    CacheWithExpiry = 2,  ///< Use HttpMetaCache expiry mechanism (ETag / max-age)
    SkipVerification = 3, ///< Skip all asset checks entirely
};

class AssetUpdateTask : public Task {
    Q_OBJECT
   public:
    AssetUpdateTask(MinecraftInstance* inst);
    virtual ~AssetUpdateTask() = default;

    void executeTask() override;

    bool canAbort() const override;

   public:
    static QString resourceUrl();

   private slots:
    void assetIndexFinished();
    void assetIndexFailed(QString reason);
    void assetsFailed(QString reason);

   public slots:
    bool abort() override;

   private:
    MinecraftInstance* m_inst;
    NetJob::Ptr downloadJob;
};
