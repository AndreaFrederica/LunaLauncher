// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
*/
#include "ServerYamlEditorPage.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QFont>

ServerYamlEditorPage::ServerYamlEditorPage(ServerInstance *instance, FileType type, QWidget *parent)
    : QWidget(parent), m_instance(instance), m_fileType(type)
{
    auto layout = new QVBoxLayout(this);

    QString fileName = (type == Bukkit) ? "bukkit.yml" : "spigot.yml";
    QString description = (type == Bukkit)
        ? tr("Edit Bukkit configuration. This file controls core server features like spawn limits, chunk garbage collection, and tick rates.")
        : tr("Edit Spigot configuration. This file controls Spigot-specific optimizations, entity activation ranges, and world settings.");

    auto infoLabel = new QLabel(description, this);
    infoLabel->setWordWrap(true);
    layout->addWidget(infoLabel);

    m_editor = new QPlainTextEdit(this);
    QFont font("Monospace");
    font.setStyleHint(QFont::TypeWriter);
    m_editor->setFont(font);
    m_editor->setTabStopDistance(40);
    layout->addWidget(m_editor);
}

ServerYamlEditorPage::~ServerYamlEditorPage() {}

QString ServerYamlEditorPage::displayName() const
{
    return (m_fileType == Bukkit) ? tr("Bukkit Config") : tr("Spigot Config");
}

QIcon ServerYamlEditorPage::icon() const { return QIcon::fromTheme("settings"); }

QString ServerYamlEditorPage::id() const
{
    return (m_fileType == Bukkit) ? "server-bukkit" : "server-spigot";
}

QString ServerYamlEditorPage::helpPage() const
{
    return (m_fileType == Bukkit) ? "Bukkit-Configuration" : "Spigot-Configuration";
}

bool ServerYamlEditorPage::shouldDisplay() const { return true; }

void ServerYamlEditorPage::openedImpl()
{
    loadFile();
}

QString ServerYamlEditorPage::getFilePath() const
{
    QString fileName = (m_fileType == Bukkit) ? "bukkit.yml" : "spigot.yml";
    return m_instance->instanceRoot() + "/" + fileName;
}

void ServerYamlEditorPage::loadFile()
{
    QFile file(getFilePath());

    if (file.exists() && file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        m_originalText = in.readAll();
        m_editor->setPlainText(m_originalText);
        file.close();
    } else {
        m_originalText.clear();
        m_editor->setPlainText(tr("# File not found. It will be created when you save."));
    }
}

bool ServerYamlEditorPage::apply()
{
    // Avoid initializing config if editor has placeholder or empty
    QString text = m_editor->toPlainText().trimmed();
    if (text.isEmpty() || text.startsWith("# File not found"))
        return true;
    // Skip writing if unchanged
    if (!m_originalText.isEmpty() && text == m_originalText.trimmed())
        return true;
    saveFile();
    return true;
}

void ServerYamlEditorPage::saveFile()
{
    QFile file(getFilePath());

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("Error"),
            tr("Failed to save %1").arg((m_fileType == Bukkit) ? "bukkit.yml" : "spigot.yml"));
        return;
    }

    QTextStream out(&file);
    out << m_editor->toPlainText();
    file.close();
}
