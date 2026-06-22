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
 * Structured mod dependency data used by the mod preflight (conflict) checker.
 * Different loader metadata formats (fabric.mod.json, quilt.mod.json,
 * META-INF/mods.toml, mcmod.info) are normalized into ModDependencyEdge so the
 * preflight engine can reason about version ranges and dependency kinds
 * uniformly. See the reference implementation this is ported from:
 * R:/mc_mod_preflight_demo/mc_mod_preflight.py
 */

#pragma once

#include <QString>
#include <QStringList>

// Kind of a dependency edge, normalized across all loader metadata formats.
enum class DepKind {
    Required,      // missing or version-mismatch is an error
    Optional,      // installed-but-mismatch is a warning
    Recommended,   // missing is a warning
    Suggested,     // missing is informational
    Conflict,      // present match is a warning
    Break,         // present match is an error (Fabric "breaks")
    Incompatible,  // present match is an error (NeoForge "incompatible")
    Discouraged,   // present match is a warning (NeoForge "discouraged")
    Embedded       // bundled inside the providing jar (informational)
};

// Side a dependency applies to. Mirrors Fabric "environment" / Forge "side".
enum class DepSide { Both, Client, Server, Unknown };

// Load-ordering hint (Forge/NeoForge "ordering" field).
enum class DepOrder { None, Before, After };

// A single normalized dependency relationship from one mod to another.
//
// `versionRange` keeps the raw range expression exactly as declared by the
// mod (Fabric atom expressions like ">=1.2.0 <2.0.0", Maven ranges like
// "[1.0,2.0)", or "*" for any). The ModVersionRange module interprets it.
// An empty string is treated the same as "*".
struct ModDependencyEdge {
    QString fromModId;  // mod declaring this dependency
    QString toModId;    // dependency target (may be virtual: minecraft, java, ...)
    DepKind kind = DepKind::Required;
    QString versionRange;  // raw range expression; "" == "*"
    DepSide side = DepSide::Both;
    DepOrder ordering = DepOrder::None;
    QString source;  // metadata file the edge came from (e.g. "fabric.mod.json")

    ModDependencyEdge() = default;
    ModDependencyEdge(QString from, QString to, DepKind k, QString range, QString src)
        : fromModId(std::move(from)), toModId(std::move(to)), kind(k), versionRange(std::move(range)), source(std::move(src))
    {}

    static const char* kindToString(DepKind k);
    static const char* sideToString(DepSide s);
    static const char* orderToString(DepOrder o);
};
