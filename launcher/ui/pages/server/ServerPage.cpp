/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
*/
#include "ServerPage.h"
#include "server/ServerInstanceCreationTask.h"
#include "ui/dialogs/NewInstanceDialog.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QLabel>
#include <QFileInfo>

ServerPage::ServerPage(QWidget *parent)
    : QWidget(parent)
{
    auto layout = new QVBoxLayout(this);
    auto form = new QFormLayout();

    m_exePathEdit = new QLineEdit(this);
    m_argsEdit = new QLineEdit(this);
    m_browseBtn = new QPushButton(tr("Browse..."), this);

    auto exeLayout = new QHBoxLayout();
    exeLayout->addWidget(m_exePathEdit);
    exeLayout->addWidget(m_browseBtn);

    form->addRow(tr("Executable:"), exeLayout);
    form->addRow(tr("Arguments:"), m_argsEdit);

    layout->addLayout(form);
    layout->addStretch();

    connect(m_browseBtn, &QPushButton::clicked, this, &ServerPage::onBrowse);
    connect(m_exePathEdit, &QLineEdit::textChanged, this, &ServerPage::onFieldChanged);
    connect(m_argsEdit, &QLineEdit::textChanged, this, &ServerPage::onFieldChanged);
}

ServerPage::~ServerPage() {}

QString ServerPage::displayName() const { return tr("Server Instance"); }
QIcon ServerPage::icon() const { return QIcon::fromTheme("server-database"); }
QString ServerPage::id() const { return "server"; }
QString ServerPage::helpPage() const { return "Server Instance"; }
bool ServerPage::shouldDisplay() const { return true; }

void ServerPage::openedImpl()
{
    onFieldChanged();
}

void ServerPage::onBrowse()
{
    QString file = QFileDialog::getOpenFileName(this, tr("Select Executable"));
    if (!file.isEmpty()) {
        m_exePathEdit->setText(file);
    }
}

void ServerPage::onFieldChanged()
{
    if (auto dialog = qobject_cast<NewInstanceDialog*>(window())) {
        QString exe = m_exePathEdit->text();
        QStringList args = m_argsEdit->text().split(' ', Qt::SkipEmptyParts);

        // Allow empty executable for empty server instance
        auto task = new ServerInstanceCreationTask(exe, args);

        QString suggestedName = exe.isEmpty() ? tr("Server Instance") : QFileInfo(exe).fileName();
        dialog->setSuggestedPack(suggestedName, task);
    }
}
