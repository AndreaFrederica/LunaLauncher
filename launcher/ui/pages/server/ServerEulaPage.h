// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
*/
#pragma once
#include "ui/pages/BasePage.h"
#include <QWidget>
#include <QCheckBox>
#include <QTextEdit>
#include "server/ServerInstance.h"

class ServerEulaPage : public QWidget, public BasePage
{
    Q_OBJECT

public:
    explicit ServerEulaPage(ServerInstance *instance, QWidget *parent = nullptr);
    virtual ~ServerEulaPage() override;

    QString displayName() const override;
    QIcon icon() const override;
    QString id() const override;
    QString helpPage() const override;
    bool shouldDisplay() const override;

    bool apply() override;

protected:
    void openedImpl() override;

private:
    void loadEula();
    void updateEulaPreview();

private:
    ServerInstance *m_instance;
    QCheckBox *m_eulaCheckbox;
    QTextEdit *m_eulaText;
    bool m_fileExistedAtLoad = false;
    bool m_initialAgreed = false;
};
