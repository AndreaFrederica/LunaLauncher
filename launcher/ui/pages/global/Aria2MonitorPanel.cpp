// SPDX-License-Identifier: GPL-3.0-only

#include "ui/pages/global/Aria2MonitorPanel.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

#include "StringUtils.h"
#include "net/Aria2Manager.h"

Aria2MonitorPanel::Aria2MonitorPanel(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle(tr("aria2 Monitor"));
    resize(920, 420);

    auto central = new QWidget(this);
    auto layout = new QVBoxLayout(central);
    auto top = new QWidget(central);
    auto topLayout = new QHBoxLayout(top);
    topLayout->setContentsMargins(0, 0, 0, 0);

    m_status = new QLabel(top);
    auto start = new QPushButton(tr("Start"), top);
    auto stop = new QPushButton(tr("Stop"), top);
    auto refreshButton = new QPushButton(tr("Refresh"), top);
    auto clear = new QPushButton(tr("Clear Finished"), top);
    m_showFinished = new QCheckBox(tr("Show finished"), top);
    topLayout->addWidget(m_status, 1);
    topLayout->addWidget(m_showFinished);
    topLayout->addWidget(start);
    topLayout->addWidget(stop);
    topLayout->addWidget(refreshButton);
    topLayout->addWidget(clear);
    layout->addWidget(top);

    m_table = new QTableWidget(0, 7, central);
    m_table->setHorizontalHeaderLabels({ tr("GID"), tr("Status"), tr("Progress"), tr("Speed"), tr("File"), tr("URI"), tr("Error") });
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(m_table, 1);
    setCentralWidget(central);

    connect(start, &QPushButton::clicked, this, &Aria2MonitorPanel::startAria2);
    connect(stop, &QPushButton::clicked, this, &Aria2MonitorPanel::stopAria2);
    connect(refreshButton, &QPushButton::clicked, this, &Aria2MonitorPanel::refresh);
    connect(m_showFinished, &QCheckBox::toggled, this, &Aria2MonitorPanel::refresh);
    connect(clear, &QPushButton::clicked, Net::Aria2Manager::instance(), &Net::Aria2Manager::clearFinished);
    connect(Net::Aria2Manager::instance(), &Net::Aria2Manager::downloadsChanged, this, &Aria2MonitorPanel::refresh);
    connect(Net::Aria2Manager::instance(), &Net::Aria2Manager::statusChanged, this, &Aria2MonitorPanel::refresh);
    refresh();
}

void Aria2MonitorPanel::startAria2()
{
    QString error;
    if (!Net::Aria2Manager::instance()->ensureStarted(&error)) {
        QMessageBox::warning(this, tr("aria2"), error);
    }
    refresh();
}

void Aria2MonitorPanel::stopAria2()
{
    Net::Aria2Manager::instance()->shutdown();
    refresh();
}

void Aria2MonitorPanel::refresh()
{
    auto manager = Net::Aria2Manager::instance();
    m_status->setText(manager->isRunning() ? tr("aria2: %1").arg(manager->statusText()) : tr("aria2: %1").arg(manager->statusText()));

    QList<Net::Aria2DownloadInfo> downloads;
    for (const auto& info : manager->downloads()) {
        if (!m_showFinished->isChecked() && info.status == "complete") {
            continue;
        }
        downloads.append(info);
    }

    m_table->setRowCount(downloads.size());
    for (int row = 0; row < downloads.size(); ++row) {
        const auto& info = downloads.at(row);
        const QString progress =
            QString("%1 / %2").arg(StringUtils::humanReadableFileSize(info.completedLength),
                                   info.totalLength > 0 ? StringUtils::humanReadableFileSize(info.totalLength) : tr("unknown"));
        const QString speed = QString("%1/s").arg(StringUtils::humanReadableFileSize(info.downloadSpeed));

        const QStringList values = {
            info.gid, info.status, progress, speed, info.path, info.url, info.errorMessage.isEmpty() ? info.errorCode : info.errorMessage
        };
        for (int col = 0; col < values.size(); ++col) {
            auto item = m_table->item(row, col);
            if (!item) {
                item = new QTableWidgetItem();
                m_table->setItem(row, col, item);
            }
            item->setText(values.at(col));
        }
    }
}
