// SPDX-License-Identifier: GPL-3.0-only
#include "InstanceStoresPage.h"

#include <QCheckBox>
#include <QDialog>
#include <QDir>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include <FileSystem.h>
#include "Application.h"
#include "DesktopServices.h"
#include "InstanceStore.h"

InstanceStoresPage::InstanceStoresPage(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    m_instanceStoresBox = new QGroupBox(tr("Instance Stores"), this);
    auto* storesLayout = new QVBoxLayout(m_instanceStoresBox);
    m_instanceStoresTable = new QTableWidget(m_instanceStoresBox);
    m_instanceStoresTable->setColumnCount(7);
    m_instanceStoresTable->setHorizontalHeaderLabels({ tr("Name"), tr("Path"), tr("Format"), tr("Enabled"), tr("Writable"), tr("Default"), tr("Actions") });
    // Keep every column user-resizable.  The old fixed/stretch combination
    // made long Windows paths collapse to `C:...` and prevented users from
    // giving the path or format columns more room.
    auto* header = m_instanceStoresTable->horizontalHeader();
    header->setStretchLastSection(false);
    for (int column = 0; column < m_instanceStoresTable->columnCount(); ++column)
        header->setSectionResizeMode(column, QHeaderView::Interactive);
    m_instanceStoresTable->setColumnWidth(0, 180);
    m_instanceStoresTable->setColumnWidth(1, 520);
    m_instanceStoresTable->setColumnWidth(2, 150);
    m_instanceStoresTable->setColumnWidth(3, 90);
    m_instanceStoresTable->setColumnWidth(4, 90);
    m_instanceStoresTable->setColumnWidth(5, 90);
    m_instanceStoresTable->setColumnWidth(6, 300);
    m_instanceStoresTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_instanceStoresTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_instanceStoresTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_addInstanceStoreButton = new QPushButton(tr("Add instance directory"), m_instanceStoresBox);
    storesLayout->addWidget(m_instanceStoresTable);
    storesLayout->addWidget(m_addInstanceStoreButton);
    layout->addWidget(m_instanceStoresBox);
    connect(m_addInstanceStoreButton, &QPushButton::clicked, this, &InstanceStoresPage::addInstanceStore);
    refreshInstanceStores();
}

void InstanceStoresPage::refreshInstanceStores()
{
    if (!m_instanceStoresTable)
        return;
    const auto stores = APPLICATION->instanceStoresStatus();
    m_instanceStoresTable->setRowCount(stores.size());
    for (int row = 0; row < stores.size(); ++row) {
        const auto store = stores.at(row).toObject();
        const auto id = store.value("id").toString();
        auto pathText = QDir::toNativeSeparators(store.value("path").toString());
        pathText += store.value("exists").toBool() ? tr(" (exists)") : tr(" (missing)");
        const auto count = store.value("instanceCount").toInt(-1);
        if (count >= 0)
            pathText += tr(" · %1 instance(s)").arg(count);
        else if (store.value("scanState").toString() == QStringLiteral("disabled"))
            pathText += tr(" · disabled");
        m_instanceStoresTable->setItem(row, 0, new QTableWidgetItem(store.value("name").toString()));
        auto* pathItem = new QTableWidgetItem(pathText);
        pathItem->setToolTip(QDir::toNativeSeparators(store.value("path").toString()));
        m_instanceStoresTable->setItem(row, 1, pathItem);
        m_instanceStoresTable->setItem(row, 2, new QTableWidgetItem(store.value("format").toString() == "vanilla-compatible" ? tr("Vanilla/PCL/HMCL") : tr("Prism")));
        auto* enabled = new QCheckBox(m_instanceStoresTable);
        enabled->setChecked(store.value("enabled").toBool());
        connect(enabled, &QCheckBox::toggled, this, [this, id](bool value) { updateInstanceStore(id, { { "enabled", value } }); });
        m_instanceStoresTable->setCellWidget(row, 3, enabled);
        auto* writable = new QCheckBox(m_instanceStoresTable);
        writable->setChecked(store.value("writable").toBool());
        connect(writable, &QCheckBox::toggled, this, [this, id](bool value) { updateInstanceStore(id, { { "writable", value } }); });
        m_instanceStoresTable->setCellWidget(row, 4, writable);
        auto* defaultStore = new QCheckBox(m_instanceStoresTable);
        defaultStore->setChecked(store.value("defaultForNewInstances").toBool());
        defaultStore->setEnabled(store.value("format").toString() == "prism");
        connect(defaultStore, &QCheckBox::toggled, this, [this, id](bool value) { updateInstanceStore(id, { { "defaultForNewInstances", value } }); });
        m_instanceStoresTable->setCellWidget(row, 5, defaultStore);
        auto* actions = new QWidget(m_instanceStoresTable);
        auto* actionsLayout = new QHBoxLayout(actions);
        actionsLayout->setContentsMargins(0, 0, 0, 0);
        auto* open = new QPushButton(tr("Open"), actions);
        connect(open, &QPushButton::clicked, this, [this, path = store.value("path").toString()] {
            if (!DesktopServices::openPath(path))
                QMessageBox::critical(this, tr("Instance Stores"), tr("Could not open the instance directory: %1").arg(QDir::toNativeSeparators(path)));
        });
        actionsLayout->addWidget(open);
        auto* remove = new QPushButton(tr("Remove"), actions);
        remove->setEnabled(store.value("origin").toString() != "portable-derived");
        connect(remove, &QPushButton::clicked, this, [this, id] {
            QString error;
            if (!APPLICATION->removeInstanceStore(id, &error)) QMessageBox::critical(this, tr("Instance Stores"), error);
            refreshInstanceStores();
        });
        actionsLayout->addWidget(remove);
        m_instanceStoresTable->setCellWidget(row, 6, actions);
    }
}

