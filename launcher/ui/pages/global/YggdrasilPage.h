// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QWidget>

#include "ui/pages/BasePage.h"

class QCheckBox;

class YggdrasilPage : public QWidget, public BasePage {
    Q_OBJECT

public:
    explicit YggdrasilPage(QWidget* parent = nullptr);

    QString displayName() const override { return tr("Yggdrasil Service"); }
    QIcon icon() const override { return QIcon::fromTheme("accounts"); }
    QString id() const override { return "yggdrasil-service"; }
    QString helpPage() const override { return "accounts"; }
    bool apply() override;
    void retranslate() override;

private:
    QCheckBox* m_metadataCache = nullptr;
    QCheckBox* m_accountCache = nullptr;
};
