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

class ServerPlayerListPage : public QWidget, public BasePage
{
    Q_OBJECT

public:
    enum ListType {
        Whitelist,
        BannedPlayers,
        BannedIPs
    };

    explicit ServerPlayerListPage(ServerInstance *instance, ListType type, QWidget *parent = nullptr);
    virtual ~ServerPlayerListPage() override;

    QString displayName() const override;
    QIcon icon() const override;
    QString id() const override;
    QString helpPage() const override;
    bool shouldDisplay() const override;

    bool apply() override;

protected:
    void openedImpl() override;

private slots:
    void addEntry();
    void removeEntry();

private:
    void loadList();
    void saveList();
    QString getFilePath() const;
    QString getFileName() const;

private:
    ServerInstance *m_instance;
    ListType m_listType;
    QListWidget *m_list;
    QPushButton *m_addButton;
    QPushButton *m_removeButton;
    bool m_fileExistedAtLoad = false;
};
