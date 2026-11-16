// SPDX-License-Identifier: GPL-3.0-only
#include "LegacyJarModPack.h"

#include <QCryptographicHash>
#include <QDirIterator>
#include <QFile>
#include <QHash>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>

#include "MMCZip.h"
#include "archive/ArchiveWriter.h"
#include "minecraft/ImportedClientProfile.h"
#include "minecraft/mod/Mod.h"

namespace LegacyJarModPack {
namespace {
bool safePath(const QString& path)
{
    if (path.isEmpty() || QDir::isAbsolutePath(path) || path.contains(':') || path.contains('\\')) return false;
    for (const auto& part : path.split('/'))
        if (part == "." || part == "..") return false;
    return true;
}

bool writeJson(const QString& path, const QJsonObject& object)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) return false;
    QSaveFile file(path);
    const auto bytes = QJsonDocument(object).toJson();
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
}
} // namespace

QList<Candidate> findCandidates(const QStringList& files)
{
    QSet<QString> names(files.begin(), files.end());
    QSet<QString> classRoots;
    const QSet<QString> excluded{"assets", "libraries", "mods", "config", "saves", "resourcepacks", "shaderpacks"};
    for (const auto& path : files) {
        if (!safePath(path) || !path.endsWith(".class")) continue;
        const auto parts = path.split('/');
        const auto index = parts.indexOf("class");
        if (index > 0) classRoots.insert(parts.mid(0, index).join('/'));
    }
    QList<Candidate> candidates;
    for (const auto& path : files) {
        if (!safePath(path) || !path.endsWith(".json")) continue;
        auto parts = path.split('/');
        if (parts.size() < 2) continue;
        const auto version = parts.at(parts.size() - 2);
        const auto folder = parts.mid(0, parts.size() - 1).join('/');
        if (parts.last() != version + ".json" || !classRoots.contains(folder) || names.contains(folder + '/' + version + ".jar")) continue;
        bool resource = false;
        for (const auto& part : parts) resource |= excluded.contains(part.toLower());
        if (resource) continue;
        const auto parent = parts.mid(0, parts.size() - 2).join('/');
        candidates.append({parent.isEmpty() ? QString() : parent + '/', version});
    }
    return candidates;
}

QString baseVersion(const QJsonObject& profile, const QString& version, QString& error)
{
    if (profile.value("id").toString() != version || profile.value("mainClass").toString().isEmpty()
        || !profile.value("libraries").isArray()
        || (profile.value("minecraftArguments").toString().isEmpty() && !profile.value("arguments").isObject())) {
        error = QObject::tr("旧式 JAR 补丁包的版本 JSON 不完整：需要匹配的 id、主类、运行库和启动参数。");
        return {};
    }
    QSet<QString> explicitVersions;
    for (const auto& key : {"inheritsFrom", "minecraftVersion", "jar"}) {
        const auto value = profile.value(key).toString();
        if (!value.isEmpty() && value != version) explicitVersions.insert(value);
    }
    if (explicitVersions.size() > 1) {
        error = QObject::tr("旧式 JAR 补丁包声明了相互冲突的基础版本，无法自动安装。");
        return {};
    }
    auto base = explicitVersions.isEmpty() ? QString() : *explicitVersions.cbegin();
    // Only recognize a release-version prefix with a delimiter. The result
    // must resolve through the launcher's version metadata before any merge.
    static const QRegularExpression release("^(1\\.\\d+(?:\\.\\d+)?)(?:[-_ ].+)?$");
    if (base.isEmpty()) base = release.match(version).captured(1);
    if (!release.match(base).hasMatch() || release.match(base).captured(1) != base) {
        error = QObject::tr("无法确定旧式 JAR 补丁包的基础 Minecraft 版本；请在版本 JSON 中明确声明 minecraftVersion。");
        return {};
    }
    return base;
}

