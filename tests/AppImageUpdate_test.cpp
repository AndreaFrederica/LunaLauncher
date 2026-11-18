// SPDX-License-Identifier: GPL-3.0-only
#include <QtTest>
#include <QProcess>
#include "updater/prismupdater/AppImageUpdate.h"

class AppImageUpdateTest : public QObject {
    Q_OBJECT
    QByteArray image(char payload = 'a') {
        QByteArray data(64, payload);
        data.replace(0, 6, QByteArray("\177ELF\002\001", 6));
        data.replace(8, 3, QByteArray("AI\002", 3));
        data[18] = 62; data[19] = 0;
        return data;
    }
    void write(const QString& path, const QByteArray& data) {
        QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write(data), data.size());
        file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    }
    QByteArray read(const QString& path) { QFile file(path); if (!file.open(QIODevice::ReadOnly)) return {}; return file.readAll(); }
    QJsonObject item(const QByteArray& data) {
        return {{"platform", "linux"}, {"format", "appimage"}, {"architecture", "x64"}, {"neoUi", true},
            {"asset", "LunaLauncher-Linux-x64-11.0.2.AppImage"}, {"size", data.size()},
            {"sha256", QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex())}};
    }
    QJsonObject manifest(const QJsonObject& artifact) {
        return {{"schemaVersion", 1}, {"protocol", 1}, {"version", "11.0.2-20261118r1"}, {"artifacts", QJsonArray{artifact}}};
    }
