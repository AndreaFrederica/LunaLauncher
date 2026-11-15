// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QWidget>

#include "ui/pages/BasePage.h"

class BaseInstance;

/** Read-only audit view for all launcher signatures found in an external instance. */
class ExternalProvenancePage : public QWidget, public BasePage {
    Q_OBJECT
   public:
    explicit ExternalProvenancePage(BaseInstance* instance, QWidget* parent = nullptr);
    QString id() const override { return QStringLiteral("external-provenance"); }
    QString displayName() const override { return tr("来源与修改痕迹"); }
    QIcon icon() const override;
    bool shouldDisplay() const override;

   private:
    BaseInstance* m_instance = nullptr;
};

