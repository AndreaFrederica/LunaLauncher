#include <QTest>
#include <QDebug>
#include <memory>
#include "BuildConfig.h"
#include "minecraft/Library.h"
#include "minecraft/MirrorDownload.h"
#include "modplatform/ModApiMirror.h"
#include "settings/SettingsObject.h"
#include "settings/Setting.h"
#include "net/HttpMetaCache.h"
#include "RuntimeContext.h"

// Mock SettingsObject to inject into Library::getDownloads
class MockSettingsObject : public SettingsObject {
public:
    MockSettingsObject() : SettingsObject(nullptr) {}
    
    // Implement pure virtual methods
    void suspendSave() override {}
    void resumeSave() override {}
    
protected:
    void changeSetting(const Setting& setting, QVariant value) override {
        m_values[setting.id()] = value;
    }
    
    void resetSetting(const Setting& setting) override {
        m_values.remove(setting.id());
    }
    
    QVariant retrieveValue(const Setting& setting) override {
        if (m_values.contains(setting.id())) {
            return m_values.value(setting.id());
        }
        return setting.defValue();
    }

private:
    QMap<QString, QVariant> m_values;
};

class BMCLAPITest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        // Initialize settings mock
        m_settings = std::make_shared<MockSettingsObject>();
        
        // Register necessary settings
        m_settings->registerSetting("DownloadMirrorType", 0);
        m_settings->registerSetting("MojangDownloadsMirrorURL", "");
        m_settings->registerSetting("LibrariesURL", "");
        m_settings->registerSetting("ResourceURLOverride", "");
        m_settings->registerSetting("LegacyFMLLibsURLOverride", "");
        m_settings->registerSetting("ModrinthMirror", ModApiMirror::Official);
        m_settings->registerSetting("CurseForgeMirror", ModApiMirror::Official);
        m_settings->registerSetting("ModrinthMirrorURL", "");
        m_settings->registerSetting("CurseForgeMirrorURL", "");
        
        // Initialize cache
        m_cache = std::make_unique<HttpMetaCache>("test_metacache.json");
        m_cache->addBase("libraries", "test_libraries");
    }

    void testBMCLAPIRewrite() {
        // Set mirror type to BMCLAPI (1)
        m_settings->set("DownloadMirrorType", 1); // MirrorDownload::MirrorType::BMCLAPI
        
        // Create a library that uses Mojang URL via setRepositoryURL
        // Standard libraries usually have a gradle specifier and no explicit repository URL (implies Maven Central or Mojang)
        // But Library::getDownloads uses default repository URL if empty.
        
        auto lib = std::make_shared<Library>();
        lib->setRawName(GradleSpecifier("com.example:test:1.0"));
        // Library defaults: repositoryURL empty.
        // getDownloads logic: if repositoryURL empty -> check LibrariesURL -> else BuildConfig.LIBRARY_BASE
        
        // We want to test explicit Mojang URL rewrite, e.g. "https://libraries.minecraft.net"
        // This usually happens if the library has explicit url.
        lib->setRepositoryURL("https://libraries.minecraft.net/");
        
        RuntimeContext context;
        QStringList failed;
        auto dls = lib->getDownloads(context, m_cache.get(), failed, "", m_settings);
        
        QVERIFY(!dls.isEmpty());
        auto url = dls.first()->url();
        // Verify it was rewritten to BMCLAPI
        QCOMPARE(url.host(), QString("bmclapi2.bangbang93.com"));
        QVERIFY(url.path().startsWith("/maven/"));
    }
    
    void testMojangRewrite() {
        m_settings->set("DownloadMirrorType", 1);
        
        // Test various URLs
        // Since we can't easily inject arbitrary URLs into getDownloads without configuring a Library object,
        // we will configure Library objects with different repository URLs that match the patterns we want to test.
        
        struct TestCase {
            QString inputUrl;
            QString expectedHost;
            QString expectedPathStart;
        };
        
        QList<TestCase> cases = {
            {"https://libraries.minecraft.net", "bmclapi2.bangbang93.com", "/maven/"},
            {"https://files.minecraftforge.net/maven", "bmclapi2.bangbang93.com", "/maven/"},
            {"https://maven.fabricmc.net", "bmclapi2.bangbang93.com", "/maven/"},
            {"https://meta.fabricmc.net", "bmclapi2.bangbang93.com", "/fabric-meta/"}, // Note: path might need adjustment based on implementation
            {"https://maven.neoforged.net/releases", "bmclapi2.bangbang93.com", "/maven/"}
        };
        
        for (const auto& testCase : cases) {
            auto lib = std::make_shared<Library>();
            lib->setRawName(GradleSpecifier("com.test:lib:1.0"));
            lib->setRepositoryURL(testCase.inputUrl);
            
            RuntimeContext context;
            QStringList failed;
            auto dls = lib->getDownloads(context, m_cache.get(), failed, "", m_settings);
            
            if (dls.isEmpty()) {
                QFAIL(qPrintable("No downloads generated for URL: " + testCase.inputUrl));
            }
            
            auto url = dls.first()->url();
            QCOMPARE(url.host(), testCase.expectedHost);
            QVERIFY2(url.path().startsWith(testCase.expectedPathStart), qPrintable("URL path mismatch for " + testCase.inputUrl + ". Got: " + url.toString()));
        }
    }

    void testOfficialMirror() {
        // Set mirror type to Official (0)
        m_settings->set("DownloadMirrorType", 0);
        
        auto lib = std::make_shared<Library>();
        lib->setRawName(GradleSpecifier("com.example:test:1.0"));
        lib->setRepositoryURL("https://libraries.minecraft.net/");
        
        RuntimeContext context;
        QStringList failed;
        auto dls = lib->getDownloads(context, m_cache.get(), failed, "", m_settings);
        
        QVERIFY(!dls.isEmpty());
        auto url = dls.first()->url();
        // Should remain unchanged
        QCOMPARE(url.host(), QString("libraries.minecraft.net"));
    }

    void testCustomRewrite() {
        m_settings->set("DownloadMirrorType", 2); // Custom
        m_settings->set("MojangDownloadsMirrorURL", "https://mirror.example.test/api");
        m_settings->set("LibrariesURL", "https://mirror.example.test/maven");
        m_settings->set("ResourceURLOverride", "");
        m_settings->set("LegacyFMLLibsURLOverride", "");

        const auto mirror = MirrorDownload::resolve(m_settings.get());
        QCOMPARE(static_cast<int>(mirror.type), static_cast<int>(MirrorDownload::Custom));
        QCOMPARE(mirror.mojangBase, QString("https://mirror.example.test/api/"));
        QCOMPARE(mirror.librariesBase, QString("https://mirror.example.test/maven/"));

        QCOMPARE(MirrorDownload::rewriteUrl("https://launcher.mojang.com/v1/objects/a/client.jar", mirror),
                 QString("https://mirror.example.test/api/v1/objects/a/client.jar"));
        QCOMPARE(MirrorDownload::rewriteUrl("https://piston-meta.mojang.com/mc/game/version_manifest_v2.json", mirror),
                 QString("https://mirror.example.test/api/mc/game/version_manifest_v2.json"));
        QCOMPARE(MirrorDownload::rewriteUrl("https://libraries.minecraft.net/com/example/lib/1.0/lib-1.0.jar", mirror),
                 QString("https://mirror.example.test/maven/com/example/lib/1.0/lib-1.0.jar"));
        QCOMPARE(MirrorDownload::rewriteUrl("https://files.minecraftforge.net/maven/net/minecraftforge/forge/1.0/forge-1.0.jar", mirror),
                 QString("https://mirror.example.test/maven/net/minecraftforge/forge/1.0/forge-1.0.jar"));
        QCOMPARE(MirrorDownload::rewriteUrl("https://maven.fabricmc.net/net/fabricmc/fabric-loader/0.15/fabric-loader-0.15.jar", mirror),
                 QString("https://mirror.example.test/maven/net/fabricmc/fabric-loader/0.15/fabric-loader-0.15.jar"));
        QCOMPARE(MirrorDownload::rewriteUrl("https://meta.fabricmc.net/v2/versions/loader", mirror),
                 QString("https://mirror.example.test/api/fabric-meta/v2/versions/loader"));
    }

    void testCustomDerivedEndpoints() {
        m_settings->set("DownloadMirrorType", 2);
        m_settings->set("MojangDownloadsMirrorURL", "https://mirror.example.test/");
        m_settings->set("LibrariesURL", "");
        m_settings->set("ResourceURLOverride", "");
        m_settings->set("LegacyFMLLibsURLOverride", "");

        const auto mirror = MirrorDownload::resolve(m_settings.get());
        QCOMPARE(mirror.librariesBase, QString("https://mirror.example.test/maven/"));
        QCOMPARE(mirror.assetsBase, QString("https://mirror.example.test/assets/"));
        QCOMPARE(mirror.fmlLibsBase, mirror.librariesBase);
        QCOMPARE(mirror.authlibInjectorBase, QString("https://mirror.example.test/mirrors/authlib-injector/"));

        QCOMPARE(MirrorDownload::rewriteUrl("https://resources.download.minecraft.net/ab/abcdef", mirror),
                 QString("https://mirror.example.test/assets/ab/abcdef"));
        QCOMPARE(MirrorDownload::rewriteUrl("https://libraries.minecraft.net/com/example/lib/1.0/lib-1.0.jar", mirror),
                 QString("https://mirror.example.test/maven/com/example/lib/1.0/lib-1.0.jar"));
    }

    void testCustomLibraryDownloadUsesLibrariesURL() {
        m_settings->set("DownloadMirrorType", 2);
        m_settings->set("MojangDownloadsMirrorURL", "");
        m_settings->set("LibrariesURL", "https://mirror.example.test/maven/");
        m_settings->set("ResourceURLOverride", "");
        m_settings->set("LegacyFMLLibsURLOverride", "");

        auto lib = std::make_shared<Library>();
        lib->setRawName(GradleSpecifier("com.custom:test:1.0"));
        lib->setRepositoryURL("https://libraries.minecraft.net/");

        RuntimeContext context;
        QStringList failed;
        auto dls = lib->getDownloads(context, m_cache.get(), failed, "", m_settings);

        QVERIFY(!dls.isEmpty());
        auto url = dls.first()->url();
        QCOMPARE(url.host(), QString("mirror.example.test"));
        QVERIFY(url.path().startsWith("/maven/"));
    }

    void testBMCLAPICompleteMapping() {
        m_settings->set("DownloadMirrorType", 1);

        const auto mirror = MirrorDownload::resolve(m_settings.get());
        QCOMPARE(mirror.quiltMetaBase, QString("https://bmclapi2.bangbang93.com/quilt-meta/"));
        QCOMPARE(mirror.authlibInjectorBase, QString("https://bmclapi2.bangbang93.com/mirrors/authlib-injector/"));

        QCOMPARE(MirrorDownload::rewriteUrl("https://maven.quiltmc.org/repository/release/org/quiltmc/quilt-loader/1.0/quilt-loader-1.0.jar",
                                            mirror),
                 QString("https://bmclapi2.bangbang93.com/maven/org/quiltmc/quilt-loader/1.0/quilt-loader-1.0.jar"));
        QCOMPARE(MirrorDownload::rewriteUrl("http://dl.liteloader.com/versions/versions.json", mirror),
                 QString("https://bmclapi2.bangbang93.com/maven/com/mumfrey/liteloader/versions.json"));
        QCOMPARE(MirrorDownload::rewriteUrl("https://resources.download.minecraft.net/ab/abcdef", mirror),
                 QString("https://bmclapi2.bangbang93.com/assets/ab/abcdef"));
    }

    void testCustomDoesNotRewriteUnknownHosts() {
        m_settings->set("DownloadMirrorType", 2);
        m_settings->set("MojangDownloadsMirrorURL", "https://mirror.example.test/");
        m_settings->set("LibrariesURL", "https://mirror.example.test/maven/");

        const auto mirror = MirrorDownload::resolve(m_settings.get());
        QCOMPARE(MirrorDownload::rewriteUrl("https://cdn.example.test/file.jar", mirror), QString("https://cdn.example.test/file.jar"));
    }

    void testCustomModApiMirrors() {
        m_settings->set("ModrinthMirror", ModApiMirror::Custom);
        m_settings->set("ModrinthMirrorURL", "https://mirror.example.test/modrinth/v2/");
        m_settings->set("CurseForgeMirror", ModApiMirror::Custom);
        m_settings->set("CurseForgeMirrorURL", "https://mirror.example.test/curseforge/v1/");

        QCOMPARE(ModApiMirror::modrinthBaseUrl(m_settings.get()), QString("https://mirror.example.test/modrinth/v2"));
        QCOMPARE(ModApiMirror::curseForgeBaseUrl(m_settings.get()), QString("https://mirror.example.test/curseforge/v1"));

        // Empty custom URL falls back to the official API.
        m_settings->set("ModrinthMirrorURL", "");
        m_settings->set("CurseForgeMirrorURL", "");
        QCOMPARE(ModApiMirror::modrinthBaseUrl(m_settings.get()), QString(BuildConfig.MODRINTH_PROD_URL));
        QCOMPARE(ModApiMirror::curseForgeBaseUrl(m_settings.get()), QString(BuildConfig.FLAME_BASE_URL));
    }

    void testModApiMirrorPresets() {
        m_settings->set("ModrinthMirror", ModApiMirror::MCIM);
        m_settings->set("CurseForgeMirror", ModApiMirror::MCIM);
        QCOMPARE(ModApiMirror::modrinthBaseUrl(m_settings.get()), QString("https://mod.mcimirror.top/modrinth/v2"));
        QCOMPARE(ModApiMirror::curseForgeBaseUrl(m_settings.get()), QString("https://mod.mcimirror.top/curseforge/v1"));

        m_settings->set("ModrinthMirror", ModApiMirror::Official);
        m_settings->set("CurseForgeMirror", ModApiMirror::Official);
        QCOMPARE(ModApiMirror::modrinthBaseUrl(m_settings.get()), QString(BuildConfig.MODRINTH_PROD_URL));
        QCOMPARE(ModApiMirror::curseForgeBaseUrl(m_settings.get()), QString(BuildConfig.FLAME_BASE_URL));
    }

private:
    std::shared_ptr<MockSettingsObject> m_settings;
    std::unique_ptr<HttpMetaCache> m_cache;
};

QTEST_GUILESS_MAIN(BMCLAPITest)
#include "BMCLAPITest.moc"
