// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
*/
#pragma once
#include "ui/pages/BasePage.h"
#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include "server/ServerInstance.h"

class ServerOpsPage : public QWidget, public BasePage
{
    Q_OBJECT

public:
    explicit ServerOpsPage(ServerInstance *instance, QWidget *parent = nullptr);
    virtual ~ServerOpsPage() override;

    QString displayName() const override;
    QIcon icon() const override;
    QString id() const override;
    QString helpPage() const override;
    bool shouldDisplay() const override;

    bool apply() override;

protected:
    void openedImpl() override;

private slots:
    void addOp();
    void removeOp();

private:
    void loadOps();
    void saveOps();

private:
    ServerInstance *m_instance;
    QListWidget *m_opsList;
    QPushButton *m_addButton;
    QPushButton *m_removeButton;
    bool m_fileExistedAtLoad = false;
};
