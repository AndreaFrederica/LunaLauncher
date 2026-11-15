#include "VanillaInstanceLayout.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class VanillaInstanceLayoutTest : public QObject {
    Q_OBJECT

   private slots:
    void detectsSharedLayout();
    void detectsIsolatedLayout();
    void detectsDirectIsolatedVersionRoot();
    void blocksPrismComponentFiles();
};

void VanillaInstanceLayoutTest::detectsSharedLayout()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(QDir(root.path()).mkpath("versions/1.20.1"));
    QFile version(QDir(root.path()).filePath("versions/1.20.1/1.20.1.json"));
    QVERIFY(version.open(QIODevice::WriteOnly));
    version.write(R"({"id":"1.20.1"})");
    version.close();
    QVERIFY(QDir(root.path()).mkpath("mods"));

    const auto layout = detectVanillaInstanceLayout(root.path());
    QVERIFY(layout.isValid());
    QCOMPARE(layout.versionId, QStringLiteral("1.20.1"));
    QCOMPARE(layout.gameRoot, QDir(root.path()).absolutePath());
    QCOMPARE(layout.mode, VanillaGameLayout::SharedGameRoot);
    QVERIFY(!layout.isolated);
}

void VanillaInstanceLayoutTest::detectsIsolatedLayout()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(QDir(root.path()).mkpath("versions/1.20.1/game/config"));
    QFile version(QDir(root.path()).filePath("versions/1.20.1/1.20.1.json"));
    QVERIFY(version.open(QIODevice::WriteOnly));
    version.write(R"({"id":"1.20.1","gameDir":"versions/1.20.1/game"})");
    version.close();

    const auto layout = detectVanillaInstanceLayout(root.path(), "1.20.1");
    QCOMPARE(layout.mode, VanillaGameLayout::ExplicitGameDir);
    QVERIFY(layout.isolated);
    QCOMPARE(QDir::cleanPath(layout.gameRoot), QDir::cleanPath(QDir(root.path()).filePath("versions/1.20.1/game")));
}

void VanillaInstanceLayoutTest::detectsDirectIsolatedVersionRoot()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto instanceRoot = QDir(root.path()).filePath("versions/pack");
    QVERIFY(QDir().mkpath(instanceRoot));
    QFile version(QDir(instanceRoot).filePath("pack.json"));
    QVERIFY(version.open(QIODevice::WriteOnly));
    version.write(R"({"id":"pack","type":"release","mainClass":"net.minecraft.client.main.Main"})");
    version.close();
    const auto layout = detectVanillaInstanceLayout(instanceRoot);
    QVERIFY(layout.isValid());
    QCOMPARE(layout.versionId, QStringLiteral("pack"));
    QCOMPARE(layout.mode, VanillaGameLayout::IsolatedVersionRoot);
    QCOMPARE(QDir::cleanPath(layout.gameRoot), QDir::cleanPath(instanceRoot));
}

void VanillaInstanceLayoutTest::blocksPrismComponentFiles()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QFile mmc(QDir(root.path()).filePath("mmc-pack.json"));
    QVERIFY(mmc.open(QIODevice::WriteOnly));
    mmc.close();

    const auto report = checkVanillaExportCompatibility(root.path());
    QVERIFY(!report.compatible);
    QVERIFY(report.blockers.join('\n').contains("mmc-pack.json"));
}

QTEST_GUILESS_MAIN(VanillaInstanceLayoutTest)
#include "VanillaInstanceLayout_test.moc"

