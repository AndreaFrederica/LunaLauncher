#include "SyncProfile.h"
#include "VanillaCompatibleProvider.h"
#include "VanillaConversion.h"
#include "settings/JSONSettingsObject.h"

#include <QFile>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class VanillaProviderSyncTest : public QObject {
    Q_OBJECT
   private slots:
    void scansAndRoundTripsSidecar();
    void scansVersionIsolatedInstance();
    void detectsModernForgeProfile();
    void installsLoaderIntoDirectIsolatedRoot();
    void rejectsConflictingSyncPlan();
    void usesBaseHashForOneSidedChanges();
    void blocksMmcOnlyExport();
    void mergesIndependentConfigChanges();
    void scansRealExternalFixtureWhenProvided();
    void writesAndReloadsLunaSettingsJson();
};

void VanillaProviderSyncTest::scansAndRoundTripsSidecar()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto root = QDir(temp.path()).filePath(".minecraft");
    QVERIFY(QDir().mkpath(QDir(root).filePath("versions/1.20.1")));
    QFile version(QDir(root).filePath("versions/1.20.1/1.20.1.json"));
    QVERIFY(version.open(QIODevice::WriteOnly));
    version.write(R"({"id":"1.20.1","type":"release"})");
    version.close();
    auto list = VanillaCompatibleProvider::scan("hmcl", root);
    QCOMPARE(list.size(), 1);
    QCOMPARE(list.first().dialect, QStringLiteral("mojang"));
    list.first().name = "我的实例";
    QVERIFY(VanillaCompatibleProvider::saveSidecar(list.first()));
    auto loaded = VanillaCompatibleProvider::scan("hmcl", root);
    QCOMPARE(loaded.first().name, QStringLiteral("我的实例"));
}

void VanillaProviderSyncTest::scansVersionIsolatedInstance()
{
    QTemporaryDir temp;
    const auto root = QDir(temp.path()).filePath(".minecraft");
    const auto instanceRoot = QDir(root).filePath("versions/isolated");
    QVERIFY(QDir().mkpath(QDir(instanceRoot).filePath("mods")));
    QFile version(QDir(instanceRoot).filePath("isolated.json"));
    QVERIFY(version.open(QIODevice::WriteOnly));
    version.write(R"({"id":"isolated","type":"release"})");
    version.close();
    const auto list = VanillaCompatibleProvider::scan(QStringLiteral("hmcl"), root);
    QCOMPARE(list.size(), 1);
    QCOMPARE(QDir::cleanPath(list.first().root), QDir::cleanPath(instanceRoot));
    QCOMPARE(list.first().layout.mode, VanillaGameLayout::IsolatedVersionRoot);
}

void VanillaProviderSyncTest::detectsModernForgeProfile()
{
    QTemporaryDir temp;
    const auto root = QDir(temp.path()).filePath(".minecraft");
    const auto instanceRoot = QDir(root).filePath("versions/1.18.2");
    QVERIFY(QDir().mkpath(instanceRoot));
    QFile version(QDir(instanceRoot).filePath("1.18.2.json"));
    QVERIFY(version.open(QIODevice::WriteOnly));
    version.write(R"({"id":"1.18.2","jar":"1.18.2","mainClass":"cpw.mods.bootstraplauncher.BootstrapLauncher","arguments":{"game":["--launchTarget","forgeclient","--fml.forgeVersion","40.2.9","--fml.mcVersion","1.18.2"],"jvm":[]},"libraries":[{"name":"net.minecraftforge:fmlloader:1.18.2-40.2.9"}]})");
    version.close();
    const auto list = VanillaCompatibleProvider::scan(QStringLiteral("hmcl"), root);
    QCOMPARE(list.size(), 1);
    QCOMPARE(list.first().loaderType, QStringLiteral("forge"));
    QCOMPARE(list.first().loaderVersion, QStringLiteral("40.2.9"));
    QCOMPARE(list.first().minecraftVersion, QStringLiteral("1.18.2"));
}

