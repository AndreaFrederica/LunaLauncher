/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
*/
#pragma once
#include "InstanceCreationTask.h"

class ServerInstanceCreationTask : public InstanceCreationTask
{
    Q_OBJECT
public:
    explicit ServerInstanceCreationTask(const QString &exePath, const QStringList &args);
    std::unique_ptr<BaseInstance> createInstance() override;
private:
    QString m_exePath;
    QStringList m_args;
};
