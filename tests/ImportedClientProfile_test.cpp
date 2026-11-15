// SPDX-License-Identifier: GPL-3.0-only
#include "VanillaConversion.h"
#include "VanillaCompatibleProvider.h"
#include "VanillaInstance.h"
#include "VanillaConversionTask.h"
#include "minecraft/ImportedClientProfile.h"
#include "minecraft/OneSixVersionFormat.h"
#include "minecraft/VersionFile.h"
#include "minecraft/LaunchProfile.h"
#include "settings/JSONSettingsObject.h"

#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>
#include <QThread>
#include <atomic>

namespace {
bool put(const QString& path, const QByteArray& bytes)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) return false;
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}
QJsonObject read(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()).object() : QJsonObject{};
}
QJsonObject sourceProfile()
{
    return QJsonDocument::fromJson(R"({
      "id":"AnotherCore", "minecraftVersion":"1.20.1", "mainClass":"example.CustomMain",
      "minecraftArguments":"--gameDir ${game_directory}",
      "javaVersion":{"majorVersion":17,"component":"java-runtime-gamma"},
      "downloads":{"client":{"url":"https:\u002f\u002fexample.org/versions/1.20.1/client.jar","sha1":"0000000000000000000000000000000000000000","size":20}},
      "assetIndex":{"id":"custom","url":"https:\u002f\u002fexample.org/assets.json","sha1":"","size":0,"totalSize":0},
      "libraries":[]
    })").object();
}
bool seed(const QString& root)
{
    return put(QDir(root).filePath("versions/AnotherCore/AnotherCore.json"), QJsonDocument(sourceProfile()).toJson())
        && put(QDir(root).filePath("versions/AnotherCore/AnotherCore.jar"), "modified-client-fixture")
        && put(QDir(root).filePath("versions/AnotherCore/config/example.cfg"), "keep=true");
}
}

class ImportedClientProfileTest : public QObject
{
    Q_OBJECT
private slots:
    void retainsOrderedRepeatedJvmOptions()
    {
        QTemporaryDir temp;
        const auto source = temp.filePath("source");
        QVERIFY(seed(source));
        auto profile = sourceProfile();
        const QJsonArray expected{
            "--enable-native-access", "ALL-UNNAMED",
            "--add-opens", "java.base/java.io=ALL-UNNAMED",
            "--add-opens", "java.base/java.lang=ALL-UNNAMED",
            "--add-exports", "java.base/jdk.internal.misc=ALL-UNNAMED",
            "--add-exports", "java.base/jdk.internal.misc=ALL-UNNAMED"
        };
        auto sourceJvm = expected;
        sourceJvm.append("-cp"); sourceJvm.append("${classpath}");
        profile.insert("arguments", QJsonObject{{"jvm", sourceJvm}});
        QVERIFY(put(QDir(source).filePath("versions/AnotherCore/AnotherCore.json"), QJsonDocument(profile).toJson()));
        const auto target = temp.filePath("mmc");
        const auto report = VanillaConverter::importToPrism(QDir(source).filePath("versions/AnotherCore"), target);
        QVERIFY2(report.succeeded, qPrintable(report.error));
        const auto patch = read(QDir(target).filePath("patches/net.minecraft.json"));
        QCOMPARE(patch.value("+jvmArgs").toArray(), expected);
        // Verify the MMC parser and final launch profile retain the same order.
        const auto parsed = OneSixVersionFormat::versionFileFromJson(QJsonDocument(patch), "net.minecraft.json", false);
        QVERIFY(parsed);
        LaunchProfile launch;
        parsed->applyTo(&launch, RuntimeContext{});
        QStringList tokens;
        for (const auto& value : expected) tokens.append(value.toString());
        QCOMPARE(launch.getAddnJvmArguments(), tokens);
    }

    void copyingReportsRealProgressAndCancels_data()
    {
        QTest::addColumn<bool>("vanilla");
        QTest::newRow("MMC") << false;
        QTest::newRow("vanilla") << true;
    }

