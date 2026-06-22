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
 * Mod preflight (conflict) checker. Statically analyzes the mods installed in
 * an instance BEFORE launching Minecraft: builds the dependency graph, reports
 * missing/unmet required dependencies, version-range mismatches, declared
 * break/incompatible conflicts, duplicate mod ids, loader/side mismatches, and
 * load-order cycles. Ported from the reference Python implementation at
 * R:/mc_mod_preflight_demo/mc_mod_preflight.py (resolve(), lines ~1276-1382).
 */

#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include "ProblemProvider.h"
#include "minecraft/mod/ModDependency.h"
#include "tasks/Task.h"

class MinecraftInstance;
class Mod;

class ModPreflightTask : public Task {
    Q_OBJECT
   public:
    struct Issue {
        ProblemSeverity severity;
        QString code;       // e.g. "missing-required-dependency"
        QString message;    // human-readable description
        QString fromModId;  // mod the issue originates from (may be empty)
        QStringList related;  // related mod ids / files
    };

    struct Result {
        QList<Issue> issues;
        // (fromModId, edge) pairs for the dependency-tree view. Includes every
        // edge declared by every installed mod plus injected virtual deps.
        QList<QPair<QString, ModDependencyEdge>> depEdges;
        // Installed mods snapshot (modId -> version -> side) used to build the
        // dependency tree's top-level nodes.
        struct InstalledMod {
            QString modId;
            QString version;
            QString environment;
            QString fileName;
        };
        QList<InstalledMod> installedMods;
        int errorCount = 0;
        int warningCount = 0;
    };

    using ResultPtr = std::shared_ptr<Result>;
    ResultPtr result() const { return m_result; }

    ModPreflightTask(MinecraftInstance* inst, QList<Mod*> mods);
    void executeTask() override;

   private:
    MinecraftInstance* m_inst;
    QList<Mod*> m_mods;
    ResultPtr m_result;
};
