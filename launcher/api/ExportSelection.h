// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include "MMCZip.h"

namespace ExportSelection {
inline bool contains(const QString& parent, const QString& path)
{
    return parent == "." || path == parent || path.startsWith(parent + '/');
}

inline QString gamePrefix(const QString& root, const QString& gameRoot)
{
    return QDir::fromNativeSeparators(QDir(root).relativeFilePath(gameRoot));
}

inline QString category(const QString& path, const QString& prefix)
{
    const auto gamePath = prefix == "." ? path : path.startsWith(prefix + '/') ? path.mid(prefix.size() + 1) : QString();
    if (gamePath.isEmpty()) {
        const auto top = path.section('/', 0, 0).toLower();
        if (top == "libraries" || top == "versions" || top == "patches" || top == "jarmods" || top == "mmc-pack.json") return "game";
        return "gameProfile";
    }
    const auto top = gamePath.section('/', 0, 0).toLower();
    if (top == "mods" || top == "coremods" || top == "nilmods") return "mod";
    if (top == "resourcepacks" || top == "texturepacks") return "resourcepacks";
    if (top == "shaderpacks" || top == "saves" || top == "screenshots") return top;
    if (top == "servers.dat" || top == "servers.dat_old") return "servers";
    if (top == "options.txt" || top.startsWith("optionsof") || top.startsWith("optionsshaders")) return "gameSettings";
    if (top == "libraries" || top == "versions" || top == "assets" || top == "bin" || top == "natives") return "game";
    return "gameProfile";
}

inline bool defaultSelected(const QString& path, const QString& prefix)
{
    const auto gamePath = prefix == "." ? path : path.startsWith(prefix + '/') ? path.mid(prefix.size() + 1) : QString();
    const auto top = (gamePath.isEmpty() ? path : gamePath).section('/', 0, 0).toLower();
    // Unknown development/mod-generated directories require an explicit choice.
    const QStringList known{ "instance.cfg", "mmc-pack.json", "patches", "jarmods", "libraries", "versions", "assets", "bin", "natives",
        "config", "defaultconfigs", "kubejs", "scripts", "resources", "global_packs", "datapacks", "lunaui",
        "mods", "coremods", "nilmods", "resourcepacks", "texturepacks", "shaderpacks", "options.txt", "saves", "screenshots", "servers.dat", "servers.dat_old" };
    return known.contains(top) || top.startsWith("optionsof") || top.startsWith("optionsshaders");
}

inline MMCZip::FilterFileFunction filter(const QString& rootPath, const QString& gameRoot, const QJsonObject& parameters)
{
    const QDir root(rootPath);
    const auto prefix = gamePrefix(rootPath, gameRoot);
    return [root, prefix, parameters](const QFileInfo& file) {
        const auto relative = QDir::fromNativeSeparators(root.relativeFilePath(file.absoluteFilePath()));
        if (file.isSymLink()) return true;
        for (const auto& value : parameters.value("exclude").toArray()) {
            const auto path = QDir::cleanPath(QDir::fromNativeSeparators(value.toString()));
            if (contains(path, relative)) return true;
        }
        if (parameters.contains("fileSelection")) {
            const auto rules = parameters.value("fileSelection").toObject();
            int specificity = -1;
            bool selected = false;
            bool selectedDescendant = false;
            for (auto it = rules.begin(); it != rules.end(); ++it) {
                const auto path = QDir::cleanPath(QDir::fromNativeSeparators(it.key()));
                if (contains(path, relative) && path.size() > specificity) {
                    specificity = path.size();
                    selected = it.value().toBool();
                }
                if (file.isDir() && contains(relative, path) && it.value().toBool()) selectedDescendant = true;
            }
            if (!selected && !selectedDescendant) return true;
        } else if (parameters.contains("include")) {
            bool selected = false;
            for (const auto& value : parameters.value("include").toArray()) {
                const auto path = QDir::cleanPath(QDir::fromNativeSeparators(value.toString()));
                if (contains(path, relative) || (file.isDir() && contains(relative, path))) { selected = true; break; }
            }
            if (!selected) return true;
        } else if (parameters.contains("items") && relative != prefix && !(file.isDir() && contains(relative, prefix))) {
            if (!parameters.value("items").toArray().contains(category(relative, prefix))) return true;
        }
        const auto lower = relative.toLower();
        if (relative == ".packignore" || lower.section('/', -1) == ".ds_store" || lower.section('/', -1) == "thumbs.db") return true;
        if (parameters.value("format").toString("zip") != "zip" &&
            (relative.endsWith(".pw.toml") || relative.split('/').contains(".index"))) return true;
        const auto gamePath = prefix == "." ? lower : lower.startsWith(prefix.toLower() + '/') ? lower.mid(prefix.size() + 1) : QString();
        if (!parameters.contains("fileSelection")) {
            if (!parameters.value("includeLogs").toBool() && contains("logs", gamePath)) return true;
            if (!parameters.value("includeCrashReports").toBool() && contains("crash-reports", gamePath)) return true;
            if (!parameters.value("includeCaches").toBool() &&
                (contains(".cache", gamePath) || contains(".fabric", gamePath) || contains(".quilt", gamePath))) return true;
        }
        return false;
    };
}
} // namespace ExportSelection
