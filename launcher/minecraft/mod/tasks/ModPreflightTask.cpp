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
 * Ported from R:/mc_mod_preflight_demo/mc_mod_preflight.py (resolve() and
 * helpers: normalize_mod_id, side_matches, loader_compatible, build_order_edges,
 * find_cycle, add_virtuals).
 */

#include "ModPreflightTask.h"

#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <functional>

#include "FileSystem.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/Component.h"
#include "minecraft/PackProfile.h"
#include "minecraft/mod/Mod.h"
#include "minecraft/mod/ModVersionRange.h"
#include "modplatform/ModIndex.h"

namespace {

// Python: normalize_mod_id() — lowercase, treat - and _ as equivalent.
QString normalizeModId(QString id)
{
    return id.toLower().replace('_', '-');
}

// Python: side_matches()
bool sideMatches(DepSide required, const QString& activeSide)
{
    if (required == DepSide::Both)
        return true;
    if (required == DepSide::Client)
        return activeSide == "client";
    if (required == DepSide::Server)
        return activeSide == "server";
    return true;  // Unknown -> don't filter out
}

// The mod's own environment ("client"/"server"/"both") vs the active side.
bool environmentMatches(const QString& environment, const QString& activeSide)
{
    QString e = environment.isEmpty() ? "both" : environment.toLower();
    if (e == "both" || e == "*")
        return true;
    return e == activeSide;
}

// Loader compatibility: is a mod authored for `modLoader` loadable on an
// instance whose primary loader is `instanceLoader`? Quilt runs Fabric mods
// (compatibility warning), NeoForge 1.20.1 runs Forge mods. Returns
// (compatible, severityIfNotError).
struct LoaderCompat {
    bool ok;
    ProblemSeverity sev;  // Warning if "compatible but discouraged", Error if not
};

LoaderCompat loaderCompatible(ModPlatform::ModLoaderType modLoader, ModPlatform::ModLoaderType instanceLoader)
{
    if (modLoader == instanceLoader)
        return { true, ProblemSeverity::None };
    // Quilt runs Fabric mods.
    if (instanceLoader == ModPlatform::Quilt && modLoader == ModPlatform::Fabric)
        return { true, ProblemSeverity::Warning };
    // NeoForge on 1.20.1 runs Forge mods.
    if (instanceLoader == ModPlatform::NeoForge && modLoader == ModPlatform::Forge)
        return { true, ProblemSeverity::Warning };
    // Cleanroom is a 1.12 Forge fork — runs Forge mods.
    // TODO: Revisit if Cleanroom mods diverge from Forge compatibility.
    if (instanceLoader == ModPlatform::Cleanroom && modLoader == ModPlatform::Forge)
        return { true, ProblemSeverity::None };
    return { false, ProblemSeverity::Error };
}

// Infer a mod's authored loader from the metadata file it was parsed from.
// mods.toml/neoforge.mods.toml -> Forge or NeoForge; fabric.mod.json -> Fabric;
// quilt.mod.json -> Quilt; mcmod.info -> Forge. This is a best-effort heuristic
// since mod details don't explicitly carry the loader family.
ModPlatform::ModLoaderType loaderFromSource(const QString& source)
{
    if (source == "fabric.mod.json")
        return ModPlatform::Fabric;
    if (source == "quilt.mod.json")
        return ModPlatform::Quilt;
    if (source == "mods.toml")
        return ModPlatform::Forge;
    // mcmod.info and unknown legacy formats: assume Forge.
    return ModPlatform::Forge;
}

// Inverse of loaderFromSource: map a ModLoaderType to the range-evaluation
// loader name expected by ModVersionRange (forge/neoforge -> maven ranges).
QString loaderStringForType(ModPlatform::ModLoaderType t)
{
    switch (t) {
        case ModPlatform::NeoForge:
            return "neoforge";
        case ModPlatform::Quilt:
            return "quilt";
        case ModPlatform::Fabric:
            return "fabric";
        case ModPlatform::LiteLoader:
            return "liteloader";
        default:
            return "forge";
    }
}

// Python: build_order_edges + find_cycle. Only BEFORE/AFTER ordering edges
// contribute; do a DFS to detect a cycle. Returns the cycle path if found.
QStringList findLoadOrderCycle(const QList<ModDependencyEdge>& edges)
{
    QHash<QString, QStringList> adj;  // from -> [to...]  (only ordering edges)
    QSet<QString> nodes;
    for (const auto& e : edges) {
        if (e.ordering == DepOrder::Before) {
            adj[e.fromModId].append(e.toModId);
            nodes.insert(e.fromModId);
            nodes.insert(e.toModId);
        }
    }
    // DFS cycle detection.
    enum { White, Gray, Black };
    QHash<QString, int> color;
    QStringList stack;
    QStringList cycle;
    bool found = false;

    std::function<bool(const QString&)> dfs = [&](const QString& node) -> bool {
        color[node] = Gray;
        stack.append(node);
        for (const QString& nb : adj.value(node)) {
            if (!color.contains(nb)) {
                if (dfs(nb))
                    return true;
            } else if (color.value(nb) == Gray) {
                // Found a back edge -> cycle.
                int start = stack.indexOf(nb);
                if (start >= 0) {
                    cycle = stack.mid(start);
                    cycle.append(nb);
                }
                return true;
            }
        }
        stack.removeLast();
        color[node] = Black;
        return false;
    };

    for (const QString& n : nodes) {
        if (!color.contains(n)) {
            if (dfs(n)) {
                found = true;
                break;
            }
        }
    }
    (void)found;
    return cycle;
}

}  // namespace