void InstanceStoresPage::updateInstanceStore(const QString& id, const QJsonObject& changes)
{
    for (const auto& value : APPLICATION->instanceStoresStatus()) {
        auto store = value.toObject();
        if (store.value("id").toString() != id)
            continue;
        for (auto it = changes.begin(); it != changes.end(); ++it)
            store.insert(it.key(), it.value());
        QString error;
        if (!APPLICATION->addOrUpdateInstanceStore(store, &error))
            QMessageBox::critical(this, tr("Instance Stores"), error);
        refreshInstanceStores();
        return;
    }
}

void InstanceStoresPage::addInstanceStore()
{
    const auto path = QFileDialog::getExistingDirectory(this, tr("Add instance directory"));
    if (path.isEmpty())
        return;
    QMessageBox formatBox(this);
    formatBox.setWindowTitle(tr("Instance directory format"));
    formatBox.setText(tr("Choose the directory format."));
    auto* prism = formatBox.addButton(tr("Prism"), QMessageBox::AcceptRole);
    auto* vanilla = formatBox.addButton(tr("Vanilla/PCL/HMCL"), QMessageBox::ActionRole);
    formatBox.addButton(QMessageBox::Cancel);
    formatBox.exec();
    if (formatBox.clickedButton() != prism && formatBox.clickedButton() != vanilla)
        return;
    const bool isVanilla = formatBox.clickedButton() == vanilla;
    QString error;
    if (!APPLICATION->addOrUpdateInstanceStore(QJsonObject{ { "path", FS::NormalizePath(path) }, { "format", isVanilla ? QStringLiteral("vanilla-compatible") : QStringLiteral("prism") }, { "enabled", true }, { "writable", true } }, &error)) {
        QMessageBox::critical(this, tr("Instance Stores"), error);
        return;
    }
    refreshInstanceStores();
}

void InstanceStoresPage::retranslate()
{
    m_instanceStoresBox->setTitle(tr("Instance Stores"));
    m_instanceStoresTable->setHorizontalHeaderLabels({ tr("Name"), tr("Path"), tr("Format"), tr("Enabled"), tr("Writable"), tr("Default"), tr("Actions") });
    m_addInstanceStoreButton->setText(tr("Add instance directory"));
    refreshInstanceStores();
}
