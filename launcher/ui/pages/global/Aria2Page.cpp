// SPDX-License-Identifier: GPL-3.0-only

#include "ui/pages/global/Aria2Page.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "Application.h"
#include "net/Aria2Manager.h"
#include "settings/SettingsObject.h"

Aria2Page::Aria2Page(QWidget* parent) : QWidget(parent)
{
    auto root = new QVBoxLayout(this);

    auto general = new QGroupBox(tr("Download Backend"), this);
    auto generalLayout = new QFormLayout(general);
    m_enabled = new QCheckBox(tr("Use aria2 for direct file downloads"), general);
    m_fallbackToQt = new QCheckBox(tr("Fall back to the built-in downloader when aria2 is unavailable"), general);
    m_executablePath = new QLineEdit(general);
    auto pathRow = new QWidget(general);
    auto pathLayout = new QHBoxLayout(pathRow);
    pathLayout->setContentsMargins(0, 0, 0, 0);
    auto browse = new QPushButton(tr("Browse..."), pathRow);
    auto detect = new QPushButton(tr("Detect"), pathRow);
    pathLayout->addWidget(m_executablePath, 1);
    pathLayout->addWidget(browse);
    pathLayout->addWidget(detect);
    m_detectedPath = new QLabel(general);
    m_detectedPath->setTextInteractionFlags(Qt::TextSelectableByMouse);

    generalLayout->addRow(m_enabled);
    generalLayout->addRow(m_fallbackToQt);
    generalLayout->addRow(tr("aria2c path:"), pathRow);
    generalLayout->addRow(tr("Detected:"), m_detectedPath);
    root->addWidget(general);

    auto transfers = new QGroupBox(tr("Transfer Options"), this);
    auto transfersLayout = new QFormLayout(transfers);
    m_followLauncherDownloadLimits = new QCheckBox(tr("Follow launcher download limits"), transfers);
    m_maxConcurrent = new QSpinBox(transfers);
    m_maxConcurrent->setRange(1, 128);
    m_maxConnection = new QSpinBox(transfers);
    m_maxConnection->setRange(1, 64);
    m_split = new QSpinBox(transfers);
    m_split->setRange(1, 64);
    m_minSplitSize = new QLineEdit(transfers);
    m_fileAllocation = new QComboBox(transfers);
    m_fileAllocation->addItems({ "none", "prealloc", "trunc", "falloc" });
    m_continue = new QCheckBox(tr("Resume partial aria2 downloads"), transfers);
    m_rpcPort = new QSpinBox(transfers);
    m_rpcPort->setRange(0, 65535);
    m_rpcPort->setSpecialValueText(tr("Random"));
    m_pollInterval = new QSpinBox(transfers);
    m_pollInterval->setRange(250, 10000);
    m_pollInterval->setSuffix(tr(" ms"));

    transfersLayout->addRow(m_followLauncherDownloadLimits);
    transfersLayout->addRow(tr("Max active downloads:"), m_maxConcurrent);
    transfersLayout->addRow(tr("Connections per server:"), m_maxConnection);
    transfersLayout->addRow(tr("Split count:"), m_split);
    transfersLayout->addRow(tr("Minimum split size:"), m_minSplitSize);
    transfersLayout->addRow(tr("File allocation:"), m_fileAllocation);
    transfersLayout->addRow(m_continue);
    transfersLayout->addRow(tr("RPC port:"), m_rpcPort);
    transfersLayout->addRow(tr("Monitor refresh:"), m_pollInterval);
    root->addWidget(transfers);

    auto proxy = new QGroupBox(tr("Proxy"), this);
    auto proxyLayout = new QFormLayout(proxy);
    m_useLauncherProxy = new QCheckBox(tr("Use launcher proxy settings"), proxy);
    m_proxyType = new QComboBox(proxy);
    m_proxyType->addItem(tr("None"), "None");
    m_proxyType->addItem(tr("HTTP"), "HTTP");
    m_proxyType->addItem(tr("SOCKS5"), "SOCKS5");
    m_proxyAddr = new QLineEdit(proxy);
    m_proxyPort = new QSpinBox(proxy);
    m_proxyPort->setRange(1, 65535);
    m_proxyUser = new QLineEdit(proxy);
    m_proxyPass = new QLineEdit(proxy);
    m_proxyPass->setEchoMode(QLineEdit::Password);
    proxyLayout->addRow(m_useLauncherProxy);
    proxyLayout->addRow(tr("aria2 proxy type:"), m_proxyType);
    proxyLayout->addRow(tr("Host:"), m_proxyAddr);
    proxyLayout->addRow(tr("Port:"), m_proxyPort);
    proxyLayout->addRow(tr("User:"), m_proxyUser);
    proxyLayout->addRow(tr("Password:"), m_proxyPass);
    root->addWidget(proxy);

    auto advanced = new QGroupBox(tr("Advanced"), this);
    auto advancedLayout = new QFormLayout(advanced);
    m_extraArgs = new QPlainTextEdit(advanced);
    m_extraArgs->setPlaceholderText(tr("--option=value, one argument per line"));
    m_extraArgs->setMinimumHeight(80);
    advancedLayout->addRow(tr("Extra aria2c arguments:"), m_extraArgs);
    root->addWidget(advanced);
    root->addStretch();

    connect(browse, &QPushButton::clicked, this, &Aria2Page::browseExecutable);
    connect(detect, &QPushButton::clicked, this, &Aria2Page::detectExecutable);
    connect(m_useLauncherProxy, &QCheckBox::toggled, this, &Aria2Page::updateProxyWidgets);
    connect(m_proxyType, &QComboBox::currentIndexChanged, this, &Aria2Page::updateProxyWidgets);
    connect(m_followLauncherDownloadLimits, &QCheckBox::toggled, this, &Aria2Page::updateTransferWidgets);

    loadSettings();
    detectExecutable();
    updateProxyWidgets();
    updateTransferWidgets();
}

