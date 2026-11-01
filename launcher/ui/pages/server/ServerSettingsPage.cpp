/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
*/
#include "ServerSettingsPage.h"
#include "server/ServerInstance.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QLabel>
#include <QGroupBox>

ServerSettingsPage::ServerSettingsPage(ServerInstance *instance, QWidget *parent)
    : QWidget(parent), m_instance(instance)
{
    auto layout = new QVBoxLayout(this);
    auto form = new QFormLayout();

    m_exePathEdit = new QLineEdit(this);
    m_argsEdit = new QLineEdit(this);
    m_workDirEdit = new QLineEdit(this);
    m_stopCommandEdit = new QLineEdit(this);
    m_authlibEnabled = new QCheckBox(tr("Enable server-side authlib-injector"), this);
    m_authlibApiUrlEdit = new QLineEdit(this);
    m_browseBtn = new QPushButton(tr("Browse..."), this);

    auto exeLayout = new QHBoxLayout();
    exeLayout->addWidget(m_exePathEdit);
    exeLayout->addWidget(m_browseBtn);

    form->addRow(tr("Executable:"), exeLayout);
    form->addRow(tr("Arguments:"), m_argsEdit);
    form->addRow(tr("Working Directory:"), m_workDirEdit);
    form->addRow(tr("Graceful Stop Command:"), m_stopCommandEdit);
    form->addRow(m_authlibEnabled);
    form->addRow(tr("Authlib Injector API URL:"), m_authlibApiUrlEdit);

    layout->addLayout(form);

    auto previewGroup = new QGroupBox(tr("Launch Command Preview"), this);
    auto previewLayout = new QVBoxLayout(previewGroup);
    m_previewLabel = new QLabel(this);
    m_previewLabel->setWordWrap(true);
    m_previewLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    previewLayout->addWidget(m_previewLabel);
    layout->addWidget(previewGroup);

    layout->addStretch();

    connect(m_browseBtn, &QPushButton::clicked, this, &ServerSettingsPage::onBrowse);
    connect(m_exePathEdit, &QLineEdit::textChanged, this, &ServerSettingsPage::updatePreview);
    connect(m_argsEdit, &QLineEdit::textChanged, this, &ServerSettingsPage::updatePreview);
    connect(m_workDirEdit, &QLineEdit::textChanged, this, &ServerSettingsPage::updatePreview);
    connect(m_stopCommandEdit, &QLineEdit::textChanged, this, &ServerSettingsPage::updatePreview);
    connect(m_authlibEnabled, &QCheckBox::toggled, this, &ServerSettingsPage::updatePreview);
    connect(m_authlibApiUrlEdit, &QLineEdit::textChanged, this, &ServerSettingsPage::updatePreview);

    // Register changes
    // connect(m_exePathEdit, &QLineEdit::textChanged, this, &ServerSettingsPage::registerSettingsChange);
    // connect(m_argsEdit, &QLineEdit::textChanged, this, &ServerSettingsPage::registerSettingsChange);
    // connect(m_workDirEdit, &QLineEdit::textChanged, this, &ServerSettingsPage::registerSettingsChange);

    load();
}

ServerSettingsPage::~ServerSettingsPage() {}

QString ServerSettingsPage::displayName() const { return tr("Settings"); }
QIcon ServerSettingsPage::icon() const { return QIcon::fromTheme("settings"); }
QString ServerSettingsPage::id() const { return "server-settings"; }
QString ServerSettingsPage::helpPage() const { return "Server Settings"; }
bool ServerSettingsPage::shouldDisplay() const { return true; }

bool ServerSettingsPage::apply()
{
    m_instance->setExecutablePath(m_exePathEdit->text());
    m_instance->setArguments(m_argsEdit->text().split(' ', Qt::SkipEmptyParts));
    m_instance->setWorkingDir(m_workDirEdit->text());
    m_instance->setStopCommand(m_stopCommandEdit->text());
    m_instance->setAuthlibInjectorEnabled(m_authlibEnabled->isChecked());
    m_instance->setAuthlibInjectorApiUrl(m_authlibApiUrlEdit->text().trimmed());
    return true;
}

void ServerSettingsPage::load()
{
    m_exePathEdit->setText(m_instance->executablePath());
    m_argsEdit->setText(m_instance->arguments().join(' '));
    m_workDirEdit->setText(m_instance->workingDir());
    m_stopCommandEdit->setText(m_instance->stopCommand());
    m_authlibEnabled->setChecked(m_instance->authlibInjectorEnabled());
    m_authlibApiUrlEdit->setText(m_instance->authlibInjectorApiUrl());
    updatePreview();
}

void ServerSettingsPage::updatePreview()
{
    QString exe = m_exePathEdit->text();
    QString args = m_argsEdit->text();

    // Java & Memory Replacements
    auto settings = m_instance->settings();
    QString javaPath = "java";
    if (settings->get("OverrideJavaLocation").toBool()) {
        javaPath = settings->get("JavaPath").toString();
    }

    // Force java instead of javaw for servers
    if (javaPath.endsWith("javaw.exe", Qt::CaseInsensitive)) {
        javaPath.chop(9); // Remove "javaw.exe"
        javaPath.append("java.exe");
    } else if (javaPath.endsWith("javaw", Qt::CaseInsensitive)) {
        javaPath.chop(5); // Remove "javaw"
        javaPath.append("java");
    }

    // Quote java path if it contains spaces
    if (javaPath.contains(' ') && !javaPath.startsWith('"')) {
        javaPath = "\"" + javaPath + "\"";
    }

    QString minMem = settings->get("MinMemAlloc").toString();
    QString maxMem = settings->get("MaxMemAlloc").toString();
    QString permMem = settings->get("PermGen").toString();

    auto doReplace = [&](QString &str) {
        str.replace("$java", javaPath);
        str.replace("$min_memory", minMem);
        str.replace("$max_memory", maxMem);
        str.replace("$perm_memory", permMem);
    };

    doReplace(exe);
    doReplace(args);

    QString cmd = QString("%1 %2").arg(exe, args);
    if (!m_stopCommandEdit->text().trimmed().isEmpty())
        cmd += tr("\nGraceful stop: %1").arg(m_stopCommandEdit->text().trimmed());
    if (m_authlibEnabled->isChecked())
        cmd += tr("\nAuthlib Injector: %1").arg(m_authlibApiUrlEdit->text().trimmed());
    m_previewLabel->setText(cmd);
}

void ServerSettingsPage::onBrowse()
{
    QString file = QFileDialog::getOpenFileName(this, tr("Select Executable"));
    if (!file.isEmpty()) {
        m_exePathEdit->setText(file);
    }
}