    void copyingReportsRealProgressAndCancels()
    {
        QFETCH(bool, vanilla);
        QTemporaryDir temp;
        const auto source = temp.filePath("source");
        QVERIFY(seed(source));
        QVERIFY(put(QDir(source).filePath("versions/AnotherCore/mods/large.jar"), QByteArray(4 * 1024 * 1024, 'x')));
        const auto target = temp.filePath(vanilla ? "store/versions/Imported" : "mmc");
        QVERIFY(QDir().mkpath(QFileInfo(target).absolutePath()));
        bool cancel = false, measured = false;
        VanillaImportOptions options;
        options.feedback.cancelled = [&] { return cancel; };
        options.feedback.progress = [&](const VanillaConversionProgress& progress) {
            if (progress.unit == "bytes" && progress.current > 0 && progress.current < progress.total) {
                measured = true;
                cancel = true;
            }
        };
        const auto version = QDir(source).filePath("versions/AnotherCore");
        const auto report = vanilla ? VanillaConverter::importToVanilla(version, target, options)
                                    : VanillaConverter::importToPrism(version, target, options);
        QVERIFY(measured);
        QVERIFY(!report.succeeded);
        QVERIFY(!report.error.isEmpty());
        QVERIFY(!QFileInfo::exists(target));
        QVERIFY(QDir(QFileInfo(target).absolutePath()).entryList({QFileInfo(target).fileName() + ".tmp-*"}, QDir::Dirs).isEmpty());
        QCOMPARE(QFileInfo(QDir(source).filePath("versions/AnotherCore/mods/large.jar")).size(), qint64(4 * 1024 * 1024));
    }

    void workerKeepsEventLoopResponsive()
    {
        auto* launcherThread = QThread::currentThread();
        std::atomic_bool observed = false, offThread = false;
        VanillaConversionTask task([&](const VanillaConversionFeedback& feedback) {
            offThread = QThread::currentThread() != launcherThread;
            feedback.update("copying", 1, 2, "files", "first.jar");
            // Completion depends on the main thread receiving real progress.
            QElapsedTimer deadline; deadline.start();
            while (!observed && !feedback.cancelled() && deadline.elapsed() < 3000) QThread::msleep(1);
            VanillaConversionReport report;
            report.succeeded = observed.load();
            return report;
        });
        connect(&task, &Task::progress, this, [&](qint64 current, qint64 total) {
            if (current == 1 && total == 2) observed = true;
        });
        QSignalSpy updates(&task, &Task::stepProgress);
        task.start();
        QTRY_VERIFY_WITH_TIMEOUT(task.isFinished(), 5000);
        QVERIFY(task.wasSuccessful());
        QVERIFY(offThread);
        QVERIFY(!updates.isEmpty());
    }

    void preservesUnknownModifiedClient()
    {
        QTemporaryDir temp;
        const auto source = temp.filePath("source");
        QVERIFY(seed(source));
        const auto target = temp.filePath("mmc");
        const auto report = VanillaConverter::importToPrism(QDir(source).filePath("versions/AnotherCore"),target);
        QVERIFY2(report.succeeded,qPrintable(report.error));
        QCOMPARE(report.clientArtifact.value("comparison").toString(),QString("differs-from-profile"));
        QCOMPARE(report.clientArtifact.value("sha256").toString(),QString::fromLatin1(QCryptographicHash::hash("modified-client-fixture",QCryptographicHash::Sha256).toHex()));
        const auto patch = read(QDir(target).filePath("patches/net.minecraft.json"));
        QCOMPARE(patch.value("version").toString(),QString("1.20.1"));
        QCOMPARE(patch.value("mainClass").toString(),QString("example.CustomMain"));
        QCOMPARE(patch.value("compatibleJavaMajors").toArray(),QJsonArray{17});
        const auto mainJar = patch.value("mainJar").toObject();
        QCOMPARE(mainJar.value("MMC-hint").toString(),QString("local"));
        QString error;
        QVERIFY(ImportedClientProfile::verifyClient(QDir(target).filePath("libraries/"+mainJar.value("MMC-filename").toString()),report.clientArtifact,&error));
        QCOMPARE(read(QDir(target).filePath("luna-imported/source-profile.json")),sourceProfile());
        QCOMPARE(read(QDir(source).filePath("versions/AnotherCore/AnotherCore.json")),sourceProfile());
    }

