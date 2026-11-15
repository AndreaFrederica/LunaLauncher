// SPDX-License-Identifier: GPL-3.0-only
#include "VanillaConversion.h"
#include "VanillaCompatibleProvider.h"
#include "VanillaInstance.h"
#include "FileSystem.h"
#include "minecraft/VersionFile.h"
#include "settings/JSONSettingsObject.h"

#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class VanillaInstallTest : public QObject {
    Q_OBJECT
private slots:
    void installsIntoVanillaStore_data();
    void installsIntoVanillaStore();
    void rejectsMissingMetadata();
    void refusesUnsupportedComponents();
    void refusesConflictingSharedArtifacts();
    void keepsRuntimeGraphOutsideInstance();
    void installsPlainVanillaWithMatchingJar();
    void exportsForgeInstallerAndArguments();
};

namespace {
bool put(const QString& path, const QByteArray& data)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) return false;
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}

QJsonObject read(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()).object() : QJsonObject{};
}

VanillaExportOptions fixture(const QString& root)
{
    VanillaExportOptions options;
    options.versionId = "My Pack";
    options.name = "My Pack";
    options.isolated = true;
    options.librariesRoot = QDir(root).filePath("cache/libraries");
    options.assetsRoot = QDir(root).filePath("cache/assets");
    const auto base = QJsonDocument::fromJson(R"({
        "uid":"net.minecraft", "version":"1.20.1", "formatVersion":1,
        "type":"release", "mainClass":"net.minecraft.client.main.Main",
        "minecraftArguments":"--username ${auth_player_name} --gameDir ${game_directory}",
        "mainJar":{"name":"com.mojang:minecraft:1.20.1:client", "downloads":{"artifact":{"url":"https://example.org/client.jar", "sha1":"0123456789012345678901234567890123456789", "size":18}}},
        "assetIndex":{"id":"test-assets", "url":"https://example.org/assets.json", "sha1":"0123456789012345678901234567890123456789", "size":100, "totalSize":100},
        "compatibleJavaMajors":[17], "compatibleJavaName":"java-runtime-gamma",
        "+traits":["FirstThreadOnMacOS"],
        "libraries":[{"name":"example:library:1", "MMC-absoluteUrl":"https://example.org/library.jar"}]
    })").object();
    const auto loader = QJsonDocument::fromJson(R"({
        "uid":"net.fabricmc.fabric-loader", "version":"0.16.10", "formatVersion":1,
        "mainClass":"net.fabricmc.loader.impl.launch.knot.KnotClient",
        "+jvmArgs":["-Dloader.test=true"],
        "arguments":{"game":["--loader-argument","value"], "jvm":["-Dstructured=true"]},
        "libraries":[{"name":"net.fabricmc:fabric-loader:0.16.10", "url":"https://maven.fabricmc.net/"}],
        "requires":[{"uid":"net.fabricmc.intermediary"}]
    })").object();
    options.componentProfiles = { base, loader, QJsonObject{ { "uid", "net.fabricmc.intermediary" }, { "version", "1.20.1" } } };
    return options;
}

bool seed(const QString& root)
{
    return put(QDir(root).filePath("prism/mmc-pack.json"), R"({"components":[{"uid":"net.minecraft","version":"1.20.1"},{"uid":"net.fabricmc.fabric-loader","version":"0.16.10"}]})")
        && put(QDir(root).filePath("prism/instance.cfg"), "[General]\nInstanceType=OneSix\n")
        && put(QDir(root).filePath("prism/patches/net.minecraft.json"), QJsonDocument(fixture(root).componentProfiles.first().toObject()).toJson())
        && put(QDir(root).filePath("prism/.minecraft/config/test.cfg"), "setting=true")
        && put(QDir(root).filePath("cache/libraries/com/mojang/minecraft/1.20.1/minecraft-1.20.1-client.jar"), "client-jar-fixture")
        && put(QDir(root).filePath("cache/libraries/example/library/1/library-1.jar"), "library-fixture")
        && put(QDir(root).filePath("cache/assets/indexes/test-assets.json"), R"({"objects":{"test":{"hash":"0123456789012345678901234567890123456789"}}})")
        && put(QDir(root).filePath("cache/assets/objects/01/0123456789012345678901234567890123456789"), "asset-fixture");
}
} // namespace


void VanillaInstallTest::installsIntoVanillaStore_data()
{
    QTest::addColumn<QString>("kind");
    QTest::newRow("empty-parent") << "empty";
    QTest::newRow("existing-dot-minecraft") << "existing";
    QTest::newRow("direct-dot-minecraft") << "direct";
    QTest::newRow("existing-game-root") << "game-root";
}

