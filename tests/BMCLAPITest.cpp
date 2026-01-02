#include <QTest>
#include <QDebug>
#include <memory>
#include "minecraft/Library.h"
#include "minecraft/MirrorDownload.h"
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

private:
    std::shared_ptr<MockSettingsObject> m_settings;
    std::unique_ptr<HttpMetaCache> m_cache;
};

QTEST_GUILESS_MAIN(BMCLAPITest)
#include "BMCLAPITest.moc"