    void localClientNeverDownloadsReplacement()
    {
        QTemporaryDir temp;
        const auto jar = temp.filePath("core.jar");
        QVERIFY(put(jar,"preserved"));
        const auto record = ImportedClientProfile::inspectClient(jar,{});
        ProblemContainer problems;
        const auto library = OneSixVersionFormat::libraryFromJson(problems,ImportedClientProfile::localMainJar(record,"core.jar"),"test");
        RuntimeContext runtime;
        runtime.system = "windows"; runtime.javaArchitecture = "64";
        QStringList errors;
        QVERIFY(library->getDownloads(runtime,nullptr,errors,temp.path()).isEmpty());
        QVERIFY(errors.isEmpty());
        QVERIFY(put(jar,"tampered"));
        QVERIFY(library->getDownloads(runtime,nullptr,errors,temp.path()).isEmpty());
        QVERIFY(!errors.isEmpty());
        errors.clear();
        QVERIFY(QFile::remove(jar));
        QVERIFY(library->getDownloads(runtime,nullptr,errors,temp.path()).isEmpty());
        QVERIFY(!errors.isEmpty());
    }

    void rawImportRenamesAndProtectsClient()
    {
        QTemporaryDir temp;
        const auto source = temp.filePath("source");
        QVERIFY(seed(source));
        const auto target = temp.filePath("store/.minecraft/versions/Renamed");
        QVERIFY(QDir().mkpath(QFileInfo(target).absolutePath()));
        const auto report = VanillaConverter::importToVanilla(QDir(source).filePath("versions/AnotherCore"),target);
        QVERIFY2(report.succeeded,qPrintable(report.error));
        const auto raw = read(QDir(target).filePath("Renamed.json"));
        QCOMPARE(raw.value("id").toString(),QString("Renamed"));
        QCOMPARE(raw.value("jar").toString(),QString("Renamed"));
        QVERIFY(!raw.value("downloads").toObject().contains("client"));
        for (const auto& name : {"mmc-pack.json","instance.cfg","patches"}) QVERIFY(!QFileInfo(QDir(target).filePath(name)).exists());
        const auto descriptors = VanillaCompatibleProvider::scan("test",temp.filePath("store"));
        QCOMPARE(descriptors.size(),1);
        QVERIFY(descriptors.first().layout.isolated);
        auto global = std::make_unique<JSONSettingsObject>(temp.filePath("global.json"));
        for (const auto& key : { "ShowGameTime", "RecordGameTime", "PreLaunchCommand", "WrapperCommand", "PostExitCommand",
                                 "ShowConsole", "AutoCloseConsole", "ShowConsoleOnError", "LogPrePostOutput", "ConsoleMaxLines", "ConsoleOverflowStop" })
            global->registerSetting(key, false);
        auto settings = std::make_unique<JSONSettingsObject>(QDir(target).filePath("luna.settings.json"));
        VanillaInstance instance(global.get(),std::move(settings),descriptors.first());
        QString error;
        QVERIFY2(instance.validateClientArtifact(&error),qPrintable(error));
        QVERIFY(put(QDir(target).filePath("Renamed.jar"),"tampered"));
        QVERIFY(!instance.validateClientArtifact(&error));
        QVERIFY(!error.isEmpty());
        QVERIFY(QFile::remove(QDir(target).filePath("Renamed.jar")));
        QVERIFY(!instance.validateClientArtifact(&error));
    }

    void inheritedProfilePrefersOwnPatchedClient()
    {
        QTemporaryDir temp;
        const auto root = temp.path();
        QVERIFY(put(temp.filePath("versions/base/base.json"),R"({"id":"1.20.1","mainClass":"net.minecraft.client.main.Main"})"));
        QVERIFY(put(temp.filePath("versions/base/base.jar"),"official"));
        QVERIFY(put(temp.filePath("versions/custom/custom.json"),R"({"id":"custom","inheritsFrom":"base"})"));
        QVERIFY(put(temp.filePath("versions/custom/custom.jar"),"patched"));
        const auto layout = detectVanillaInstanceLayout(root,"custom");
        QCOMPARE(layout.clientJar,temp.filePath("versions/custom/custom.jar"));
        const auto direct = detectVanillaInstanceLayout(temp.filePath("versions/custom"));
        QCOMPARE(direct.clientJar,layout.clientJar);
    }

