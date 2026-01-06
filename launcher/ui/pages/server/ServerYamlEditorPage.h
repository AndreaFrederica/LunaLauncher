// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
*/
#pragma once
#include "ui/pages/BasePage.h"
#include <QWidget>
#include <QPlainTextEdit>
#include "server/ServerInstance.h"

class ServerYamlEditorPage : public QWidget, public BasePage
{
    Q_OBJECT

public:
    enum FileType {
        Bukkit,
        Spigot
    };

    explicit ServerYamlEditorPage(ServerInstance *instance, FileType type, QWidget *parent = nullptr);
    virtual ~ServerYamlEditorPage() override;

    QString displayName() const override;
    QIcon icon() const override;
    QString id() const override;
    QString helpPage() const override;
    bool shouldDisplay() const override;

    bool apply() override;

protected:
    void openedImpl() override;

private:
    void loadFile();
    void saveFile();
    QString getFilePath() const;

private:
    ServerInstance *m_instance;
    FileType m_fileType;
    QPlainTextEdit *m_editor;
    QString m_originalText;
};
