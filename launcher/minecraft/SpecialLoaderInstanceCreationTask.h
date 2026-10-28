// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "InstanceCreationTask.h"

class SpecialLoaderInstanceCreationTask final : public InstanceCreationTask {
    Q_OBJECT
   public:
    SpecialLoaderInstanceCreationTask(BaseVersion::Ptr minecraftVersion,
                                      QString loader,
                                      QString loaderVersion,
                                      QString commitRef = {});

   protected:
    std::unique_ptr<BaseInstance> createInstance() override;

   private:
    BaseVersion::Ptr m_minecraftVersion;
    QString m_loader;
    QString m_loaderVersion;
    QString m_commitRef;
};

