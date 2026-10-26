// SPDX-License-Identifier: GPL-3.0-only
#include "ui/themes/ThemePackage.h"
#include <QFile>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QPainter>
#include <QStyleOptionButton>
#include "ui/themes/ThemeStyle.h"
#include "ui/themes/ThemeInstallTask.h"
#include "ui/themes/ThemeStore.h"
#include "Application.h"
#include "cli/McpServer.h"
#include "settings/SettingsObject.h"

class ThemeFixtureReply : public QNetworkReply {
    QByteArray m_bytes;
    qint64 m_position = 0;
   public:
    ThemeFixtureReply(const QNetworkRequest& request, QByteArray bytes, QObject* parent)
        : QNetworkReply(parent), m_bytes(std::move(bytes))
    {
        setRequest(request);
        setUrl(request.url());
        setOperation(QNetworkAccessManager::GetOperation);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
        setHeader(QNetworkRequest::ContentLengthHeader, m_bytes.size());
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        QTimer::singleShot(0, this, [this] {
            if (isFinished())
                return;
            emit readyRead();
            setFinished(true);
            emit finished();
        });
    }
    qint64 bytesAvailable() const override { return m_bytes.size() - m_position + QNetworkReply::bytesAvailable(); }
    void abort() override
    {
        if (isFinished())
            return;
        setError(OperationCanceledError, "Cancelled");
        setFinished(true);
        emit errorOccurred(OperationCanceledError);
        emit finished();
    }
   protected:
    qint64 readData(char* data, qint64 size) override
    {
        const auto count = qMin(size, m_bytes.size() - m_position);
        if (!count)
            return -1;
        memcpy(data, m_bytes.constData() + m_position, count);
        m_position += count;
        return count;
    }
};
class ThemeFixtureNetwork : public QNetworkAccessManager {
   public:
    using QNetworkAccessManager::QNetworkAccessManager;
    bool multipleFiles = false;
    QStringList requests;
   protected:
    QNetworkReply* createRequest(Operation, const QNetworkRequest& request, QIODevice*) override
    {
        const auto path = request.url().path();
        requests.append(path);
        QByteArray data;
        if (path.contains("content/data/")) {
            data = "<content><id>123</id><name>Fixture</name><downloadname1>Fixture.colors</downloadname1><downloadway1>1</downloadway1><downloadprice1>0</downloadprice1>";
            if (multipleFiles)
                data += "<downloadname2>FixtureLight.colors</downloadname2><downloadway2>1</downloadway2><downloadprice2>0</downloadprice2>";
            data += "</content>";
        } else if (path.contains("content/download/")) {
            data = "<content><downloadway>1</downloadway><downloadlink>https://themes.example/fixture.colors</downloadlink></content>";
        } else {
            return new ThemeFixtureReply(request, "[General]\nName=Fixture\n[Colors:Window]\nBackgroundNormal=30,31,32\n[Colors:View]\nBackgroundNormal=20,21,22\n", this);
        }
        return new ThemeFixtureReply(request, "<ocs><meta><statuscode>100</statuscode></meta><data>" + data + "</data></ocs>", this);
    }
};

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
    void kvantumNativeControls()
    {
        QTemporaryDir dir;
        const auto configuration = dir.filePath("Fixture.kvconfig");
        const QByteArray svg = R"(<svg xmlns="http://www.w3.org/2000/svg" width="100" height="100"><rect id="button-normal" width="40" height="20" fill="#12ab34"/><rect id="button-pressed" y="30" width="40" height="20" fill="#1234ab"/></svg>)";
        put(configuration, "[GeneralColors]\nwindow.color=#152637\n[PanelButtonCommand]\nframe=false\ninterior=true\ninterior.element=button\ntext.margin.left=24\ntext.margin.right=24\n");
        put(dir.filePath("Fixture.svg"), svg);
        QString error;
        auto entries = ThemePackage::inspect(QDir(dir.path()), error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries.first().toObject().value("format").toString(), QString("kvantum"));
        if (!ThemeStyle::engines().contains("Kvantum"))
            QSKIP("The bundled Kvantum renderer requires Qt 6.6 or later.");
        std::unique_ptr<QStyle> style(ThemeStyle::create("Kvantum", configuration));
        QCOMPARE(style->standardPalette().color(QPalette::Window), QColor("#152637"));
        auto render = [&](QStyle::State state) {
            QImage image(160, 40, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            QStyleOptionButton option;
            option.rect = image.rect();
            option.state = QStyle::State_Enabled | state;
            option.palette = style->standardPalette();
            style->drawControl(QStyle::CE_PushButtonBevel, &option, &painter);
            return image;
        };
        const auto normal = render(QStyle::State_Raised);
        const auto pressed = render(QStyle::State_Sunken);
        QCOMPARE(normal.pixelColor(80, 20), QColor("#12ab34"));
        QCOMPARE(pressed.pixelColor(80, 20), QColor("#1234ab"));
        put(dir.filePath("Fixture.svg"), "<svg xmlns='http://www.w3.org/2000/svg'><image href='../outside.png'/></svg>");
        QVERIFY(!ThemePackage::validateKvantum(configuration, error));
        QVERIFY(error.contains("external"));
    }
    void kdeGlobalThemeReferences()
    {
        QTemporaryDir dir;
        QVERIFY(QDir(dir.path()).mkpath("Fixture/contents"));
        put(dir.filePath("Fixture/metadata.json"), QJsonDocument(QJsonObject{
            { "KPlugin", QJsonObject{ { "Name", "Complete Fixture" } } },
            { "X-KPackage-Dependencies", QJsonArray{ "kns://icons.knsrc/api.kde-look.org/123",
                "kns://kvantum.knsrc/api.opendesktop.org/456", "kns://plasma-themes.knsrc/api.kde-look.org/789",
                "https://example.com/install.sh" } } }).toJson());
        put(dir.filePath("Fixture/contents/defaults"), "[kdeglobals][KDE]\nwidgetStyle=kvantum\n[kdeglobals][General]\nColorScheme=FixtureDark\n[kdeglobals][Icons]\nTheme=FixtureIcons\n");
        QString error;
        const auto data = ThemePackage::readGlobalTheme(dir.filePath("Fixture/contents/defaults"), error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(data.value("name").toString(), QString("Complete Fixture"));
        QCOMPARE(data.value("references").toObject().value("widgets").toString(), QString("kvantum"));
        QCOMPARE(data.value("references").toObject().value("icons").toString(), QString("FixtureIcons"));
        const auto deps = data.value("dependencies").toArray();
        QCOMPARE(deps.size(), 4);
        QVERIFY(deps.at(0).toObject().value("supported").toBool());
        QCOMPARE(deps.at(1).toObject().value("category").toString(), QString("widgets"));
        QVERIFY(!deps.at(2).toObject().value("supported").toBool());
        QVERIFY(!deps.at(3).toObject().value("supported").toBool());
        const auto entries = ThemePackage::inspect(QDir(dir.path()), error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries.first().toObject().value("kind").toString(), QString("bundle"));
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
    void declaredDependencyInstallation()
    {
        QTemporaryDir dir;
        QVERIFY(QDir(dir.path()).mkpath("source/global/contents"));
        put(dir.filePath("source/global/contents/defaults"), "[kdeglobals][General]\nColorScheme=Fixture\n");
        put(dir.filePath("source/global/metadata.json"), QJsonDocument(QJsonObject{
            { "KPlugin", QJsonObject{ { "Name", "Dependency fixture" } } },
            { "X-KPackage-Dependencies", QJsonArray{ "kns://colorschemes.knsrc/api.kde-look.org/123" } }
        }).toJson());
        // Like the application's network manager, this must outlive deferred
        // deletion of NetJob and its replies after the installer finishes.
        auto network = new ThemeFixtureNetwork(qApp);
        QJsonObject request{ { "path", dir.filePath("source") } };
        ThemeInstallTask task(network, request, dir.filePath("packages"), dir.filePath("cache"));
        QSignalSpy done(&task, &Task::finished);
        task.start();
        QVERIFY(done.wait(10000));
        QVERIFY2(task.wasSuccessful(), qPrintable(task.failReason()));
        QCOMPARE(task.result().value("dependencyPackages").toArray().size(), 1);
        QVERIFY(task.result().value("unresolvedDependencies").toArray().isEmpty());
        QVERIFY(network->requests.contains("/ocs/v1/content/download/123/1"));
        QVERIFY(network->requests.contains("/fixture.colors"));
        network->multipleFiles = true;
        // A different cache directory forces a fresh file inventory.
        ThemeInstallTask ambiguous(network, request, dir.filePath("packages"), dir.filePath("cache-ambiguous"));
        QSignalSpy ambiguousDone(&ambiguous, &Task::finished);
        ambiguous.start();
        QVERIFY(ambiguousDone.wait(10000));
        QVERIFY(ambiguous.wasSuccessful());
        QVERIFY(ambiguous.result().value("dependencyPackages").toArray().isEmpty());
        QCOMPARE(ambiguous.result().value("unresolvedDependencies").toArray().first().toObject().value("files").toArray().size(), 2);
        request.insert("dependencyFiles", QJsonObject{ { "123", "2" } });
        ThemeInstallTask selected(network, request, dir.filePath("packages"), dir.filePath("cache-ambiguous"));
        QSignalSpy selectedDone(&selected, &Task::finished);
        selected.start();
        QVERIFY(selectedDone.wait(10000));
        QVERIFY2(selected.wasSuccessful(), qPrintable(selected.failReason()));
        QCOMPARE(selected.result().value("dependencyPackages").toArray().size(), 1);
        QVERIFY(network->requests.contains("/ocs/v1/content/download/123/2"));
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
int main(int argc, char** argv)
{
    const auto original = QDir::currentPath();
    QTemporaryDir profile;
    int result = 2;
    {
        QByteArrayList arguments{ QByteArray(argv[0]), "--dir", profile.path().toUtf8(), "--mcp" };
        QList<char*> pointers;
        for (auto& value : arguments)
            pointers.append(value.data());
        pointers.append(nullptr);
        int count = arguments.size();
        Application app(count, pointers.data());
        if (app.status() != Application::Initialized)
            return 2;
        for (auto server : app.findChildren<McpServer*>())
            delete server;
        // The fixture network must be used instead of an external downloader.
        app.settings()->set("DownloadBackend", 0);
        ThemePackageTest test;
        result = QTest::qExec(&test, argc, argv);
    }
    QDir::setCurrent(original);
    return result;
}
#include "ThemePackage_test.moc"
