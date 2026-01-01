/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
*/
#pragma once
#include "ui/pages/BasePage.h"
#include <QWidget>

class ServerInstance;
class JavaSettingsWidget;

class ServerJavaPage : public QWidget, public BasePage
{
    Q_OBJECT
public:
    explicit ServerJavaPage(ServerInstance *instance, QWidget *parent = nullptr);
    virtual ~ServerJavaPage() override;

    QString displayName() const override;
    QIcon icon() const override;
    QString id() const override;
    QString helpPage() const override;
    bool shouldDisplay() const override;

    bool apply() override;
    void load();

private:
    ServerInstance *m_instance;
    JavaSettingsWidget *m_javaWidget;
};
