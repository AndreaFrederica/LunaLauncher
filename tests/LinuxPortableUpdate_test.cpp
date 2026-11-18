// SPDX-License-Identifier: GPL-3.0-only
#include <QtTest>
#include <QProcess>
#include "updater/prismupdater/PortableUpdate.h"

class LinuxPortableUpdateTest : public QObject {
    Q_OBJECT
    void write(const QString& path, const QByteArray& contents) {
        QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath())); QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write(contents), contents.size());
        file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    }
    QByteArray read(const QString& path) { QFile file(path); if (!file.open(QIODevice::ReadOnly)) return {}; return file.readAll(); }
    QJsonObject fixture(const QString& root, const QByteArray& contents, bool obsolete = false) {
        QStringList paths{"AppRun", "usr/bin/lunalauncher", "usr/bin/lunalauncher_updater", "usr/share/empty"};
        if (obsolete) paths.append("usr/share/obsolete");
        QJsonArray files;
        for (const auto& path : paths) {
            const auto payload = path == "usr/share/empty" ? QByteArray{} : contents;
            write(QDir(root).filePath(path), payload);
            files.append(QJsonObject{{"path", path}, {"size", payload.size()},
                {"sha256", QString::fromLatin1(QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex())}});
        }
        QJsonObject data{{"schemaVersion", 1}, {"protocol", 1}, {"version", "11.0.2-20261118"},
            {"format", "portable-tar"}, {"architecture", LinuxPortable::architecture()}, {"libc", "glibc"},
            {"minGlibc", "2.17"}, {"neoUi", false}, {"files", files}};
        write(root + "/.luna-portable.json", QJsonDocument(data).toJson()); return data;
    }
    void tarFile(archive* writer, const QString& path, const QByteArray& bytes, int type = AE_IFREG, const QString& link = {}) {
        auto* entry = archive_entry_new(); archive_entry_set_pathname(entry, path.toUtf8().constData());
        archive_entry_set_filetype(entry, type); archive_entry_set_perm(entry, 0755);
        if (type == AE_IFLNK) archive_entry_set_symlink(entry, link.toUtf8().constData());
        else archive_entry_set_size(entry, bytes.size());
        QCOMPARE(archive_write_header(writer, entry), ARCHIVE_OK);
        if (type == AE_IFREG) QCOMPARE(archive_write_data(writer, bytes.constData(), bytes.size()), la_ssize_t(bytes.size()));
        archive_entry_free(entry);
    }
    QJsonObject makeTar(const QString& path, const QString& root, const QString& unsafe = {}, const QString& link = {}) {
        auto* writer = archive_write_new(); archive_write_add_filter_gzip(writer); archive_write_set_format_pax_restricted(writer);
        if (archive_write_open_filename(writer, QFile::encodeName(path).constData()) != ARCHIVE_OK) return {};
        qint64 total = 0;
        if (!unsafe.isEmpty()) tarFile(writer, unsafe, "unsafe", link.isEmpty() ? AE_IFREG : AE_IFLNK, link);
        else {
            auto paths = PortableUpdate::records(LinuxPortable::metadata(root)); paths.append(".luna-portable.json");
            for (const auto& name : paths) { const auto contents = read(QDir(root).filePath(name)); total += contents.size(); tarFile(writer, name, contents); }
        }
        archive_write_close(writer); archive_write_free(writer);
        return {{"platform", "linux"}, {"format", "portable-tar"}, {"architecture", LinuxPortable::architecture()},
            {"libc", "glibc"}, {"minGlibc", "2.17"}, {"neoUi", false}, {"size", QFileInfo(path).size()},
            {"unpackedSize", qMax<qint64>(1, total)}, {"sha256", QString::fromLatin1(QCryptographicHash::hash(read(path), QCryptographicHash::Sha256).toHex())},
            {"asset", "LunaLauncher-Linux-x64-11.0.2-portable.tar.gz"}};
    }
    bool health(const QString& root) {
        QProcess process; auto env = AppImageUpdate::cleanEnvironment(); env.insert("QT_QPA_PLATFORM", "offscreen");
        env.insert("PATH", "/usr/bin:/bin"); env.insert("LUNA_LAUNCHER_DATA", root + "/usr/UserData");
        process.setProcessEnvironment(env);
        process.start(root + "/AppRun", {"--luna-entry", "classic", "--version"});
        if (!process.waitForFinished(60000)) { qWarning() << process.errorString(); process.kill(); process.waitForFinished(); return false; }
        const auto output = process.readAllStandardOutput();
        if (process.exitCode()) qWarning() << process.readAllStandardError();
        return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0 && output.contains("LunaLauncher 11.");
    }