void VanillaProviderSyncTest::installsLoaderIntoDirectIsolatedRoot()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto root = QDir(temp.path()).filePath(".minecraft");
    const auto instanceRoot = QDir(root).filePath("versions/forge-pack");
    QVERIFY(QDir().mkpath(instanceRoot));
    QFile version(QDir(instanceRoot).filePath("forge-pack.json"));
    QVERIFY(version.open(QIODevice::WriteOnly));
    version.write(R"({"id":"forge-pack","inheritsFrom":"1.20.1","type":"release","libraries":[]})");
    version.close();

    VanillaLoaderInstallOptions options;
    options.versionId = QStringLiteral("forge-pack");
    options.loaderId = QStringLiteral("net.minecraftforge");
    options.loaderVersion = QStringLiteral("47.2.0");
    const auto report = VanillaConverter::installLoader(instanceRoot, options);
    QVERIFY2(report.succeeded, qPrintable(report.error));
    QVERIFY2(QFileInfo::exists(QDir(instanceRoot).filePath("forge-pack-minecraftforge-47.2.0.json")), qPrintable(QDir(instanceRoot).entryList(QDir::Files).join(',')));
    QVERIFY(!QFileInfo::exists(QDir(instanceRoot).filePath("versions/forge-pack-minecraftforge-47.2.0/forge-pack-minecraftforge-47.2.0.json")));
}

void VanillaProviderSyncTest::blocksMmcOnlyExport()
{
    QTemporaryDir temp;
    const auto prism = QDir(temp.path()).filePath("prism");
    QVERIFY(QDir().mkpath(prism));
    QFile pack(QDir(prism).filePath("mmc-pack.json")); QVERIFY(pack.open(QIODevice::WriteOnly)); pack.write("{}"); pack.close();
    VanillaExportOptions options;
    options.versionId = QStringLiteral("1.20.1");
    options.copyGameContent = true;
    const auto report = VanillaConverter::exportToVanilla(prism, QDir(temp.path()).filePath("vanilla"), options);
    QVERIFY(!report.succeeded);
    QVERIFY(!report.blockers.isEmpty());
}

void VanillaProviderSyncTest::rejectsConflictingSyncPlan()
{
    QTemporaryDir temp;
    const auto client = QDir(temp.path()).filePath("client");
    const auto server = QDir(temp.path()).filePath("server");
    QVERIFY(QDir().mkpath(QDir(client).filePath("config")));
    QVERIFY(QDir().mkpath(QDir(server).filePath("config")));
    QFile left(QDir(client).filePath("config/example.cfg")); QVERIFY(left.open(QIODevice::WriteOnly)); left.write("client"); left.close();
    QFile right(QDir(server).filePath("config/example.cfg")); QVERIFY(right.open(QIODevice::WriteOnly)); right.write("server"); right.close();
    SyncProfile profile; profile.id = "test"; profile.clientInstanceId = "c"; profile.serverInstanceId = "s"; profile.commonConfigs = { "config/example.cfg" };
    const auto plan = SyncEngine::plan(profile, client, server);
    QVERIFY(!plan.valid);
    QVERIFY(plan.conflicts.contains("config/example.cfg"));
}

void VanillaProviderSyncTest::usesBaseHashForOneSidedChanges()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto client = QDir(temp.path()).filePath("client");
    const auto server = QDir(temp.path()).filePath("server");
    QVERIFY(QDir().mkpath(QDir(client).filePath("config")));
    QVERIFY(QDir().mkpath(QDir(server).filePath("config")));
    const auto relative = QStringLiteral("config/example.cfg");
    QFile left(QDir(client).filePath(relative)); QVERIFY(left.open(QIODevice::WriteOnly)); left.write("changed-client"); left.close();
    QFile right(QDir(server).filePath(relative)); QVERIFY(right.open(QIODevice::WriteOnly)); right.write("base"); right.close();
    SyncProfile profile; profile.id = "test-base"; profile.clientInstanceId = "c"; profile.serverInstanceId = "s"; profile.commonConfigs = { relative };
    profile.baseHashes.insert(relative, QString::fromLatin1(QCryptographicHash::hash("base", QCryptographicHash::Sha256).toHex()));
    const auto plan = SyncEngine::plan(profile, client, server);
    QVERIFY(plan.valid);
    QCOMPARE(plan.conflicts.size(), 0);
    QCOMPARE(plan.changes.size(), 1);
    QCOMPARE(plan.changes.first().direction, QStringLiteral("client->server"));
}

