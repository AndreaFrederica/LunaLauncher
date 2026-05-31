// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QMainWindow>

class QLabel;
class QCheckBox;
class QTableWidget;

class Aria2MonitorPanel : public QMainWindow {
    Q_OBJECT

   public:
    explicit Aria2MonitorPanel(QWidget* parent = nullptr);

   private slots:
    void refresh();
    void startAria2();
    void stopAria2();

   private:
    QLabel* m_status = nullptr;
    QCheckBox* m_showFinished = nullptr;
    QTableWidget* m_table = nullptr;
};
