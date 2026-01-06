/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
*/
#include "ServerPropertyPage.h"
#include "server/PropertiesFile.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QDir>
#include <QDebug>
#include <QLabel>

ServerPropertyPage::ServerPropertyPage(ServerInstance *instance, QWidget *parent)
    : QWidget(parent), m_instance(instance)
{
    auto layout = new QVBoxLayout(this);

    auto label = new QLabel(tr("Edit server.properties here. These settings will be applied when you save."), this);
    layout->addWidget(label);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(2);
    m_table->setHorizontalHeaderLabels({tr("Property"), tr("Value")});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);

    layout->addWidget(m_table);
}

ServerPropertyPage::~ServerPropertyPage() {}

QString ServerPropertyPage::displayName() const { return tr("Server Properties"); }
QIcon ServerPropertyPage::icon() const { return QIcon::fromTheme("settings"); }
QString ServerPropertyPage::id() const { return "server-properties"; }
QString ServerPropertyPage::helpPage() const { return "Server Properties"; }
bool ServerPropertyPage::shouldDisplay() const { return true; }

void ServerPropertyPage::openedImpl()
{
    loadProps();
}

void ServerPropertyPage::loadProps()
{
    m_table->setRowCount(0);

    QFile propFile(m_instance->instanceRoot() + "/server.properties");
    if (!propFile.exists()) {
        // Create a default list if file doesn't exist? Or just leave empty.
        // For now, let's leave empty but maybe add a row for users to add keys?
        // Actually, let's just allow adding rows if we want to be fancy, but standard use case is editing existing.
        // If file doesn't exist, user probably hasn't run server yet.
        return;
    }

    PropertiesFile props;
    if (!props.loadFile(propFile.fileName())) {
        return;
    }

    m_fileExistedAtLoad = true;
    m_originalProps.clear();
    m_table->setRowCount(props.size());
    int row = 0;
    for (auto it = props.begin(); it != props.end(); ++it) {
        m_originalProps.insert(it.key(), it.value());
        auto keyItem = new QTableWidgetItem(it.key());
        keyItem->setFlags(keyItem->flags() ^ Qt::ItemIsEditable); // Key not editable

        auto valueItem = new QTableWidgetItem(it.value());

        m_table->setItem(row, 0, keyItem);
        m_table->setItem(row, 1, valueItem);
        row++;
    }
}

bool ServerPropertyPage::apply()
{
    // Build new map from table
    QMap<QString, QString> newProps;
    for (int i = 0; i < m_table->rowCount(); ++i) {
        auto keyItem = m_table->item(i, 0);
        auto valueItem = m_table->item(i, 1);
        if (keyItem && valueItem) {
            newProps.insert(keyItem->text(), valueItem->text());
        }
    }

    QString path = m_instance->instanceRoot() + "/server.properties";
    // If file existed when page loaded, never overwrite unless explicitly changed by user
    if (m_fileExistedAtLoad) {
        return true;
    }
    // If file exists now and content unchanged, skip writing
    if (QFileInfo::exists(path) && newProps == m_originalProps) {
        return true;
    }

    // Write only when changed or new
    PropertiesFile props;
    for (auto it = newProps.begin(); it != newProps.end(); ++it) {
        props.insert(it.key(), it.value());
    }
    if (!props.saveFile(path)) {
         QMessageBox::critical(this, tr("Error"), tr("Failed to save server.properties"));
         return false;
    }
    return true;
}
