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
 * Version-range satisfaction for mod dependency expressions.
 *
 * This is a faithful C++ port of the version-comparison and range-satisfaction
 * helpers in R:/mc_mod_preflight_demo/mc_mod_preflight.py (compare_versions,
 * satisfies_maven_range, satisfies_fabric_range/atom, bump_for_caret/tilde,
 * exact_or_prefix_match). It intentionally does NOT reuse launcher/Version.h
 * (FlexVer) because the tokenization semantics here are different and the
 * preflight report must match the reference implementation's verdicts.
 */

#pragma once

#include <QString>
#include <QStringView>

namespace ModVersionRange {

// Three-way comparison of two mod/MC version strings following the
// reference tokenizer: split on numeric/textual tokens, compare element-wise,
// numeric release segments sort newer than textual pre-release-ish segments.
// Returns <0 if a<b, 0 if equal, >0 if a>b.
int compareVersions(const QString& a, const QString& b);

// True if `version` satisfies `range` for the given `loader` (forge/neoforge
// use Maven ranges; everything else uses Fabric-style atom expressions).
// An empty / "*" / "NONE" range is always satisfied.
bool satisfiesRange(QStringView loader, const QString& version, const QString& range);

// Maven range evaluation (Forge / NeoForge), e.g. "[1.0,2.0)", "[1.5.0,)".
bool satisfiesMavenRange(const QString& version, const QString& range);

// Fabric-style range evaluation. `range` may be an OR-joined ("||") set of
// clauses; each clause is a space-separated AND of atoms like ">=1.2.0",
// "^1.0.0", "~1.2", "1.21.*".
bool satisfiesFabricRange(const QString& version, const QString& range);

}  // namespace ModVersionRange