bool prepare(const QString& sourceRoot, const QString& version, const QJsonObject& baseProfile,
             const QString& baseJar, const VanillaConversionFeedback& feedback, QString& error)
{
    const auto input = QDir(sourceRoot).filePath(version);
    QFile json(QDir(input).filePath(version + ".json"));
    if (!json.open(QIODevice::ReadOnly)) { error = QObject::tr("无法读取旧式补丁包的版本 JSON。"); return false; }
    const auto original = QJsonDocument::fromJson(json.readAll()).object();
    const auto base = baseVersion(original, version, error);
    if (base.isEmpty() || baseProfile.value("version").toString() != base) {
        if (error.isEmpty()) error = QObject::tr("补丁基底与下载的 Minecraft 版本元数据不一致。");
        return false;
    }
    const auto expected = baseProfile.value("mainJar").toObject().value("downloads").toObject().value("artifact").toObject();
    const auto sourceRecord = ImportedClientProfile::inspectClient(baseJar, {}, &error);
    if (sourceRecord.isEmpty() || expected.value("sha1").toString().isEmpty()
        || sourceRecord.value("sha1").toString().compare(expected.value("sha1").toString(), Qt::CaseInsensitive)
        || sourceRecord.value("bytes").toInteger() != expected.value("size").toInteger()) {
        error = QObject::tr("基础 Minecraft JAR 校验失败，已停止应用补丁。");
        return false;
    }
    const auto output = QDir(sourceRoot).filePath("versions/" + version);
    const auto records = QDir(output).filePath("luna-installer");
    if (!QDir().mkpath(records)) { error = QObject::tr("无法创建补丁安装临时目录。"); return false; }
    const auto cancelled = [&] {
        if (!feedback.cancelled || !feedback.cancelled()) return false;
        error = QObject::tr("补丁安装已取消。"); return true;
    };
    const auto classes = QDir(input).filePath("class");
    QStringList files;
    QDirIterator iterator(classes, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        if (cancelled()) return false;
        const auto file = iterator.next();
        const auto relative = QDir(classes).relativeFilePath(file);
        if (!safePath(relative) || QFileInfo(file).isSymLink() || relative.startsWith("META-INF/", Qt::CaseInsensitive)) {
            error = QObject::tr("补丁目录包含不支持的链接、路径或签名文件：%1").arg(relative); return false;
        }
        files.append(relative);
    }
    files.sort();
    const auto patchPath = QDir(records).filePath("classes.jar");
    MMCZip::ArchiveWriter patch(patchPath);
    if (!patch.open()) { error = QObject::tr("无法创建 JAR 补丁文件。"); return false; }
    QJsonArray entries;
    int classCount = 0;
    for (const auto& relative : files) {
        if (cancelled()) return false;
        QFile file(QDir(classes).filePath(relative));
        if (!file.open(QIODevice::ReadOnly)) { error = QObject::tr("无法读取补丁文件：%1").arg(relative); return false; }
        const auto bytes = file.readAll();
        if (relative.endsWith(".class")) {
            if (bytes.size() < 8 || bytes.first(4).toHex() != "cafebabe") {
                error = QObject::tr("补丁包含无效的 Java class 文件：%1").arg(relative); return false;
            }
            ++classCount;
        }
        if (!patch.addFile(relative, bytes)) { error = QObject::tr("写入 JAR 补丁失败：%1").arg(relative); return false; }
        entries.append(QJsonObject{{"path", relative}, {"sha256", QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex())}});
        feedback.update(QObject::tr("正在整理 JAR 补丁"), entries.size(), files.size(), "files", relative);
    }
    if (!classCount || !patch.close()) { error = QObject::tr("补丁没有有效的 class 文件，或 JAR 补丁写入失败。"); return false; }
    MMCZip::ArchiveReader baseArchive(baseJar);
    if (!baseArchive.collectFiles()) { error = QObject::tr("基础客户端不是有效的 JAR 文件。"); return false; }
    qint64 processed = 0;
    const auto total = baseArchive.getFiles().size() + files.size();
    Mod mod(patchPath);
    const auto client = QDir(output).filePath(version + ".jar");
    if (!MMCZip::createModdedJar(baseJar, client, {&mod}, [&](const QString& path) {
        feedback.update(QObject::tr("正在合并客户端 JAR 补丁"), ++processed, total, "files", path);
        return !cancelled();
    })) {
        if (error.isEmpty()) error = QObject::tr("客户端 JAR 补丁合并失败。");
        return false;
    }
    // Validate the actual result, including every replacement and the entry
    // point, before handing it to the ordinary custom-client importer.
    QHash<QString, QString> pending;
    for (const auto& value : entries) {
        const auto entry = value.toObject();
        pending.insert(entry.value("path").toString(), entry.value("sha256").toString());
    }
    auto main = original.value("mainClass").toString().replace('.', '/') + ".class";
    bool foundMain = false;
    MMCZip::ArchiveReader result(client);
    if (!result.parse([&](MMCZip::ArchiveReader::File* file) {
        if (cancelled()) return false;
        const auto name = file->filename();
        if (name.startsWith("META-INF/", Qt::CaseInsensitive)) return false;
        foundMain |= name == main;
        if (!pending.contains(name)) return true;
        int status = 0;
        const auto bytes = file->readAll(&status);
        if (status < 0 || QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()) != pending.take(name)) return false;
        feedback.update(QObject::tr("正在校验合并后的 JAR"), entries.size() - pending.size(), entries.size(), "files", name);
        return true;
    }) || !pending.isEmpty() || !foundMain) {
        if (error.isEmpty()) error = QObject::tr("合并后的客户端未通过补丁内容或主类校验。");
        return false;
    }
    auto profile = original;
    profile.remove("inheritsFrom");
    profile.insert("minecraftVersion", base);
    profile.insert("jar", version);
    for (const auto& key : {"assetIndex", "assets", "compatibleJavaMajors", "compatibleJavaName"})
        if (!profile.contains(key) && baseProfile.contains(key)) profile.insert(key, baseProfile.value(key));
    // Never retain an official client repair URL on a modified client.
    auto downloads = profile.value("downloads").toObject(); downloads.remove("client");
    profile.insert("downloads", downloads);
    const auto resources = QDir(input).entryInfoList({"*.zip"}, QDir::Files);
    for (const auto& resource : resources) {
        if (cancelled()) return false;
        MMCZip::ArchiveReader archive(resource.absoluteFilePath());
        if (!archive.goToFile("pack.mcmeta")) continue;
        const auto target = QDir(output).filePath("resourcepacks/" + resource.fileName());
        if (!QDir().mkpath(QFileInfo(target).absolutePath()) || !QFile::copy(resource.absoluteFilePath(), target)) {
            error = QObject::tr("无法安装附带的资源包：%1").arg(resource.fileName()); return false;
        }
        feedback.update(QObject::tr("正在安装附带资源包"), 1, 1, "files", resource.fileName());
    }
    const auto artifact = ImportedClientProfile::inspectClient(client, {}, &error);
    const QJsonObject report{{"formatVersion", 1}, {"type", "legacy-class-overlay"}, {"minecraftVersion", base},
                             {"baseClient", sourceRecord}, {"client", artifact}, {"patchFiles", entries}};
    if (artifact.isEmpty() || !writeJson(QDir(records).filePath("source-profile.json"), original)
        || !writeJson(QDir(records).filePath("install-report.json"), report)
        || !writeJson(QDir(output).filePath(version + ".json"), profile)
        || !writeJson(QDir(output).filePath("hmclversion.cfg"), {{"gameDirType", 1}, {"usesGlobal", false}})) {
        error = QObject::tr("无法保存补丁安装记录或版本配置。"); return false;
    }
    return !cancelled();
}
} // namespace LegacyJarModPack
