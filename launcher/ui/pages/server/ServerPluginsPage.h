/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
*/
#pragma once
#include "ui/pages/instance/ExternalResourcesPage.h"

class PluginFolderModel;

class ServerPluginsPage : public ExternalResourcesPage
{
    Q_OBJECT
public:
    explicit ServerPluginsPage(BaseInstance *inst, std::shared_ptr<PluginFolderModel> plugins, QWidget *parent = nullptr);
    virtual ~ServerPluginsPage() = default;

    void setFilter(const QString& filter) { m_fileSelectionFilter = filter; }

    bool shouldDisplay() const override { return true; }
    QString displayName() const override { return tr("Plugins"); }
    QIcon icon() const override { return QIcon::fromTheme("loadermods"); }
    QString id() const override { return "plugins"; }
    QString helpPage() const override { return "Plugins"; }

private:
    QString m_fileSelectionFilter = "%1 (*.jar)";
};
