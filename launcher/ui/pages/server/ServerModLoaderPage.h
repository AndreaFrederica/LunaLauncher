/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
*/
#pragma once

#include <QWidget>
#include "ui/pages/BasePage.h"
#include "modplatform/ModIndex.h"

class ServerInstance;
class QLineEdit;
class QCheckBox;
class QGroupBox;
class QLabel;

class ServerModLoaderPage : public QWidget, public BasePage
{
    Q_OBJECT
public:
    explicit ServerModLoaderPage(ServerInstance *instance, QWidget *parent = nullptr);
    virtual ~ServerModLoaderPage();

    QString displayName() const override;
    QIcon icon() const override;
    QString id() const override;
    QString helpPage() const override;
    bool shouldDisplay() const override;

    bool apply();
    void load();

private:
    void updateSummary();

private:
    ServerInstance *m_instance;
    QLineEdit *m_minecraftVersionEdit;

    // Mod loaders
    QCheckBox *m_neoforgeCheck;
    QCheckBox *m_forgeCheck;
    QCheckBox *m_fabricCheck;
    QCheckBox *m_quiltCheck;

    // Plugin loaders (for future plugin download system)
    QCheckBox *m_paperCheck;
    QCheckBox *m_spigotCheck;
    QCheckBox *m_bukkitCheck;
    QCheckBox *m_purpurCheck;
    QCheckBox *m_spongeCheck;
    QCheckBox *m_velocityCheck;
    QCheckBox *m_waterfallCheck;
    QCheckBox *m_bungeecordCheck;

    QGroupBox *m_summaryGroup;
    QLabel *m_summaryLabel;
};
