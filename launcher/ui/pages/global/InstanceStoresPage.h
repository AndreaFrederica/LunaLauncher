// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QWidget>
#include <QJsonObject>

#include "ui/pages/BasePage.h"

class QTableWidget;
class QPushButton;
class QGroupBox;

class InstanceStoresPage final : public QWidget, public BasePage {
    Q_OBJECT
   public:
    explicit InstanceStoresPage(QWidget* parent = nullptr);
    ~InstanceStoresPage() override = default;

    QString displayName() const override { return tr("Instance Stores"); }
    QIcon icon() const override { return QIcon::fromTheme("folder-open"); }
    QString id() const override { return "instance-stores"; }
    QString helpPage() const override { return "Launcher-settings"; }
    void retranslate() override;

   private:
    void refreshInstanceStores();
    void addInstanceStore();
    void updateInstanceStore(const QString& id, const QJsonObject& changes);

    QGroupBox* m_instanceStoresBox = nullptr;
    QTableWidget* m_instanceStoresTable = nullptr;
    QPushButton* m_addInstanceStoreButton = nullptr;
};
