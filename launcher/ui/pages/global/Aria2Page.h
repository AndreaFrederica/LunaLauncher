// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QWidget>

#include "ui/pages/BasePage.h"

class QCheckBox;
class QComboBox;
class QLineEdit;
class QLabel;
class QPlainTextEdit;
class QSpinBox;

class Aria2Page : public QWidget, public BasePage {
    Q_OBJECT

   public:
    explicit Aria2Page(QWidget* parent = nullptr);

    QString displayName() const override { return tr("aria2"); }
    QIcon icon() const override { return QIcon::fromTheme("proxy"); }
    QString id() const override { return "aria2-settings"; }
    QString helpPage() const override { return "Aria2-settings"; }
    bool apply() override;

   private slots:
    void browseExecutable();
    void detectExecutable();
    void updateProxyWidgets();
    void updateTransferWidgets();

   private:
    void loadSettings();
    void applySettings();

   private:
    QCheckBox* m_enabled = nullptr;
    QLineEdit* m_executablePath = nullptr;
    QCheckBox* m_fallbackToQt = nullptr;
    QCheckBox* m_followLauncherDownloadLimits = nullptr;
    QSpinBox* m_maxConcurrent = nullptr;
    QSpinBox* m_maxConnection = nullptr;
    QSpinBox* m_split = nullptr;
    QLineEdit* m_minSplitSize = nullptr;
    QComboBox* m_fileAllocation = nullptr;
    QCheckBox* m_continue = nullptr;
    QSpinBox* m_rpcPort = nullptr;
    QSpinBox* m_pollInterval = nullptr;
    QPlainTextEdit* m_extraArgs = nullptr;
    QCheckBox* m_useLauncherProxy = nullptr;
    QComboBox* m_proxyType = nullptr;
    QLineEdit* m_proxyAddr = nullptr;
    QSpinBox* m_proxyPort = nullptr;
    QLineEdit* m_proxyUser = nullptr;
    QLineEdit* m_proxyPass = nullptr;
    QLabel* m_detectedPath = nullptr;
};