void VanillaInstallTest::installsIntoVanillaStore()
{
    QFETCH(QString, kind);
    QTemporaryDir temp;
    QVERIFY(seed(temp.path()));
    auto store = QDir(temp.path()).filePath("store");
    auto expected = QDir(store).filePath(".minecraft");
    if (kind == "existing") QVERIFY(QDir().mkpath(expected));
    if (kind == "direct") store = expected;
    if (kind == "game-root") {
        QVERIFY(QDir().mkpath(QDir(store).filePath("versions")));
        expected = store;
    }
    const auto gameRoot = VanillaConverter::gameRootForStore(store);
    QCOMPARE(gameRoot, expected);
    auto options = fixture(temp.path());
    const auto versions = QDir(gameRoot).filePath("versions");
    const auto target = QDir(versions).filePath(options.versionId);
    const auto report = VanillaConverter::exportToVanilla(QDir(temp.path()).filePath("prism"), target, options);
    QVERIFY2(report.succeeded, qPrintable(report.error));
    const auto profile = read(QDir(target).filePath(options.versionId + ".json"));
    QCOMPARE(profile.value("id").toString(), options.versionId);
    QCOMPARE(profile.value("jar").toString(), QStringLiteral("1.20.1"));
    QCOMPARE(profile.value("mainClass").toString(), QStringLiteral("net.fabricmc.loader.impl.launch.knot.KnotClient"));
    QVERIFY(profile.value("downloads").toObject().value("client").toObject().contains("url"));
    QCOMPARE(profile.value("javaVersion").toObject().value("majorVersion").toInt(), 17);
    const auto args = profile.value("arguments").toObject();
    QVERIFY(args.value("game").toArray().contains("--username"));
    QVERIFY(args.value("game").toArray().contains("--loader-argument"));
    QVERIFY(args.value("jvm").toArray().contains("-Dloader.test=true"));
    QVERIFY(args.value("jvm").toArray().contains("-Dstructured=true"));
    QVERIFY(args.value("jvm").toArray().contains("${classpath}"));
    const auto serialized = QJsonDocument(profile).toJson();
    for (const auto& forbidden : { "MMC-", "mainJar", "formatVersion", "requires", "+jvmArgs", "+traits", "minecraftArguments" })
        QVERIFY2(!serialized.contains(forbidden), forbidden);
    QVERIFY(QFileInfo::exists(QDir(gameRoot).filePath("versions/1.20.1/1.20.1.jar")));
    QVERIFY(QFileInfo::exists(QDir(gameRoot).filePath("libraries/example/library/1/library-1.jar")));
    QVERIFY(QFileInfo::exists(QDir(gameRoot).filePath("assets/objects/01/0123456789012345678901234567890123456789")));
    QVERIFY(QFileInfo::exists(QDir(target).filePath("config/test.cfg")));
    for (const auto& forbidden : { "mmc-pack.json", "instance.cfg", "patches", "versions", ".minecraft" })
        QVERIFY2(!QFileInfo::exists(QDir(target).filePath(forbidden)), forbidden);
    // A second installation of the same release remains an independent
    // instance, and shares only ordinary vanilla cache files.
    options.versionId = FS::DirNameFromString(options.name, versions);
    QVERIFY(options.versionId != "My Pack");
    const auto second = VanillaConverter::exportToVanilla(QDir(temp.path()).filePath("prism"), QDir(versions).filePath(options.versionId), options);
    QVERIFY2(second.succeeded, qPrintable(second.error));
    const auto scanned = VanillaCompatibleProvider::scan("test", store);
    QCOMPARE(scanned.size(), 3); // Two installed profiles plus their shared base.
    for (const auto& instance : scanned) {
        QCOMPARE(instance.minecraftVersion, QStringLiteral("1.20.1"));
        if (instance.versionId == "1.20.1") { QCOMPARE(instance.loaderType, QStringLiteral("vanilla")); continue; }
        QCOMPARE(instance.loaderType, QStringLiteral("fabric"));
        QCOMPARE(instance.loaderVersion, QStringLiteral("0.16.10"));
    }
    const auto duplicate = VanillaConverter::exportToVanilla(QDir(temp.path()).filePath("prism"), target, options);
    QVERIFY(!duplicate.succeeded);
    QCOMPARE(read(QDir(target).filePath("My Pack.json")), profile);
}