ModPreflightTask::ModPreflightTask(MinecraftInstance* inst, QList<Mod*> mods) : m_inst(inst), m_mods(std::move(mods))
{
    m_result = std::make_shared<Result>();
}

void ModPreflightTask::executeTask()
{
    setStatus(tr("Analyzing mods..."));

    auto profile = m_inst->getPackProfile();
    QString mcVersion = profile->getComponentVersion("net.minecraft");

    // Determine the instance's primary loader and version.
    auto loaders = profile->getModLoadersList();
    ModPlatform::ModLoaderType instanceLoader = ModPlatform::Forge;  // default fallback
    QString instanceLoaderName = "forge";
    QString loaderVersion;
    // Cleanroom is a 1.12 Forge fork. CurseForge/Modrinth APIs don't have a
    // "cleanroom" loader category, so we map it to "forge" for API queries while
    // keeping ModPlatform::Cleanroom for internal platform identification.
    // TODO: Revisit when CurseForge/Modrinth add native Cleanroom loader support.
    bool isCleanroom = loaders.contains(ModPlatform::Cleanroom);
    auto primaryLoaderStr = [&]() -> QString {
        if (loaders.isEmpty())
            return {};
        if (loaders.contains(ModPlatform::NeoForge))
            return "neoforge";
        if (isCleanroom)
            return "forge";
        if (loaders.contains(ModPlatform::Forge))
            return "forge";
        if (loaders.contains(ModPlatform::Quilt))
            return "quilt";
        if (loaders.contains(ModPlatform::Fabric))
            return "fabric";
        if (loaders.contains(ModPlatform::LiteLoader))
            return "liteloader";
        return "forge";
    }();
    auto loaderComponent = [&]() -> QString {
        if (isCleanroom)
            return "com.cleanroommc.cleanroom";
        if (primaryLoaderStr == "neoforge")
            return "net.neoforged";
        if (primaryLoaderStr == "forge")
            return "net.minecraftforge";
        if (primaryLoaderStr == "quilt")
            return "org.quiltmc.quilt-loader";
        if (primaryLoaderStr == "fabric")
            return "net.fabricmc.fabric-loader";
        if (primaryLoaderStr == "liteloader")
            return "com.mumfrey.liteloader";
        return {};
    }();
    loaderVersion = profile->getComponentVersion(loaderComponent);
    if (isCleanroom) {
        instanceLoader = ModPlatform::Cleanroom;
        instanceLoaderName = "cleanroom";
    } else if (primaryLoaderStr == "neoforge") {
        instanceLoader = ModPlatform::NeoForge;
    } else if (primaryLoaderStr == "quilt") {
        instanceLoader = ModPlatform::Quilt;
    } else if (primaryLoaderStr == "fabric") {
        instanceLoader = ModPlatform::Fabric;
    } else if (primaryLoaderStr == "liteloader") {
        instanceLoader = ModPlatform::LiteLoader;
    } else {
        instanceLoader = ModPlatform::Forge;
    }
    if (instanceLoaderName.isEmpty())
        instanceLoaderName = primaryLoaderStr;

    // Java: prefer the instance's configured Java version; fall back to the
    // Minecraft component's compatibleJavaMajors when unset.
    int runtimeJava = 0;
    auto javaVersion = m_inst->getJavaVersion();
    if (javaVersion.major() > 0) {
        runtimeJava = javaVersion.major();
    } else {
        auto majors = profile->getProfile()->getCompatibleJavaMajors();
        for (int m : majors)
            runtimeJava = qMax(runtimeJava, m);
    }

    // Active side — Prism is a client launcher.
    QString activeSide = "client";

    // ---- Build the by_id map of provided mods (real + virtual). ----
    struct Provided {
        QString modId;       // normalized
        QString displayId;   // original
        QString version;
        QString environment;
        QString fileName;
        bool virtualMod;
        // For loader-mismatch checks we need the mod's authored loader. Derived
        // from its first dependency edge's source (best-effort).
        ModPlatform::ModLoaderType authoredLoader = ModPlatform::Forge;
    };

    QHash<QString, QList<Provided>> byId;
    QList<Provided> allProvided;

    for (Mod* mod : m_mods) {
        if (!mod)
            continue;
        const auto& d = mod->details();
        if (d.mod_id.isEmpty())
            continue;  // unresolvable / unknown mod

        Provided p;
        p.modId = normalizeModId(d.mod_id);
        p.displayId = d.mod_id;
        p.version = d.version;
        p.environment = d.environment.isEmpty() ? QStringLiteral("both") : d.environment.toLower();
        p.fileName = mod->fileinfo().fileName();
        p.virtualMod = false;
        // Infer authored loader from metadata source.
        if (!d.dependencyEdges.isEmpty()) {
            QString src = d.dependencyEdges.first().source;
            p.authoredLoader = loaderFromSource(src);
        }

        byId[p.modId].append(p);
        allProvided.append(p);

        // Snapshot for the tree view.
        Result::InstalledMod im;
        im.modId = d.mod_id;
        im.version = d.version;
        im.environment = p.environment;
        im.fileName = p.fileName;
        m_result->installedMods.append(im);
    }

    // Inject virtual mods with the instance's actual versions so edges that
    // target minecraft/java/<loader> resolve against real values.
    auto injectVirtual = [&](const QString& id, const QString& version) {
        Provided p;
        p.modId = normalizeModId(id);
        p.displayId = id;
        p.version = version;
        p.environment = "both";
        p.virtualMod = true;
        byId[p.modId].append(p);
        allProvided.append(p);
    };
    injectVirtual("minecraft", mcVersion);
    injectVirtual("java", runtimeJava > 0 ? QString::number(runtimeJava) : QString());
    if (instanceLoaderName == "fabric") {
        injectVirtual("fabricloader", loaderVersion);
        injectVirtual("fabric_loader", loaderVersion);
    } else if (instanceLoaderName == "quilt") {
        injectVirtual("quilt_loader", loaderVersion);
        injectVirtual("quiltloader", loaderVersion);
    } else if (instanceLoaderName == "forge") {
        injectVirtual("forge", loaderVersion);
        injectVirtual("fml", loaderVersion);
    } else if (instanceLoaderName == "neoforge") {
        injectVirtual("neoforge", loaderVersion);
    } else if (instanceLoaderName == "liteloader") {
        injectVirtual("liteloader", loaderVersion);
    }

    QList<Issue> issues;

    auto addIssue = [&](ProblemSeverity sev, const QString& code, const QString& msg, const QString& fromId,
                        const QStringList& related) {
        Issue i;
        i.severity = sev;
        i.code = code;
        i.message = msg;
        i.fromModId = fromId;
        i.related = related;
        issues.append(i);
    };

    // ---- Loader / side compatibility per mod. ----
    for (const auto& p : allProvided) {
        if (p.virtualMod)
            continue;
        auto compat = loaderCompatible(p.authoredLoader, instanceLoader);
        if (!compat.ok) {
            addIssue(ProblemSeverity::Error, "loader-mismatch",
                     tr("Mod '%1' is authored for a different loader than this instance's %2.").arg(p.displayId, instanceLoaderName),
                     p.displayId, { p.fileName });
        } else if (compat.sev == ProblemSeverity::Warning) {
            addIssue(ProblemSeverity::Warning, "loader-compat-warning",
                     tr("Mod '%1' runs on %2 via compatibility, which may be unstable.").arg(p.displayId, instanceLoaderName),
                     p.displayId, { p.fileName });
        }
        if (!environmentMatches(p.environment, activeSide)) {
            addIssue(ProblemSeverity::Error, "side-mismatch",
                     tr("Mod '%1' is %2-only but this is a %3 instance.").arg(p.displayId, p.environment, activeSide), p.displayId,
                     { p.fileName });
        }
    }

    // ---- Duplicate mod id (multiple real providers). ----
    for (auto it = byId.constBegin(); it != byId.constEnd(); ++it) {
        QList<Provided> realProviders;
        for (const auto& p : it.value())
            if (!p.virtualMod)
                realProviders.append(p);
        if (realProviders.size() > 1) {
            QStringList files;
            for (const auto& p : realProviders)
                files.append(p.fileName);
            addIssue(ProblemSeverity::Error, "duplicate-mod-id",
                     tr("Multiple mods provide the id '%1'.").arg(realProviders.first().displayId), realProviders.first().displayId, files);
        }
    }

    // ---- Collect all dependency edges and resolve each against byId. ----
    QList<ModDependencyEdge> allEdges;
    QSet<QString> seenEdgeKeys;  // dedupe identical edges (virtual injection etc.)
    for (Mod* mod : m_mods) {
        if (!mod)
            continue;
        const auto& d = mod->details();
        if (d.mod_id.isEmpty())
            continue;
        for (const auto& e : d.dependencyEdges) {
            QString key = normalizeModId(e.fromModId) + "|" + normalizeModId(e.toModId) + "|" + QString::number((int)e.kind) + "|" +
                          e.versionRange;
            if (seenEdgeKeys.contains(key))
                continue;
            seenEdgeKeys.insert(key);
            ModDependencyEdge normalized = e;
            normalized.fromModId = normalizeModId(e.fromModId);
            normalized.toModId = normalizeModId(e.toModId);
            allEdges.append(normalized);
            m_result->depEdges.append({ mod->details().mod_id, e });
        }
    }

    // Resolver: map a normalized id to candidate providers, filtered by side
    // and version range (per Python resolve() lines ~1322-1349).
    auto candidatesFor = [&](const QString& id, DepSide edgeSide, const QString& range) {
        QList<Provided> active;   // present on this side
        QList<Provided> matched;  // also satisfy the version range
        auto it = byId.constFind(id);
        if (it == byId.constEnd())
            return QPair<QList<Provided>, QList<Provided>>(active, matched);
        for (const auto& c : it.value()) {
            if (!sideMatches(edgeSide, activeSide))
                continue;
            if (!environmentMatches(c.environment, activeSide))
                continue;
            active.append(c);
            // Resolve the loader for range evaluation: virtual deps use the
            // instance loader; otherwise use the candidate's authored loader.
            QString evalLoader = c.virtualMod ? instanceLoaderName : loaderStringForType(c.authoredLoader);
            if (ModVersionRange::satisfiesRange(evalLoader, c.version, range))
                matched.append(c);
        }
        return QPair<QList<Provided>, QList<Provided>>(active, matched);
    };
    (void)candidatesFor;  // used below

    for (const auto& e : allEdges) {
        auto res = candidatesFor(e.toModId, e.side, e.versionRange);
        auto active = res.first;
        auto matched = res.second;
        bool presentAnyVersion = !active.isEmpty();
        bool matchedAny = !matched.isEmpty();

        auto installedVersionsStr = [&]() {
            QStringList v;
            for (const auto& c : active)
                v.append(c.displayId + " " + c.version);
            return v.join(", ");
        };
        QString rangeStr = e.versionRange.isEmpty() ? "*" : e.versionRange;

        if (e.kind == DepKind::Required) {
            if (!presentAnyVersion) {
                addIssue(ProblemSeverity::Error, "missing-required-dependency",
                         tr("Mod '%1' requires '%2' %3, but it is missing.").arg(e.fromModId, e.toModId, rangeStr), e.fromModId,
                         { e.toModId });
            } else if (!matchedAny) {
                addIssue(ProblemSeverity::Error, "dependency-version-mismatch",
                         tr("Mod '%1' requires '%2' %3, but installed version is %4.").arg(e.fromModId, e.toModId, rangeStr, installedVersionsStr()),
                         e.fromModId, { e.toModId });
            }
        } else if (e.kind == DepKind::Optional || e.kind == DepKind::Embedded) {
            if (presentAnyVersion && !matchedAny) {
                addIssue(ProblemSeverity::Warning, "optional-dependency-version-mismatch",
                         tr("Optional dependency '%1' of '%2' is installed but does not satisfy %3; installed %4.")
                             .arg(e.toModId, e.fromModId, rangeStr, installedVersionsStr()),
                         e.fromModId, { e.toModId });
            }
        } else if (e.kind == DepKind::Recommended || e.kind == DepKind::Suggested) {
            if (!presentAnyVersion) {
                addIssue(e.kind == DepKind::Suggested ? ProblemSeverity::Warning : ProblemSeverity::Warning,
                         QString("%1-dependency-missing").arg(e.kind == DepKind::Recommended ? "recommended" : "suggested"),
                         tr("%1 dependency '%2' %3 of '%4' is missing.")
                             .arg(e.kind == DepKind::Recommended ? "Recommended" : "Suggested", e.toModId, rangeStr, e.fromModId),
                         e.fromModId, { e.toModId });
            } else if (!matchedAny) {
                addIssue(ProblemSeverity::Warning,
                         QString("%1-dependency-version-mismatch").arg(e.kind == DepKind::Recommended ? "recommended" : "suggested"),
                         tr("%1 dependency '%2' of '%3' does not satisfy %4.")
                             .arg(e.kind == DepKind::Recommended ? "Recommended" : "Suggested", e.toModId, e.fromModId, rangeStr),
                         e.fromModId, { e.toModId });
            }
        } else if (e.kind == DepKind::Conflict || e.kind == DepKind::Discouraged) {
            if (matchedAny) {
                addIssue(ProblemSeverity::Warning, QString("declared-%1").arg(e.kind == DepKind::Conflict ? "conflict" : "discouraged"),
                         tr("Mod '%1' declares %2 with installed '%3' %4.")
                             .arg(e.fromModId, e.kind == DepKind::Conflict ? "a conflict" : "discouraged", e.toModId, rangeStr),
                         e.fromModId, { e.toModId });
            }
        } else if (e.kind == DepKind::Break || e.kind == DepKind::Incompatible) {
            if (matchedAny) {
                addIssue(ProblemSeverity::Error, QString("declared-%1").arg(e.kind == DepKind::Break ? "break" : "incompatible"),
                         tr("Mod '%1' declares hard incompatibility with installed '%2' %3.").arg(e.fromModId, e.toModId, rangeStr),
                         e.fromModId, { e.toModId });
            }
        }
    }

    // ---- Load-order cycle detection (Forge/NeoForge BEFORE edges). ----
    QStringList cycle = findLoadOrderCycle(allEdges);
    if (!cycle.isEmpty()) {
        addIssue(ProblemSeverity::Error, "load-order-cycle", tr("Load ordering cycle: %1").arg(cycle.join(" -> ")), QString(), cycle);
    }

    // ---- Java dependency check (mod requires a newer Java than the runtime). ----
    if (runtimeJava > 0) {
        for (const auto& e : allEdges) {
            if (e.toModId != "java" || e.kind != DepKind::Required)
                continue;
            // The required Java version is typically a bare major like ">=17" or "21".
            // Extract the numeric floor and compare to runtimeJava.
            QRegularExpression numRe(QStringLiteral("(\\d+)"));
            auto m = numRe.match(e.versionRange);
            if (m.hasMatch()) {
                int needed = m.captured(1).toInt();
                if (needed > runtimeJava) {
                    addIssue(ProblemSeverity::Error, "java-version-too-low",
                             tr("Mod '%1' requires Java %2, but the instance runs Java %3.").arg(e.fromModId, QString::number(needed), QString::number(runtimeJava)),
                             e.fromModId, {});
                }
            }
        }
    }

    // ---- Sort issues: errors first, then warnings, then by code. ----
    std::sort(issues.begin(), issues.end(), [](const Issue& a, const Issue& b) {
        int ra = a.severity == ProblemSeverity::Error ? 0 : (a.severity == ProblemSeverity::Warning ? 1 : 2);
        int rb = b.severity == ProblemSeverity::Error ? 0 : (b.severity == ProblemSeverity::Warning ? 1 : 2);
        if (ra != rb)
            return ra < rb;
        if (a.code != b.code)
            return a.code < b.code;
        return a.fromModId < b.fromModId;
    });

    for (const auto& i : issues) {
        if (i.severity == ProblemSeverity::Error)
            m_result->errorCount++;
        else if (i.severity == ProblemSeverity::Warning)
            m_result->warningCount++;
    }
    m_result->issues = issues;

    emitSucceeded();
}
