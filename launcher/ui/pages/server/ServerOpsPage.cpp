// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
*/
#include "ServerOpsPage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QInputDialog>
#include <QMessageBox>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

ServerOpsPage::ServerOpsPage(ServerInstance *instance, QWidget *parent)
    : QWidget(parent), m_instance(instance)
{
    auto layout = new QVBoxLayout(this);

    auto infoLabel = new QLabel(tr("Manage server operators (ops). Operators have all permissions on the server."), this);
    infoLabel->setWordWrap(true);
    layout->addWidget(infoLabel);

    m_opsList = new QListWidget(this);
    layout->addWidget(m_opsList);

    auto buttonLayout = new QHBoxLayout();
    m_addButton = new QPushButton(tr("Add Operator"), this);
    m_removeButton = new QPushButton(tr("Remove Selected"), this);

    buttonLayout->addWidget(m_addButton);
    buttonLayout->addWidget(m_removeButton);
    buttonLayout->addStretch();

    layout->addLayout(buttonLayout);

    connect(m_addButton, &QPushButton::clicked, this, &ServerOpsPage::addOp);
    connect(m_removeButton, &QPushButton::clicked, this, &ServerOpsPage::removeOp);
}

ServerOpsPage::~ServerOpsPage() {}

QString ServerOpsPage::displayName() const { return tr("Operators"); }
QIcon ServerOpsPage::icon() const
{
    auto icon = QIcon::fromTheme("user-admin");
    if (icon.isNull())
        icon = QIcon::fromTheme("accounts");
    return icon;
}
QString ServerOpsPage::id() const { return "server-ops"; }
QString ServerOpsPage::helpPage() const { return "Server-Operators"; }
bool ServerOpsPage::shouldDisplay() const { return true; }

void ServerOpsPage::openedImpl()
{
    loadOps();
}

void ServerOpsPage::loadOps()
{
    m_opsList->clear();

    QString opsPath = m_instance->instanceRoot() + "/ops.json";
    QFile opsFile(opsPath);

    if (!opsFile.exists() || !opsFile.open(QIODevice::ReadOnly)) {
        return;
    }

    m_fileExistedAtLoad = true;
    QJsonDocument doc = QJsonDocument::fromJson(opsFile.readAll());
    opsFile.close();

    if (doc.isArray()) {
        QJsonArray opsArray = doc.array();
        for (const QJsonValue &value : opsArray) {
            if (value.isObject()) {
                QJsonObject op = value.toObject();
                QString name = op["name"].toString();
                if (!name.isEmpty()) {
                    m_opsList->addItem(name);
                }
            }
        }
    }
}

void ServerOpsPage::addOp()
{
    bool ok;
    QString playerName = QInputDialog::getText(this, tr("Add Operator"),
                                               tr("Enter player name:"), QLineEdit::Normal,
                                               "", &ok);
    if (ok && !playerName.isEmpty()) {
        // Check if already exists
        for (int i = 0; i < m_opsList->count(); ++i) {
            if (m_opsList->item(i)->text() == playerName) {
                QMessageBox::warning(this, tr("Warning"), tr("Player already in operators list."));
                return;
            }
        }
        m_opsList->addItem(playerName);
    }
}

void ServerOpsPage::removeOp()
{
    auto selected = m_opsList->selectedItems();
    if (selected.isEmpty()) {
        QMessageBox::information(this, tr("Info"), tr("Please select an operator to remove."));
        return;
    }

    for (auto item : selected) {
        delete item;
    }
}

bool ServerOpsPage::apply()
{
    if (m_fileExistedAtLoad)
        return true;
    saveOps();
    return true;
}

void ServerOpsPage::saveOps()
{
    QJsonArray opsArray;

    for (int i = 0; i < m_opsList->count(); ++i) {
        QString playerName = m_opsList->item(i)->text();
        QJsonObject op;
        op["name"] = playerName;
        op["uuid"] = "00000000-0000-0000-0000-000000000000";  // Will be updated by server
        op["level"] = 4;
        op["bypassesPlayerLimit"] = false;
        opsArray.append(op);
    }

    QString opsPath = m_instance->instanceRoot() + "/ops.json";
    QFile opsFile(opsPath);

    if (!opsFile.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, tr("Error"), tr("Failed to save ops.json"));
        return;
    }

    QJsonDocument doc(opsArray);
    opsFile.write(doc.toJson(QJsonDocument::Indented));
    opsFile.close();
}