private slots:
    void compatibility() {
        QTemporaryDir dir; fixture(dir.filePath("root"), "program");
        auto item = makeTar(dir.filePath("program.tar.gz"), dir.filePath("root")); item["minGlibc"] = "2.35";
        QJsonObject manifest{{"schemaVersion", 1}, {"protocol", 1}, {"version", "11.0.2"}, {"artifacts", QJsonArray{item}}};
        QVERIFY(!LinuxPortable::artifact(manifest, LinuxPortable::architecture(), false, "2.35").isEmpty());
        QVERIFY(LinuxPortable::artifact(manifest, LinuxPortable::architecture(), false, "2.31").isEmpty());
        QVERIFY(LinuxPortable::artifact(manifest, LinuxPortable::architecture(), false, "").isEmpty());
        QVERIFY(LinuxPortable::artifact(manifest, "other", false, "2.40").isEmpty());
        QVERIFY(LinuxPortable::artifact(manifest, LinuxPortable::architecture(), true, "2.40").isEmpty());
        item["libc"] = "musl"; manifest["artifacts"] = QJsonArray{item};
        QVERIFY(LinuxPortable::artifact(manifest, LinuxPortable::architecture(), false, "2.40").isEmpty());
        QCOMPARE(LinuxPortable::runningRoot("/usr/bin"), QString());
    }
    void replacementAndRollback() {
        QTemporaryDir dir;
        const auto target = dir.filePath(QString::fromUtf8("便携 启动器")), source = dir.filePath("source"), bundle = dir.filePath("next.tar.gz");
        fixture(target, "old", true); fixture(source, "new");
        write(target + "/usr/UserData/instances/world.dat", "world"); write(target + "/usr/.luna-data-dir", "/custom/data");
        write(target + "/extra.txt", "custom");
        const auto item = makeTar(bundle, source);
        const auto check = [](const QString&) { return true; };
        auto result = PortableUpdate::replace(target, bundle, item, "11.0.2-20261118", target + "/usr/UserData", check);
        QVERIFY2(result.success, qPrintable(result.error)); QCOMPARE(read(target + "/AppRun"), QByteArray("new"));
        QCOMPARE(read(target + "/usr/UserData/instances/world.dat"), QByteArray("world"));
        QCOMPARE(read(target + "/usr/.luna-data-dir"), QByteArray("/custom/data"));
        QCOMPARE(read(target + "/extra.txt"), QByteArray("custom")); QVERIFY(!QFileInfo::exists(target + "/usr/share/obsolete"));
        QCOMPARE(read(result.backup + "/usr/share/obsolete"), QByteArray("old"));
        int calls = 0;
        result = PortableUpdate::replace(target, bundle, item, "11.0.2-20261118", target + "/usr/UserData", [&](const QString&) { return ++calls == 1; });
        QVERIFY(!result.success); QCOMPARE(calls, 2);
        QCOMPARE(read(target + "/AppRun"), QByteArray("new")); QCOMPARE(read(target + "/usr/UserData/instances/world.dat"), QByteArray("world"));
        QCOMPARE(read(target + "/usr/.luna-data-dir"), QByteArray("/custom/data"));
    }
    void rejectsUnsafePackages() {
        QTemporaryDir dir; const auto target = dir.filePath("target"), source = dir.filePath("source"), bundle = dir.filePath("next.tar.gz");
        fixture(target, "old"); fixture(source, "new");
        int calls = 0; const auto check = [&](const QString&) { ++calls; return true; };
        for (const auto& path : {QString("../escape"), QString("usr/UserData/accounts.json"), QString("/tmp/absolute")}) {
            const auto item = makeTar(bundle, source, path);
            QVERIFY(!PortableUpdate::replace(target, bundle, item, "11.0.2-20261118", target + "/data", check).success);
        }
        auto item = makeTar(bundle, source, "escape-link", "/tmp");
        QVERIFY(!PortableUpdate::replace(target, bundle, item, "11.0.2-20261118", target + "/data", check).success);
        item = makeTar(bundle, source); item["sha256"] = QString(64, '0');
        QVERIFY(!PortableUpdate::replace(target, bundle, item, "11.0.2-20261118", target + "/data", check).success);
        QCOMPARE(calls, 0); QCOMPARE(read(target + "/AppRun"), QByteArray("old"));
        item = makeTar(bundle, source);
        auto oldMetadata = LinuxPortable::metadata(target); oldMetadata["neoUi"] = true;
        write(target + "/.luna-portable.json", QJsonDocument(oldMetadata).toJson());
        QVERIFY(!PortableUpdate::replace(target, bundle, item, "11.0.2-20261118", target + "/data", check).success);
        oldMetadata["neoUi"] = false; write(target + "/.luna-portable.json", QJsonDocument(oldMetadata).toJson());
        QLockFile lock(target + ".update.lock"); QVERIFY(lock.tryLock());
        QVERIFY(!PortableUpdate::replace(target, bundle, item, "11.0.2-20261118", target + "/data", check).success);
    }
    void realPackagedDirectory() {
        const auto bundle = qEnvironmentVariable("LUNA_TEST_PORTABLE_ARCHIVE");
        if (bundle.isEmpty()) QSKIP("Set LUNA_TEST_PORTABLE_ARCHIVE and LUNA_TEST_PORTABLE_MANIFEST for the real runtime test.");
        QFile manifestFile(qEnvironmentVariable("LUNA_TEST_PORTABLE_MANIFEST")); QVERIFY(manifestFile.open(QIODevice::ReadOnly));
        const auto manifest = QJsonDocument::fromJson(manifestFile.readAll()).object();
        const auto item = LinuxPortable::artifact(manifest, LinuxPortable::architecture(), false); QVERIFY(!item.isEmpty());
        QTemporaryDir dir; const auto target = dir.filePath(QString::fromUtf8("真实 便携包")); QVERIFY(QDir().mkpath(target));
        QString error;
        QVERIFY2(PortableUpdate::extract(bundle, target, item.value("unpackedSize").toInteger(), error, {}), qPrintable(error));
        QVERIFY(health(target));
        write(target + "/usr/UserData/instances/save.dat", "keep-save"); write(target + "/usr/.luna-data-dir", "/custom/data");
        const auto worker = dir.filePath("isolated-worker"); QVERIFY(QDir().mkpath(worker));
        QVERIFY2(PortableUpdate::copyProgram(target, worker, error), qPrintable(error));
        QVERIFY(QDir().rename(target, target + ".moved")); QVERIFY(health(worker)); QVERIFY(QDir().rename(target + ".moved", target));
        auto result = PortableUpdate::replace(target, bundle, item, manifest.value("version").toString(), target + "/usr/UserData",
            [&](const QString& root) { return health(root); });
        QVERIFY2(result.success, qPrintable(result.error)); QVERIFY(health(result.backup));
        QCOMPARE(read(target + "/usr/UserData/instances/save.dat"), QByteArray("keep-save"));
        QCOMPARE(read(target + "/usr/.luna-data-dir"), QByteArray("/custom/data"));
        int calls = 0;
        result = PortableUpdate::replace(target, bundle, item, manifest.value("version").toString(), target + "/usr/UserData",
            [&](const QString& root) { return health(root) && ++calls != 2; });
        QVERIFY(!result.success); QVERIFY(health(target));
        QCOMPARE(read(target + "/usr/UserData/instances/save.dat"), QByteArray("keep-save"));
        QCOMPARE(read(target + "/usr/.luna-data-dir"), QByteArray("/custom/data"));
    }
};
QTEST_GUILESS_MAIN(LinuxPortableUpdateTest)
#include "LinuxPortableUpdate_test.moc"
