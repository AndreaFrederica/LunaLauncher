/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
*/
#pragma once
#include "ui/pages/BasePage.h"
#include <QWidget>
#include <QTableWidget>
#include <QMap>
#include "server/ServerInstance.h"

class ServerPropertyPage : public QWidget, public BasePage
{
    Q_OBJECT

public:
    explicit ServerPropertyPage(ServerInstance *instance, QWidget *parent = nullptr);
    virtual ~ServerPropertyPage() override;

    QString displayName() const override;
    QIcon icon() const override;
    QString id() const override;
    QString helpPage() const override;
    bool shouldDisplay() const override;

    bool apply() override;

protected:
    void openedImpl() override;

private:
    void loadProps();

private:
    ServerInstance *m_instance;
    QTableWidget *m_table;
    QMap<QString, QString> m_originalProps;
    bool m_fileExistedAtLoad = false;
};
