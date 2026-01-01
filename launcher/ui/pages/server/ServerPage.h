/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
*/
#pragma once
#include "ui/pages/BasePage.h"
#include <QWidget>
#include <QLineEdit>
#include <QPushButton>

class ServerPage : public QWidget, public BasePage
{
    Q_OBJECT
public:
    explicit ServerPage(QWidget *parent = nullptr);
    virtual ~ServerPage() override;

    QString displayName() const override;
    QIcon icon() const override;
    QString id() const override;
    QString helpPage() const override;
    bool shouldDisplay() const override;

    void openedImpl() override;

private slots:
    void onBrowse();
    void onFieldChanged();

private:
    QLineEdit *m_exePathEdit;
    QLineEdit *m_argsEdit;
    QPushButton *m_browseBtn;
};