    void detectsGtnhBaseAndPreservesPlatformArguments()
    {
        const auto profile = QJsonDocument::fromJson(R"({"id":"GTNH-custom",
          "libraries":[{"name":"net.minecraftforge:forge:1.7.10-10.13.4.1614-1.7.10:universal"}],
          "javaVersion":{"majorVersion":25},"mainClass":"org.jackhuang.hmcl.HMCLMultiMCBootstrap",
          "arguments":{"game":["--example","value with spaces"],"jvm":["-cp","${classpath}","-Djava.system.class.loader=example.RfbLoader",
            {"rules":[{"action":"allow","os":{"name":"osx"}}],"value":"-XstartOnFirstThread"}]}})").object();
        QCOMPARE(VanillaCompatibleProvider::detectedMinecraftVersion(profile),QString("1.7.10"));
        const auto patch = VanillaCompatibleProvider::prismRuntimePatch(profile,"net.minecraft","1.7.10",false);
        const auto jvm = patch.value("+jvmArgs").toArray();
        QVERIFY(!jvm.contains("-cp"));
        QVERIFY(!jvm.contains("${classpath}"));
        QVERIFY(jvm.contains("-Djava.system.class.loader=example.RfbLoader"));
#ifndef Q_OS_MACOS
        QVERIFY(!jvm.contains("-XstartOnFirstThread"));
#endif
        QVERIFY(patch.value("minecraftArguments").toString().contains("\"value with spaces\""));
        QCOMPARE(patch.value("compatibleJavaMajors").toArray(),QJsonArray{25});
    }

    void legacyRuntimeKeepsJava8Default()
    {
        QJsonObject profile{{"mainClass","net.minecraft.launchwrapper.Launch"}};
        auto patch = VanillaCompatibleProvider::prismRuntimePatch(profile,"net.minecraft","1.12.2",false);
        QCOMPARE(patch.value("compatibleJavaMajors").toArray(),QJsonArray{8});
        profile.insert("javaVersion",QJsonObject{{"majorVersion",17}});
        patch = VanillaCompatibleProvider::prismRuntimePatch(profile,"net.minecraft","1.6.4",false);
        QCOMPARE(patch.value("compatibleJavaMajors").toArray(),QJsonArray{17});
    }

    void rejectsBrokenInheritanceAndParentCollision()
    {
        QTemporaryDir temp;
        const auto source = temp.filePath("source");
        QVERIFY(seed(source));
        auto profile = sourceProfile();
        profile.insert("inheritsFrom","missing");
        const auto file = QDir(source).filePath("versions/AnotherCore/AnotherCore.json");
        QVERIFY(put(file,QJsonDocument(profile).toJson()));
        for (bool vanilla : {false,true}) {
            const auto target = temp.filePath(vanilla ? "raw/versions/child" : "mmc");
            const auto report = vanilla ? VanillaConverter::importToVanilla(QDir(source).filePath("versions/AnotherCore"),target)
                                        : VanillaConverter::importToPrism(QDir(source).filePath("versions/AnotherCore"),target);
            QVERIFY(!report.succeeded);
            QVERIFY(!report.error.isEmpty());
            QVERIFY(!QFileInfo::exists(target));
        }
        QVERIFY(put(QDir(source).filePath("versions/missing/missing.json"),R"({"id":"1.20.1","inheritsFrom":"AnotherCore"})"));
        auto report = VanillaConverter::importToPrism(QDir(source).filePath("versions/AnotherCore"),temp.filePath("cycle"));
        QVERIFY(!report.succeeded);
        QVERIFY(report.error.contains("cycle"));
        QVERIFY(put(QDir(source).filePath("versions/missing/missing.json"),R"({"id":"1.20.1"})"));
        report = VanillaConverter::importToVanilla(QDir(source).filePath("versions/AnotherCore"),temp.filePath("raw/versions/missing"));
        QVERIFY(!report.succeeded);
        QVERIFY(report.error.contains("conflicts"));
    }
};
QTEST_GUILESS_MAIN(ImportedClientProfileTest)
#include "ImportedClientProfile_test.moc"