bool Aria2Page::apply()
{
    applySettings();
    return true;
}

void Aria2Page::browseExecutable()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Select aria2c"), m_executablePath->text());
    if (!path.isEmpty()) {
        m_executablePath->setText(path);
    }
}

void Aria2Page::detectExecutable()
{
    m_detectedPath->setText(Net::Aria2Manager::instance()->findExecutable());
}

void Aria2Page::updateProxyWidgets()
{
    const bool ownProxy = !m_useLauncherProxy->isChecked();
    const bool proxyEnabled = ownProxy && m_proxyType->currentData().toString() != "None";
    m_proxyType->setEnabled(ownProxy);
    m_proxyAddr->setEnabled(proxyEnabled);
    m_proxyPort->setEnabled(proxyEnabled);
    m_proxyUser->setEnabled(proxyEnabled);
    m_proxyPass->setEnabled(proxyEnabled);
}

void Aria2Page::updateTransferWidgets()
{
    const bool customLimits = !m_followLauncherDownloadLimits->isChecked();
    m_maxConcurrent->setEnabled(customLimits);
}

void Aria2Page::loadSettings()
{
    auto s = APPLICATION->settings();
    m_enabled->setChecked(s->get("Aria2Enabled").toBool());
    m_executablePath->setText(s->get("Aria2ExecutablePath").toString());
    m_fallbackToQt->setChecked(s->get("Aria2FallbackToQt").toBool());
    m_followLauncherDownloadLimits->setChecked(s->get("Aria2FollowLauncherDownloadLimits").toBool());
    m_maxConcurrent->setValue(s->get("Aria2MaxConcurrentDownloads").toInt());
    m_maxConnection->setValue(s->get("Aria2MaxConnectionPerServer").toInt());
    m_split->setValue(s->get("Aria2Split").toInt());
    m_minSplitSize->setText(s->get("Aria2MinSplitSize").toString());
    m_fileAllocation->setCurrentText(s->get("Aria2FileAllocation").toString());
    m_continue->setChecked(s->get("Aria2Continue").toBool());
    m_rpcPort->setValue(s->get("Aria2RpcPort").toInt());
    m_pollInterval->setValue(s->get("Aria2PollInterval").toInt());
    m_extraArgs->setPlainText(s->get("Aria2ExtraArgs").toString());
    m_useLauncherProxy->setChecked(s->get("Aria2UseLauncherProxy").toBool());
    const int proxyIndex = m_proxyType->findData(s->get("Aria2ProxyType").toString());
    m_proxyType->setCurrentIndex(proxyIndex < 0 ? 0 : proxyIndex);
    m_proxyAddr->setText(s->get("Aria2ProxyAddr").toString());
    m_proxyPort->setValue(s->get("Aria2ProxyPort").toInt());
    m_proxyUser->setText(s->get("Aria2ProxyUser").toString());
    m_proxyPass->setText(s->get("Aria2ProxyPass").toString());
}

void Aria2Page::applySettings()
{
    auto s = APPLICATION->settings();
    s->set("Aria2Enabled", m_enabled->isChecked());
    s->set("Aria2ExecutablePath", m_executablePath->text());
    s->set("Aria2FallbackToQt", m_fallbackToQt->isChecked());
    s->set("Aria2FollowLauncherDownloadLimits", m_followLauncherDownloadLimits->isChecked());
    s->set("Aria2MaxConcurrentDownloads", m_maxConcurrent->value());
    s->set("Aria2MaxConnectionPerServer", m_maxConnection->value());
    s->set("Aria2Split", m_split->value());
    s->set("Aria2MinSplitSize", m_minSplitSize->text());
    s->set("Aria2FileAllocation", m_fileAllocation->currentText());
    s->set("Aria2Continue", m_continue->isChecked());
    s->set("Aria2RpcPort", m_rpcPort->value());
    s->set("Aria2PollInterval", m_pollInterval->value());
    s->set("Aria2ExtraArgs", m_extraArgs->toPlainText());
    s->set("Aria2UseLauncherProxy", m_useLauncherProxy->isChecked());
    s->set("Aria2ProxyType", m_proxyType->currentData().toString());
    s->set("Aria2ProxyAddr", m_proxyAddr->text());
    s->set("Aria2ProxyPort", m_proxyPort->value());
    s->set("Aria2ProxyUser", m_proxyUser->text());
    s->set("Aria2ProxyPass", m_proxyPass->text());
}
