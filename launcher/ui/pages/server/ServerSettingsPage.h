/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
*/
#pragma once
#include "ui/pages/BasePage.h"
#include <QWidget>
#include <QLineEdit>
#include <QPushButton>

class ServerInstance;

class ServerSettingsPage : public QWidget, public BasePage
{
    Q_OBJECT
public:
    explicit ServerSettingsPage(ServerInstance *instance, QWidget *parent = nullptr);
    virtual ~ServerSettingsPage() override;

    QString displayName() const override;
    QIcon icon() const override;
    QString id() const override;
    QString helpPage() const override;
    bool shouldDisplay() const override;

    bool apply() override;
    void load();

private slots:
    void onBrowse();
    void updatePreview();

private:
    ServerInstance *m_instance;
    QLineEdit *m_exePathEdit;
    QLineEdit *m_argsEdit;
    QLineEdit *m_workDirEdit;
    QPushButton *m_browseBtn;
    class QLabel *m_previewLabel;
};
