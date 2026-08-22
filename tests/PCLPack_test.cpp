// SPDX-License-Identifier: GPL-3.0-only

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "modplatform/pcl/PCLPack.h"

class PCLPackTest : public QObject {
    Q_OBJECT

   private slots:
    void parseSetup()
    {
        const auto setup = PCL::parseSetup("VersionServerEnter:example.org:25565\r\n"
                                           "# comment\r\n"
                                           "VersionArgumentIndieV2:True\r\n"
                                           "MalformedLine\r\n");
        QCOMPARE(setup.values.value("VersionServerEnter"), QString("example.org:25565"));
        QCOMPARE(setup.values.value("VersionArgumentIndieV2"), QString("True"));
        QCOMPARE(setup.values.size(), 2);
        QCOMPARE(setup.sourceSha256.size(), 32);
    }

    void customMemoryMapping_data()
    {
        QTest::addColumn<int>("slider");
        QTest::addColumn<int>("mebibytes");
        QTest::newRow("minimum") << 0 << 307;
        QTest::newRow("first-segment-end") << 12 << 1536;
        QTest::newRow("second-segment-start") << 13 << 2048;
        QTest::newRow("second-segment-end") << 25 << 8192;
        QTest::newRow("third-segment-end") << 33 << 16384;
        QTest::newRow("fourth-segment-start") << 34 << 18432;
        QTest::newRow("maximum") << 49 << 49152;
    }

    void customMemoryMapping()
    {
        QFETCH(int, slider);
        QFETCH(int, mebibytes);
        QCOMPARE(PCL::customRamMegabytes(slider), mebibytes);
    }

    void parseInstanceConfig()
    {
        const auto config = PCL::parseInstanceConfig(R"({
            "InstanceMigratedJava": true,
            "InstanceForcedJava": {
                "Folder": "D:\\mc\\java21\\bin\\",
                "Version": { "Major": 21, "Minor": 0, "Build": 6, "Revision": 0 }
            }
        })");
        QVERIFY(config.valid);
        QVERIFY(config.migratedJava);
        QCOMPARE(config.javaFolder, QString("D:\\mc\\java21\\bin\\"));
        QCOMPARE(config.javaMajor, 21);
        QCOMPARE(config.javaBuild, 6);
        QCOMPARE(config.sourceSha256.size(), 32);
    }

    void nestedCandidates()
    {
        const QStringList files{ "Plain Craft Launcher.exe", "modpack.mrpack", "PCL/Setup.ini", "wrapper/modpack.zip",
                                 "too/deep/modpack.mrpack", "unrelated.zip" };
        QCOMPARE(PCL::findNestedPackCandidates(files), QStringList({ "modpack.mrpack", "wrapper/modpack.zip" }));
    }

    void javaAgents()
    {
        QCOMPARE(PCL::javaAgentPaths("-Xmx2G -javaagent:GraphicsFixer.jar -javaagent:\"agents/test agent.jar\"=mode"),
                 QStringList({ "GraphicsFixer.jar", "agents/test agent.jar" }));

        QTemporaryDir gameRoot;
        QVERIFY(gameRoot.isValid());
        QFile existing(gameRoot.filePath("included-agent.jar"));
        QVERIFY(existing.open(QIODevice::WriteOnly));
        existing.close();

        QCOMPARE(PCL::unavailableJavaAgentFiles("-javaagent:included-agent.jar -javaagent:GraphicsFixer.jar", gameRoot.path()),
                 QStringList({ "GraphicsFixer.jar" }));
        QCOMPARE(PCL::unavailableJavaAgentFiles("-javaagent:../external-agent.jar", gameRoot.path()),
                 QStringList({ "../external-agent.jar" }));
    }

    void pclBuiltinIcons()
    {
        QCOMPARE(PCL::pclBuiltinIconCandidate(
                     "pack://application:,,,/Plain Craft Launcher 2;component/Images/Blocks/Anvil.png"),
                 QString("anvil"));
        QCOMPARE(PCL::pclBuiltinIconCandidate(
                     "pack://application:,,,/Plain Craft Launcher 2;component/Images/Blocks/Egg.png"),
                 QString("egg"));
        for (const auto& name : { "Fabric", "CobbleStone", "CraftingTable", "Workbench" }) {
            QVERIFY2(PCL::pclBuiltinIconCandidate(
                         QString("pack://application:,,,/Plain Craft Launcher 2;component/Images/Blocks/%1.png").arg(name))
                         .isEmpty(),
                     name);
        }
        QVERIFY(PCL::pclBuiltinIconCandidate("PCL/Logo.png").isEmpty());
    }
};

QTEST_GUILESS_MAIN(PCLPackTest)

#include "PCLPack_test.moc"