private slots:
    void filters() {
        auto entry = item(image());
        QVERIFY(!AppImageUpdate::artifact(manifest(entry), "x64", true).isEmpty());
        QVERIFY(AppImageUpdate::artifact(manifest(entry), "arm64", false).isEmpty());
        entry["neoUi"] = false; QVERIFY(AppImageUpdate::artifact(manifest(entry), "x64", true).isEmpty());
        entry["neoUi"] = true;
        for (auto name : {"../bad.AppImage", "LunaLauncher-Linux-x64.AppImage.sha256", "other.AppImage", "LunaLauncher-Linux-x64.tar.gz"}) {
            auto bad = entry; bad["asset"] = name; QVERIFY(AppImageUpdate::artifact(manifest(bad), "x64", true).isEmpty());
        }
        auto bad = manifest(entry); bad["protocol"] = 2; QVERIFY(AppImageUpdate::artifact(bad, "x64", true).isEmpty());
        bad = manifest(entry); bad["artifacts"] = QJsonArray{entry, entry}; QVERIFY(AppImageUpdate::artifact(bad, "x64", true).isEmpty());
    }
    void replacementAndRollback() {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const auto target = dir.filePath(QString::fromUtf8("启动器 空格.AppImage"));
        const auto source = dir.filePath("new-image");
        const auto old = image('a'), next = image('b');
        write(target, old); write(source, next);
        int progress = 0;
        auto result = AppImageUpdate::replace(target, source, item(next), [](const QString&) { return true; },
            [&](qint64 done, qint64 total) { QCOMPARE(done, total); ++progress; });
        QVERIFY2(result.success, qPrintable(result.error)); QCOMPARE(read(target), next); QCOMPARE(read(result.backup), old); QCOMPARE(progress, 1);
        QFile::remove(result.backup);
        write(source, old);
        int calls = 0;
        result = AppImageUpdate::replace(target, source, item(old), [&](const QString&) { return ++calls == 1; });
        QVERIFY(!result.success); QCOMPARE(calls, 2); QCOMPARE(read(target), next);
        result = AppImageUpdate::replace(target, source, item(old), [](const QString&) { return false; });
        QVERIFY(!result.success); QCOMPARE(read(target), next);
    }
    void refusesUnsafeInputs() {
        QTemporaryDir dir;
        auto target = dir.filePath("old.AppImage"), source = dir.filePath("new.AppImage");
        const auto old = image('a'), next = image('b'); write(target, old); write(source, next);
        auto corrupted = item(next); corrupted["sha256"] = QString(64, '0');
        int calls = 0; const auto health = [&](const QString&) { ++calls; return true; };
        QVERIFY(!AppImageUpdate::replace(target, source, corrupted, health).success); QCOMPARE(calls, 0); QCOMPARE(read(target), old);
        auto wrongArch = item(next); wrongArch["architecture"] = "arm64";
        QVERIFY(!AppImageUpdate::replace(target, source, wrongArch, health).success);
        const auto link = dir.filePath("symlink"); QVERIFY(QFile::link(target, link));
        QVERIFY(!AppImageUpdate::replace(link, source, item(next), health).success);
        const auto hard = QFile::encodeName(dir.filePath("hardlink"));
        QCOMPARE(::link(QFile::encodeName(target).constData(), hard.constData()), 0);
        QVERIFY(!AppImageUpdate::replace(target, source, item(next), health).success); QFile::remove(QString::fromLocal8Bit(hard));
        QLockFile lock(target + ".update.lock"); QVERIFY(lock.tryLock());
        QVERIFY(!AppImageUpdate::replace(target, source, item(next), health).success); lock.unlock();
        QFile::setPermissions(target, QFile::ReadOwner | QFile::ExeOwner);
        QVERIFY(!AppImageUpdate::replace(target, source, item(next), health).success);
    }
    void changedDuringHealthCheck() {
        QTemporaryDir dir; auto target = dir.filePath("old.AppImage"), source = dir.filePath("new.AppImage");
        write(target, image('a')); write(source, image('b'));
        auto result = AppImageUpdate::replace(target, source, item(image('b')), [&](const QString&) {
            QFile::remove(target); write(target, image('c')); return true;
        });
        QVERIFY(!result.success); QCOMPARE(read(target), image('c'));
    }
    void realPackagedImage() {
        const auto source = qEnvironmentVariable("LUNA_TEST_APPIMAGE");
        if (source.isEmpty()) QSKIP("Set LUNA_TEST_APPIMAGE to run the real runtime replacement test.");
        QVERIFY(QFileInfo::exists(source));
        QTemporaryDir dir; const auto target = dir.filePath(QString::fromUtf8("便携 启动器.AppImage"));
        QVERIFY(QFile::copy(source, target));
        QDir().mkpath(target + ".home"); write(target + ".home/user-data.txt", "keep-me");
        QDir().mkpath(target + ".config"); write(target + ".config/settings.txt", "keep-settings");
        auto artifact = item({}); artifact["size"] = QFileInfo(source).size();
        QFile input(source); QVERIFY(input.open(QIODevice::ReadOnly));
        QCryptographicHash hash(QCryptographicHash::Sha256); QVERIFY(hash.addData(&input));
        artifact["sha256"] = QString::fromLatin1(hash.result().toHex());
        const auto health = [&](const QString& path) {
            QProcess process; auto env = AppImageUpdate::cleanEnvironment(); env.insert("QT_QPA_PLATFORM", "offscreen");
            process.setProcessEnvironment(env);
            process.start(path, {"--appimage-extract-and-run", "--luna-entry", "classic", "--version"});
            if (!process.waitForFinished(60000)) {
                qWarning() << "AppImage probe could not complete:" << process.errorString() << process.readAllStandardError();
                process.kill(); process.waitForFinished(); return false;
            }
            const auto output = process.readAllStandardOutput();
            if (process.exitCode() != 0 || !output.contains("LunaLauncher 11."))
                qWarning() << "AppImage probe failed:" << process.exitCode() << output << process.readAllStandardError();
            return process.exitCode() == 0 && output.contains("LunaLauncher 11.");
        };
        const auto result = AppImageUpdate::replace(target, source, artifact, health);
        QVERIFY2(result.success, qPrintable(result.error)); QVERIFY(health(result.backup));
        QCOMPARE(read(target + ".home/user-data.txt"), QByteArray("keep-me"));
        QCOMPARE(read(target + ".config/settings.txt"), QByteArray("keep-settings"));
    }
};
QTEST_GUILESS_MAIN(AppImageUpdateTest)
#include "AppImageUpdate_test.moc"