void VanillaProviderSyncTest::mergesIndependentConfigChanges()
{
    QTemporaryDir temp;
    const auto client = QDir(temp.path()).filePath("client");
    const auto server = QDir(temp.path()).filePath("server");
    QDir().mkpath(QDir(client).filePath("config"));
    QDir().mkpath(QDir(server).filePath("config"));
    const auto path = QStringLiteral("config/example.cfg");
    QFile base(QDir(client).filePath(path)); QVERIFY(base.open(QIODevice::WriteOnly)); base.write("a=1\nb=1\n"); base.close();
    QFile other(QDir(server).filePath(path)); QVERIFY(other.open(QIODevice::WriteOnly)); other.write("a=1\nb=1\n"); other.close();
    SyncProfile profile; profile.id = "merge"; profile.clientInstanceId = "c"; profile.serverInstanceId = "s"; profile.commonConfigs = { path };
    profile.baseHashes.insert(path, QString::fromLatin1(QCryptographicHash::hash("a=1\nb=1\n", QCryptographicHash::Sha256).toHex()));
    profile.baseContents.insert(path, QString::fromLatin1(QByteArray("a=1\nb=1\n").toBase64()));
    QFile clientChanged(QDir(client).filePath(path)); QVERIFY(clientChanged.open(QIODevice::WriteOnly)); clientChanged.write("a=2\nb=1\n"); clientChanged.close();
    QFile serverChanged(QDir(server).filePath(path)); QVERIFY(serverChanged.open(QIODevice::WriteOnly)); serverChanged.write("a=1\nb=2\n"); serverChanged.close();
    profile.conflictPolicy = QStringLiteral("three-way");
    const auto plan = SyncEngine::plan(profile, client, server);
    QVERIFY(plan.valid);
    QCOMPARE(plan.conflicts.size(), 0);
    QCOMPARE(plan.changes.size(), 1);
    QVERIFY(plan.changes.first().content.contains("a=2"));
    QVERIFY(plan.changes.first().content.contains("b=2"));
    QVERIFY(SyncEngine::apply(profile, plan, client, server));
}

void VanillaProviderSyncTest::scansRealExternalFixtureWhenProvided()
{
    const auto root = qEnvironmentVariable("LUNA_HMCL_TEST_ROOT");
    if (root.isEmpty()) QSKIP("LUNA_HMCL_TEST_ROOT is not set");
    const auto instances = VanillaCompatibleProvider::scan(QStringLiteral("hmcl-fixture"), root);
    QVERIFY2(!instances.isEmpty(), qPrintable(root));
    QVERIFY(instances.first().layout.isValid());
    if (root.contains(QStringLiteral("PCL"), Qt::CaseInsensitive))
        QCOMPARE(instances.first().dialect, QStringLiteral("pcl"));
}

void VanillaProviderSyncTest::writesAndReloadsLunaSettingsJson()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto jsonPath = QDir(temp.path()).filePath("luna.settings.json");
    JSONSettingsObject settings(jsonPath);
    settings.registerSetting(QStringLiteral("MaxMemAlloc"), 1024);
    QVERIFY(settings.set(QStringLiteral("MaxMemAlloc"), 6144));
    QVERIFY(QFileInfo::exists(jsonPath));

    QFile json(jsonPath);
    QVERIFY(json.open(QIODevice::ReadOnly));
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(json.readAll(), &error);
    QCOMPARE(error.error, QJsonParseError::NoError);
    QCOMPARE(document.object().value(QStringLiteral("formatVersion")).toInt(), 1);
    QCOMPARE(document.object().value(QStringLiteral("settings")).toObject().value(QStringLiteral("MaxMemAlloc")).toInt(), 6144);

    JSONSettingsObject reloaded(jsonPath);
    reloaded.registerSetting(QStringLiteral("MaxMemAlloc"), 1024);
    QCOMPARE(reloaded.get(QStringLiteral("MaxMemAlloc")).toInt(), 6144);
}

QTEST_GUILESS_MAIN(VanillaProviderSyncTest)
#include "VanillaProviderSync_test.moc"