void VanillaInstallTest::rejectsMissingMetadata()
{
    QTemporaryDir temp;
    QVERIFY(seed(temp.path()));
    auto options = fixture(temp.path());
    options.componentProfiles = {};
    options.versionJson = QJsonObject{ { "id", "1.20.1" }, { "type", "release" } };
    const auto target = QDir(temp.path()).filePath("versions/My Pack");
    const auto report = VanillaConverter::exportToVanilla(QDir(temp.path()).filePath("prism"), target, options);
    QVERIFY(!report.succeeded);
    QVERIFY(report.error.contains("net.fabricmc.fabric-loader"));
    QVERIFY(!QFileInfo::exists(target));
    options.componentProfiles = { QJsonObject{ { "uid", "net.minecraft" }, { "version", "1.20.1" } } };
    QVERIFY(!VanillaConverter::exportToVanilla(QDir(temp.path()).filePath("prism"), target, options).succeeded);
    QVERIFY(!QFileInfo::exists(target));
    options = fixture(temp.path());
    options.componentProfiles.removeLast();
    const auto missingDependency = VanillaConverter::exportToVanilla(QDir(temp.path()).filePath("prism"), target, options);
    QVERIFY(!missingDependency.succeeded);
    QVERIFY(missingDependency.error.contains("net.fabricmc.intermediary"));
}

void VanillaInstallTest::installsPlainVanillaWithMatchingJar()
{
    QTemporaryDir temp;
    QVERIFY(seed(temp.path()));
    const auto prism = QDir(temp.path()).filePath("prism");
    QVERIFY(put(QDir(prism).filePath("mmc-pack.json"), R"({"components":[{"uid":"net.minecraft","version":"1.20.1"}]})"));
    auto options = fixture(temp.path());
    options.versionId = "1.20.1";
    options.componentProfiles = { options.componentProfiles.first() };
    const auto target = QDir(temp.path()).filePath(".minecraft/versions/1.20.1");
    const auto report = VanillaConverter::exportToVanilla(prism, target, options);
    QVERIFY2(report.succeeded, qPrintable(report.error));
    QVERIFY(QFileInfo::exists(QDir(target).filePath("1.20.1.jar")));
    const auto profile = read(QDir(target).filePath("1.20.1.json"));
    QCOMPARE(profile.value("id").toString(), QStringLiteral("1.20.1"));
    QCOMPARE(profile.value("mainClass").toString(), QStringLiteral("net.minecraft.client.main.Main"));
    QCOMPARE(VanillaCompatibleProvider::detectedLoaderType(profile), QStringLiteral("vanilla"));
    QVERIFY(!QFileInfo::exists(QDir(target).filePath("versions")));
}

void VanillaInstallTest::exportsForgeInstallerAndArguments()
{
    QTemporaryDir temp;
    QVERIFY(seed(temp.path()));
    const auto prism = QDir(temp.path()).filePath("prism");
    QVERIFY(put(QDir(prism).filePath("mmc-pack.json"), R"({"components":[{"uid":"net.minecraft","version":"1.20.1"},{"uid":"net.minecraftforge","version":"47.2.0"}]})"));
    auto options = fixture(temp.path());
    const auto forge = QJsonDocument::fromJson(R"({
        "uid":"net.minecraftforge", "version":"47.2.0", "mainClass":"io.github.zekerzhayard.forgewrapper.installer.Main",
        "minecraftArguments":"--username ${auth_player_name} --launchTarget forgeclient --fml.mcVersion 1.20.1 --fml.forgeVersion 47.2.0",
        "mavenFiles":[{"name":"net.minecraftforge:forge:1.20.1-47.2.0:installer", "url":"https://maven.minecraftforge.net/"}]
    })").object();
    options.componentProfiles = { options.componentProfiles.first(), forge };
    const auto installer = QStringLiteral("net/minecraftforge/forge/1.20.1-47.2.0/forge-1.20.1-47.2.0-installer.jar");
    QVERIFY(put(QDir(options.librariesRoot).filePath(installer), "installer-fixture"));
    const auto root = QDir(temp.path()).filePath(".minecraft");
    const auto target = QDir(root).filePath("versions/My Pack");
    const auto report = VanillaConverter::exportToVanilla(prism, target, options);
    QVERIFY2(report.succeeded, qPrintable(report.error));
    const auto profile = read(QDir(target).filePath("My Pack.json"));
    const auto args = profile.value("arguments").toObject();
    QCOMPARE(args.value("game").toArray().count(), 8);
    QVERIFY(args.value("jvm").toArray().contains("-Dforgewrapper.installer=${library_directory}/" + installer));
    QVERIFY(args.value("jvm").toArray().contains("-Dforgewrapper.minecraft=${library_directory}/../versions/1.20.1/1.20.1.jar"));
    QVERIFY(!profile.contains("mavenFiles"));
    QVERIFY(QFileInfo::exists(QDir(root).filePath("libraries/" + installer)));
    const auto base = read(QDir(root).filePath("versions/1.20.1/1.20.1.json"));
    QCOMPARE(base.value("mainClass").toString(), QStringLiteral("net.minecraft.client.main.Main"));
    QVERIFY(base.value("arguments").toObject().value("jvm").toArray().contains("${classpath}"));
    auto scanned = VanillaCompatibleProvider::scan("test", root);
    QCOMPARE(scanned.size(), 2);
    scanned.removeIf([](const auto& item) { return item.versionId == "1.20.1"; });
    QCOMPARE(scanned.first().loaderType, QStringLiteral("forge"));
    QCOMPARE(scanned.first().loaderVersion, QStringLiteral("47.2.0"));
}

