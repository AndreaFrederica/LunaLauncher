/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
*/
#include "ServerJavaPage.h"
#include "server/ServerInstance.h"
#include "ui/widgets/JavaSettingsWidget.h"
#include <QVBoxLayout>

ServerJavaPage::ServerJavaPage(ServerInstance *instance, QWidget *parent)
    : QWidget(parent), m_instance(instance)
{
    auto layout = new QVBoxLayout(this);
    m_javaWidget = new JavaSettingsWidget(instance, this);
    layout->addWidget(m_javaWidget);
    layout->setContentsMargins(0, 0, 0, 0);
}

ServerJavaPage::~ServerJavaPage() {}

QString ServerJavaPage::displayName() const { return tr("Java"); }
QIcon ServerJavaPage::icon() const { return QIcon::fromTheme("java"); }
QString ServerJavaPage::id() const { return "java"; }
QString ServerJavaPage::helpPage() const { return "Java Settings"; }
bool ServerJavaPage::shouldDisplay() const { return true; }

bool ServerJavaPage::apply()
{
    m_javaWidget->saveSettings();
    return true;
}

void ServerJavaPage::load()
{
    m_javaWidget->loadSettings();
}
