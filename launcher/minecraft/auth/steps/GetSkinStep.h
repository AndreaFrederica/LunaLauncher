#pragma once
#include <QObject>

#include "minecraft/auth/AuthStep.h"
#include "net/Download.h"
#include "net/NetJob.h"

class GetSkinStep : public AuthStep {
    Q_OBJECT

   public:
    explicit GetSkinStep(AccountData* data);
    virtual ~GetSkinStep() noexcept = default;

    void perform() override;

    QString describe() override;

   private:
    NetJob::Ptr m_task;
};
