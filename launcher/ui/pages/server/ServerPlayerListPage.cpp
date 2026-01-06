// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
*/
#include "ServerPlayerListPage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QInputDialog>
#include <QMessageBox>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

ServerPlayerListPage::ServerPlayerListPage(ServerInstance *instance, ListType type, QWidget *parent)
    : QWidget(parent), m_instance(instance), m_listType(type)
{
    auto layout = new QVBoxLayout(this);

    QString description;
    switch (type) {
        case Whitelist:
            description = tr("Manage server whitelist. Only whitelisted players can join the server when whitelist is enabled in server.properties.");
            break;
        case BannedPlayers:
            description = tr("Manage banned players. Banned players cannot join the server.");
            break;
        case BannedIPs:
            description = tr("Manage banned IP addresses. Players from banned IPs cannot join the server.");
            break;
    }

    auto infoLabel = new QLabel(description, this);
    infoLabel->setWordWrap(true);
    layout->addWidget(infoLabel);

    m_list = new QListWidget(this);
    layout->addWidget(m_list);

    auto buttonLayout = new QHBoxLayout();

    QString addButtonText = (type == BannedIPs) ? tr("Add IP Address") : tr("Add Player");
    m_addButton = new QPushButton(addButtonText, this);
    m_removeButton = new QPushButton(tr("Remove Selected"), this);

    buttonLayout->addWidget(m_addButton);
    buttonLayout->addWidget(m_removeButton);
    buttonLayout->addStretch();

    layout->addLayout(buttonLayout);

    connect(m_addButton, &QPushButton::clicked, this, &ServerPlayerListPage::addEntry);
    connect(m_removeButton, &QPushButton::clicked, this, &ServerPlayerListPage::removeEntry);
}

ServerPlayerListPage::~ServerPlayerListPage() {}

QString ServerPlayerListPage::displayName() const
{
    switch (m_listType) {
        case Whitelist: return tr("Whitelist");
        case BannedPlayers: return tr("Banned Players");
        case BannedIPs: return tr("Banned IPs");
    }
    return "";
}

QIcon ServerPlayerListPage::icon() const
{
    auto icon = QIcon::fromTheme("accounts");
    return icon;
}

QString ServerPlayerListPage::id() const
{
    switch (m_listType) {
        case Whitelist: return "server-whitelist";
        case BannedPlayers: return "server-banned-players";
        case BannedIPs: return "server-banned-ips";
    }
    return "";
}

QString ServerPlayerListPage::helpPage() const
{
    switch (m_listType) {
        case Whitelist: return "Server-Whitelist";
        case BannedPlayers: return "Server-Banned-Players";
        case BannedIPs: return "Server-Banned-IPs";
    }
    return "";
}

bool ServerPlayerListPage::shouldDisplay() const { return true; }

void ServerPlayerListPage::openedImpl()
{
    loadList();
}

QString ServerPlayerListPage::getFileName() const
{
    switch (m_listType) {
        case Whitelist: return "whitelist.json";
        case BannedPlayers: return "banned-players.json";
        case BannedIPs: return "banned-ips.json";
    }
    return "";
}

QString ServerPlayerListPage::getFilePath() const
{
    return m_instance->instanceRoot() + "/" + getFileName();
}

void ServerPlayerListPage::loadList()
{
    m_list->clear();

    QFile file(getFilePath());

    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return;
    }

    m_fileExistedAtLoad = true;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (doc.isArray()) {
        QJsonArray array = doc.array();
        for (const QJsonValue &value : array) {
            if (value.isObject()) {
                QJsonObject obj = value.toObject();
                QString entry;

                if (m_listType == BannedIPs) {
                    entry = obj["ip"].toString();
                } else {
                    entry = obj["name"].toString();
                }

                if (!entry.isEmpty()) {
                    m_list->addItem(entry);
                }
            }
        }
    }
}

void ServerPlayerListPage::addEntry()
{
    bool ok;
    QString prompt = (m_listType == BannedIPs)
        ? tr("Enter IP address:")
        : tr("Enter player name:");

    QString entry = QInputDialog::getText(this,
                                          (m_listType == BannedIPs) ? tr("Add IP Address") : tr("Add Player"),
                                          prompt, QLineEdit::Normal, "", &ok);

    if (ok && !entry.isEmpty()) {
        // Check if already exists
        for (int i = 0; i < m_list->count(); ++i) {
            if (m_list->item(i)->text() == entry) {
                QMessageBox::warning(this, tr("Warning"), tr("Entry already exists in the list."));
                return;
            }
        }
        m_list->addItem(entry);
    }
}

void ServerPlayerListPage::removeEntry()
{
    auto selected = m_list->selectedItems();
    if (selected.isEmpty()) {
        QMessageBox::information(this, tr("Info"), tr("Please select an entry to remove."));
        return;
    }

    for (auto item : selected) {
        delete item;
    }
}

bool ServerPlayerListPage::apply()
{
    if (m_fileExistedAtLoad)
        return true;
    saveList();
    return true;
}

void ServerPlayerListPage::saveList()
{
    QJsonArray array;

    for (int i = 0; i < m_list->count(); ++i) {
        QString entry = m_list->item(i)->text();
        QJsonObject obj;

        if (m_listType == BannedIPs) {
            obj["ip"] = entry;
            obj["created"] = QDateTime::currentDateTime().toString(Qt::ISODate);
            obj["source"] = "Luna Launcher";
            obj["expires"] = "forever";
            obj["reason"] = "Banned by an operator";
        } else {
            obj["name"] = entry;
            obj["uuid"] = "00000000-0000-0000-0000-000000000000";  // Will be updated by server

            if (m_listType == BannedPlayers) {
                obj["created"] = QDateTime::currentDateTime().toString(Qt::ISODate);
                obj["source"] = "Luna Launcher";
                obj["expires"] = "forever";
                obj["reason"] = "Banned by an operator";
            }
        }

        array.append(obj);
    }

    QFile file(getFilePath());

    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, tr("Error"), tr("Failed to save %1").arg(getFileName()));
        return;
    }

    QJsonDocument doc(array);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
}
