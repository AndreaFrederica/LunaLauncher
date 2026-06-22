// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * Report dialog for the mod preflight checker. Shows a severity-colored list
 * of detected issues, a dependency-tree view of all installed mods, an
 * interactive dependency graph (QGraphicsView), and can export the graph as a
 * Graphviz .dot file.
 */

#pragma once

#include <QDialog>

#include "minecraft/mod/tasks/ModPreflightTask.h"

class QTreeWidget;
class QTreeWidgetItem;
class QLabel;
class QGraphicsView;

class ModPreflightDialog : public QDialog {
    Q_OBJECT
   public:
    ModPreflightDialog(QWidget* parent, ModPreflightTask::ResultPtr result);
    ~ModPreflightDialog() override = default;

   private slots:
    void exportDependencyGraph();

   private:
    void populateIssues();
    void populateDependencyTree();
    void populateDependencyGraph();

    // Build the dependency graph as a Graphviz DOT document string.
    QString buildDotDocument() const;

    ModPreflightTask::ResultPtr m_result;
    QTreeWidget* m_issuesTree = nullptr;
    QTreeWidget* m_depsTree = nullptr;
    QGraphicsView* m_graphView = nullptr;
    QLabel* m_summary = nullptr;
};
