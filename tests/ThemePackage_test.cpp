// SPDX-License-Identifier: GPL-3.0-only
#include "ui/themes/ThemePackage.h"
#include <QFile>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include "ui/themes/ThemeInstallTask.h"
#include "ui/themes/ThemeStore.h"

class ThemePackageTest : public QObject {
    Q_OBJECT
    static void put(const QString& path, const QByteArray& data)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(data), data.size());
    }
    static QByteArray colors()
    {
        return "[General]\nName=Fixture\n[Colors:Window]\nBackgroundNormal=30,31,32\nForegroundNormal=240,241,242\n[Colors:View]"
               "\nBackgroundNormal=20,21,22\nForegroundNormal=230,231,232\n[Colors:Selection]\nBackgroundNormal=10,120,"
               "230\nBackgroundInactive=60,61,62\nForegroundNormal=255,255,255\n";
    }
   private slots:
    void kdePalette()
    {
        QTemporaryDir dir;
        const auto path = dir.filePath("fixture.colors");
        put(path, colors());
        ThemePackage::Palette palette;
        QString error;
        QVERIFY2(ThemePackage::readColors(path, palette, error), qPrintable(error));
        QCOMPARE(palette.name, QString("Fixture"));
        QVERIFY(palette.dark);
        QCOMPARE(palette.colors.color(QPalette::Active, QPalette::Window), QColor(30, 31, 32));
        QCOMPARE(palette.colors.color(QPalette::Inactive, QPalette::Highlight), QColor(60, 61, 62));
        QVERIFY(palette.colors.color(QPalette::Disabled, QPalette::Text) != palette.colors.color(QPalette::Active, QPalette::Text));
        put(path, colors().replace("30,31,32", "300,0,0"));
        error.clear();
        QVERIFY(!ThemePackage::readColors(path, palette, error));
        QVERIFY(!error.isEmpty());
    }
    void stylesheetPaths()
    {
        QTemporaryDir dir;
        QDir root(dir.path());
        root.mkpath("resources");
        put(dir.filePath("resources/icon.svg"), "<svg/>");
        QString error;
        auto text = ThemePackage::scopedStyleSheet("/*url(https://ignored)*/QWidget {image:url(theme:icon.svg);}", root, "fixture", error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(text.contains("fixture:resources/icon.svg"));
        for (const auto& value : { "url(https://example.com/a)", "url(../outside)", "url(C:/outside)", "url('resources/\\icon.svg')",
                                   "@import 'other.qss';" }) {
            error.clear();
            QVERIFY(!ThemePackage::validateStyleSheet(value, root, error));
        }
        for (const auto& value : { "../x", "a/../../x", "C:/x", "resources/CON.svg", "resources/x.", "resources/x ", "resources/a\\b" }) {
            error.clear();
            QVERIFY(ThemePackage::containedPath(root, value, error).isEmpty());
        }
    }
    void manifestComponents()
    {
        QTemporaryDir dir;
        put(dir.filePath("fixture.colors"), colors());
        put(dir.filePath("skin.qss"), "QPushButton { padding: 5px; }");
        put(dir.filePath("caption.json"), "{\"height\":36,\"boldTitle\":true}");
        put(dir.filePath("theme.json"),
            R"({"schemaVersion":2,"name":"Fixture","components":{"widgets":{"qss":"skin.qss","engine":"Fusion","paletteBinding":"semantic"},"palettes":{"dark":{"format":"kde-colors","file":"fixture.colors"}},"decoration":{"renderer":"luna-widgets","config":"caption.json"}}})");
        QString error;
        const auto entries = ThemePackage::inspect(QDir(dir.path()), error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(entries.size(), 3);
        put(dir.filePath("caption.json"), "{\"script\":\"run.exe\"}");
        error.clear();
        QVERIFY(ThemePackage::inspect(QDir(dir.path()), error).isEmpty());
    }
    void bundleResourceRoots()
    {
        QTemporaryDir dir;
        QVERIFY(QDir(dir.path()).mkpath("icons/fixture"));
        QVERIFY(QDir(dir.path()).mkpath("background"));
        put(dir.filePath("icons/fixture/index.theme"), "[Icon Theme]\nName=Fixture\nDirectories=\n");
        put(dir.filePath("background/catpack.json"), R"({"name":"Fixture","default":"image.svg"})");
        put(dir.filePath("background/image.svg"), "<svg/>");
        put(dir.filePath("theme.json"),
            R"({"schemaVersion":2,"name":"Fixture","components":{"icons":{"directory":"icons/fixture"},"background":{"manifest":"background/catpack.json"}}})");
        QString error;
        const auto entries = ThemePackage::inspect(QDir(dir.path()), error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(entries.size(), 2);
        QCOMPARE(entries[0].toObject().value("root").toString(), QString("icons/fixture"));
        QCOMPARE(entries[1].toObject().value("root").toString(), QString("background"));
    }
    void ocsBusinessStatusAndPaging()
    {
        QString error;
        auto result =
            ThemeStoreTask::parseOcs("<ocs><meta><statuscode>101</statuscode><message>Denied</message></meta><data/></ocs>", {}, error);
        QVERIFY(result.isEmpty());
        QVERIFY(error.contains("101"));
        error.clear();
        result = ThemeStoreTask::parseOcs(
            "<ocs><meta><statuscode>100</statuscode><page>2</page><pagesize>10</pagesize><totalitems>32</totalitems></"
            "meta><data><content><id>123</id><name>Theme</name><description>&lt;b&gt;Plain&lt;/b&gt;</"
            "description><downloadname1>one.colors</downloadname1><downloadway1>1</downloadway1><downloadprice1>0</"
            "downloadprice1><downloadlink1>https://example.com/signed-secret</downloadlink1></content></data></ocs>",
            { { "mode", "files" }, { "category", "palette" } }, error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(result.value("page").toInt(), 2);
        QCOMPARE(result.value("pageSize").toInt(), 10);
        QVERIFY(result.value("hasNext").toBool());
        QCOMPARE(result.value("files").toArray().size(), 1);
        QVERIFY(!QJsonDocument(result).toJson().contains("signed-secret"));
        QCOMPARE(result.value("items").toArray().first().toObject().value("description").toString(), QString("Plain"));
        error.clear();
        QVERIFY(ThemeStoreTask::parseOcs("<!DOCTYPE ocs [<!ENTITY x SYSTEM 'file:///test'>]><ocs/>", {}, error).isEmpty());
    }
    void githubSourcesAndAssets()
    {
        QCOMPARE(ThemeStoreTask::githubRepository("https://github.com/PrismLauncher/Themes.git"), QString("PrismLauncher/Themes"));
        QVERIFY(ThemeStoreTask::githubRepository("https://evil.example/Prism/Themes").isEmpty());
        QVERIFY(ThemeStoreTask::githubRepository("https://user:password@github.com/a/b").isEmpty());
        QString error;
        const auto result = ThemeStoreTask::parseGitHub(
            R"({"tag_name":"v1","assets":[{"id":12,"name":"a-theme.zip","browser_download_url":"https://github.com/a/b/releases/a-theme.zip","size":10},{"id":13,"name":"installer.exe","browser_download_url":"https://example.com/x"}]})",
            { { "contentId", "12" } }, error);
        QVERIFY(error.isEmpty());
        QCOMPARE(result.value("items").toArray().size(), 1);
        QCOMPARE(result.value("files").toArray().size(), 1);
    }
    void immutableImportAndCancellation()
    {
        QTemporaryDir dir;
        const auto source = dir.filePath("source.colors");
        put(source, colors());
        QNetworkAccessManager network;
        ThemeInstallTask task(&network, { { "path", source } }, dir.filePath("packages"), dir.filePath("cache"));
        QSignalSpy done(&task, &Task::finished);
        task.start();
        QVERIFY(done.wait(10000));
        QVERIFY2(task.wasSuccessful(), qPrintable(task.failReason()));
        const auto id = task.result().value("packageId").toString();
        QCOMPARE(id.size(), 64);
        QVERIFY(QFile::exists(dir.filePath("packages/" + id + "/.luna-package.json")));
        ThemeInstallTask duplicate(&network, { { "path", source } }, dir.filePath("packages"), dir.filePath("cache"));
        QSignalSpy duplicateDone(&duplicate, &Task::finished);
        duplicate.start();
        QVERIFY(duplicateDone.wait(10000));
        QVERIFY(duplicate.wasSuccessful());
        QVERIFY(duplicate.result().value("alreadyInstalled").toBool());
        ThemeInstallTask cancelled(&network, { { "path", source } }, dir.filePath("packages"), dir.filePath("cache"));
        QSignalSpy cancelDone(&cancelled, &Task::finished);
        cancelled.start();
        QVERIFY(cancelled.abort());
        QTRY_VERIFY_WITH_TIMEOUT(cancelled.isFinished(), 10000);
        QCOMPARE(cancelDone.size(), 1);
        QCOMPARE(cancelled.getState(), Task::State::AbortedByUser);
    }
};
QTEST_MAIN(ThemePackageTest)
#include "ThemePackage_test.moc"
