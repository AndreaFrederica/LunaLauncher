// SPDX-License-Identifier: GPL-3.0-only
#include "ImportedClientProfile.h"

#include "archive/ArchiveReader.h"
#include "minecraft/GradleSpecifier.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QtEndian>

namespace ImportedClientProfile {
namespace {
int mainClassJava(const QString& path, QString mainClass)
{
    if (!QFileInfo(path).isFile() || mainClass.isEmpty()) return 0;
    mainClass.replace('.', '/');
    MMCZip::ArchiveReader archive(path);
    auto entry = archive.goToFile(mainClass + ".class");
    if (!entry) return 0;
    int status = 0;
    const auto bytes = entry->readAll(&status);
    if (status < 0 || bytes.size() < 8) return 0;
    const auto* data = reinterpret_cast<const uchar*>(bytes.constData());
    if (qFromBigEndian<quint32>(data) != 0xCAFEBABE) return 0;
    const int major = qFromBigEndian<quint16>(data + 6);
    return major >= 45 ? major - 44 : 0;
}

QString localLibrary(const QString& root, const QJsonObject& library)
{
    const GradleSpecifier spec(library.value("name").toString());
    if (!spec.valid()) return {};
    const auto relative = QDir::cleanPath(QDir::fromNativeSeparators(library.value("downloads").toObject().value("artifact").toObject().value("path").toString(spec.toPath())));
    if (QDir::isAbsolutePath(relative) || relative.contains(':') || relative == ".." || relative.startsWith("../")) return {};
    QDir current(root);
    const auto parts = relative.split('/');
    for (qsizetype i = 0; i < parts.size(); ++i) {
        QString match;
        for (const auto& entry : current.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot)) {
            if (entry.fileName().compare(parts[i], Qt::CaseInsensitive) == 0) { match = entry.absoluteFilePath(); break; }
        }
        if (match.isEmpty()) return {};
        if (i == parts.size() - 1) return match;
        current.setPath(match);
    }
    return {};
}
} // namespace

QJsonObject inspectClient(const QString& path, const QJsonObject& profile, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() == 0) {
        if (error) *error = QObject::tr("Could not read the preserved client JAR: %1").arg(path);
        return {};
    }
    QCryptographicHash sha1(QCryptographicHash::Sha1), sha256(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        const auto bytes = file.read(1024 * 1024);
        if (bytes.isEmpty() && file.error() != QFile::NoError) {
            if (error) *error = file.errorString();
            return {};
        }
        sha1.addData(bytes); sha256.addData(bytes);
    }
    const auto digest = QString::fromLatin1(sha1.result().toHex());
    const auto declared = profile.value("downloads").toObject().value("client").toObject().value("sha1").toString();
    return {{"sha1", digest}, {"sha256", QString::fromLatin1(sha256.result().toHex())}, {"bytes", file.size()},
            {"sourceName", QFileInfo(path).fileName()}, {"declaredSha1", declared},
            {"comparison", declared.isEmpty() ? "unverified" : declared.compare(digest, Qt::CaseInsensitive) == 0 ? "matches-profile" : "differs-from-profile"}};
}

bool verifyClient(const QString& path, const QJsonObject& record, QString* error)
{
    if (record.value("sha256").toString().isEmpty()) {
        if (error) *error = QObject::tr("The preserved client has no SHA-256 verification record.");
        return false;
    }
    const auto current = inspectClient(path, {}, error);
    if (current.isEmpty()) return false;
    if (current.value("sha256") == record.value("sha256") && current.value("bytes") == record.value("bytes")) return true;
    if (error) *error = QObject::tr("The preserved client JAR has changed: %1. Restore the imported client; automatic replacement is disabled.").arg(path);
    return false;
}

QJsonObject localMainJar(const QJsonObject& record, const QString& filename)
{
    return {{"name", "org.lunalauncher.imported:client:" + record.value("sha256").toString().left(16)},
            {"MMC-hint", "local"}, {"MMC-filename", filename},
            {"downloads", QJsonObject{{"artifact", QJsonObject{{"url", ""}, {"sha1", record.value("sha1")}, {"size", record.value("bytes")}}}}}};
}

void inferJavaRequirement(QJsonObject& profile, const QString& librariesRoot, const QString& clientJar)
{
    if (profile.value("javaVersion").toObject().value("majorVersion").toInt() > 0) return;
    const auto mainClass = profile.value("mainClass").toString();
    int major = mainClassJava(clientJar, mainClass);
    for (const auto& value : profile.value("libraries").toArray())
        major = qMax(major, mainClassJava(localLibrary(librariesRoot, value.toObject()), mainClass));
    // Older class files only establish a minimum, not an exact Java runtime.
    // Preserve the launcher's legacy defaults unless the entry point needs 9+.
    if (major >= 9) profile.insert("javaVersion", QJsonObject{{"majorVersion", major}});
}
} // namespace ImportedClientProfile
