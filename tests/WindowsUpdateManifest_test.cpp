#include <QTest>
#include <QTemporaryFile>
#include "updater/prismupdater/WindowsUpdateManifest.h"
#include "Version.h"

class WindowsUpdateManifestTest : public QObject {
    Q_OBJECT
private slots:
    void rejectsTruncatedAndTamperedPackages()
    {
        QTemporaryFile package; QVERIFY(package.open()); package.write("installer"); package.flush();
        QJsonObject item{{"size", 9}, {"sha256", QString(QCryptographicHash::hash("installer", QCryptographicHash::Sha256).toHex())}};
        QVERIFY(WindowsUpdate::verifyPackage(package.fileName(), item));
        package.resize(2); package.flush();
        QVERIFY(!WindowsUpdate::verifyPackage(package.fileName(), item));
        package.resize(0); package.write("tampered!"); package.flush();
        QVERIFY(!WindowsUpdate::verifyPackage(package.fileName(), item));
    }
    void rejectsWrongProtocolArchitectureAndChecksumAttachments()
    {
        QJsonObject package{{"platform", "windows"}, {"architecture", "x64"}, {"neoUi", true},
            {"asset", "LunaLauncher-Windows-x64-11.0.2-Setup.exe"}, {"size", 123}, {"sha256", QString(64, 'a')}};
        QJsonObject manifest{{"schemaVersion", 1}, {"protocol", 1}, {"version", "11.0.2-20261118r1"}, {"artifacts", QJsonArray{package}}};
        QVERIFY(!WindowsUpdate::artifact(manifest, "x64", true).isEmpty());
        QVERIFY(WindowsUpdate::artifact(manifest, "arm64", true).isEmpty());
        auto bad = manifest; bad["protocol"] = 2;
        QVERIFY(WindowsUpdate::artifact(bad, "x64", true).isEmpty());
        for (auto name : {"LunaLauncher-Windows-x64-Setup.exe.sha256", "../LunaLauncher-Windows-x64-Setup.exe", "Linux.tar.gz"}) {
            auto item = package; item["asset"] = name; bad = manifest; bad["artifacts"] = QJsonArray{item};
            QVERIFY(WindowsUpdate::artifact(bad, "x64", true).isEmpty());
        }
        auto item = package; item["neoUi"] = false; bad = manifest; bad["artifacts"] = QJsonArray{item};
        QVERIFY(WindowsUpdate::artifact(bad, "x64", true).isEmpty());
        QVERIFY(!WindowsUpdate::artifact(bad, "x64", false).isEmpty());
        bad = manifest; bad["artifacts"] = QJsonArray{package, package};
        QVERIFY(WindowsUpdate::artifact(bad, "x64", false).isEmpty());
    }
    void comparesDateAndSameDayRevision()
    {
        const auto version = [](const QString& text) { return Version(WindowsUpdate::normalizedVersion(text)); };
        QVERIFY(version("v11.0.2-20261118") > version("11.0.2-20261117"));
        QVERIFY(version("11.0.2-20261118r2") > version("v11.0.2-20261118r1"));
        QCOMPARE(version("v11.0.2-20261118"), version("11.0.2-20261118"));
    }
};
QTEST_GUILESS_MAIN(WindowsUpdateManifestTest)
#include "WindowsUpdateManifest_test.moc"
