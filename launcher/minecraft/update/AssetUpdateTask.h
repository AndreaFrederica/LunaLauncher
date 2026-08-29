#pragma once
#include "net/NetJob.h"
#include "tasks/Task.h"
class MinecraftInstance;

enum AssetVerificationMode {
    AlwaysVerify = 0,
    CheckExistence = 1,
    CacheWithExpiry = 2,
    SkipVerification = 3,
};

class AssetUpdateTask : public Task {
    Q_OBJECT
   public:
    AssetUpdateTask(MinecraftInstance* inst, bool forceIntegrityCheck = false);
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
    int m_verificationMode = CheckExistence;
    int m_cacheExpiryDays = 7;
    bool m_forceIntegrityCheck = false;
};