void VanillaInstallTest::refusesUnsupportedComponents()
{
    QTemporaryDir temp;
    QVERIFY(seed(temp.path()));
    auto options = fixture(temp.path());
    auto loader = options.componentProfiles.at(1).toObject();
    loader.insert("jarMods", QJsonArray{ QJsonObject{ { "name", "test:jar-mod:1" } } });
    options.componentProfiles[1] = loader;
    const auto target = QDir(temp.path()).filePath("versions/My Pack");
    const auto report = VanillaConverter::exportToVanilla(QDir(temp.path()).filePath("prism"), target, options);
    QVERIFY(!report.succeeded);
    QVERIFY(report.error.contains("jarMods"));
    QVERIFY(!QFileInfo::exists(target));
}

void VanillaInstallTest::refusesConflictingSharedArtifacts()
{
    QTemporaryDir temp;
    QVERIFY(seed(temp.path()));
    const auto root = QDir(temp.path()).filePath(".minecraft");
    const auto jar = QDir(root).filePath("versions/1.20.1/1.20.1.jar");
    QVERIFY(put(jar, "existing-different-jar"));
    const auto target = QDir(root).filePath("versions/My Pack");
    const auto report = VanillaConverter::exportToVanilla(QDir(temp.path()).filePath("prism"), target, fixture(temp.path()));
    QVERIFY(!report.succeeded);
    QVERIFY(!QFileInfo::exists(target));
    QFile file(jar); QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), QByteArray("existing-different-jar"));
}

void VanillaInstallTest::keepsRuntimeGraphOutsideInstance()
{
    QTemporaryDir temp;
    QVERIFY(seed(temp.path()));
    const auto root = QDir(temp.path()).filePath(".minecraft");
    const auto target = QDir(root).filePath("versions/My Pack");
    const auto report = VanillaConverter::exportToVanilla(QDir(temp.path()).filePath("prism"), target, fixture(temp.path()));
    QVERIFY2(report.succeeded, qPrintable(report.error));
    auto scanned = VanillaCompatibleProvider::scan("test", root);
    QCOMPARE(scanned.size(), 2);
    scanned.removeIf([](const auto& item) { return item.versionId == "1.20.1"; });
    JSONSettingsObject global(QDir(temp.path()).filePath("global.json"));
    for (const auto& key : { "ShowGameTime", "RecordGameTime", "PreLaunchCommand", "WrapperCommand", "PostExitCommand",
                             "ShowConsole", "AutoCloseConsole", "ShowConsoleOnError", "LogPrePostOutput", "ConsoleMaxLines", "ConsoleOverflowStop" })
        global.registerSetting(key, false);
    const auto profileBefore = read(QDir(target).filePath("My Pack.json"));
    {
        auto settings = std::make_unique<JSONSettingsObject>(QDir(target).filePath("luna.settings.json"));
        VanillaInstance instance(&global, std::move(settings), scanned.first());
        const auto componentRoot = instance.componentRoot();
        QVERIFY(!componentRoot.isEmpty());
        QVERIFY(!componentRoot.startsWith(target));
        QVERIFY(QFileInfo::exists(QDir(componentRoot).filePath("mmc-pack.json")));
        QVERIFY(instance.canLaunch());
        const auto base = instance.loadForeignComponent("net.minecraft", "1.20.1");
        QVERIFY(base);
        QVERIFY(base->mainJar);
        instance.saveNow();
    }
    QCOMPARE(read(QDir(target).filePath("My Pack.json")), profileBefore);
    QVERIFY(!QFileInfo::exists(QDir(target).filePath("mmc-pack.json")));
    QVERIFY(!QFileInfo::exists(QDir(target).filePath("patches")));
    QVERIFY(!QFileInfo::exists(QDir(target).filePath("instance.cfg")));
}

QTEST_GUILESS_MAIN(VanillaInstallTest)
#include "VanillaInstall_test.moc"
