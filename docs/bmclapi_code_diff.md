# BMCLAPI Support Implementation - Diff Report

此文档详细列出了为了支持 BMCLAPI 镜像源而对 Prism Launcher 代码库进行的所有修改。

## 1. 核心逻辑修改

### `launcher/minecraft/Library.h`
修改 `getDownloads` 方法签名，允许注入 `SettingsObject` 以便在测试环境中模拟配置。

```diff
--- a/launcher/minecraft/Library.h
+++ b/launcher/minecraft/Library.h
@@ -147,7 +147,8 @@ class Library {
     QList<Net::NetRequest::Ptr> getDownloads(const RuntimeContext& runtimeContext,
                                              class HttpMetaCache* cache,
                                              QStringList& failedLocalFiles,
-                                             const QString& overridePath) const;
+                                             const QString& overridePath,
+                                             std::shared_ptr<SettingsObject> settings = nullptr) const;
```

### `launcher/minecraft/Library.cpp`
实现 BMCLAPI 的 URL 重写逻辑，覆盖 Fabric, Forge, NeoForge 以及 Mojang 库文件和元数据下载。

```diff
--- a/launcher/minecraft/Library.cpp
+++ b/launcher/minecraft/Library.cpp
@@ -110,7 +110,8 @@ void Library::getApplicableFiles(const RuntimeContext& runtimeContext,
 QList<Net::NetRequest::Ptr> Library::getDownloads(const RuntimeContext& runtimeContext,
                                                   class HttpMetaCache* cache,
                                                   QStringList& failedLocalFiles,
-                                                  const QString& overridePath) const
+                                                  const QString& overridePath,
+                                                  std::shared_ptr<SettingsObject> settings) const
 {
     QList<Net::NetRequest::Ptr> out;
     bool stale = isAlwaysStale();
@@ -130,13 +131,26 @@ QList<Net::NetRequest::Ptr> Library::getDownloads(const RuntimeContext& runtimeC
     };

     // Lambda function to rewrite Mojang URLs to mirror URLs
-    auto rewrite_url = [](QString url) -> QString {
-        auto s = APPLICATION->settings();
+    auto rewrite_url = [settings](QString url) -> QString {
+        auto s = settings ? settings : APPLICATION->settings();
         int mirrorType = s->get("DownloadMirrorType").toInt();
         QString replacementUrl;

         if (mirrorType == MirrorDownload::MirrorType::BMCLAPI) {
             replacementUrl = "https://bmclapi2.bangbang93.com";
+
+            // Fabric
+            url.replace("https://meta.fabricmc.net", "https://bmclapi2.bangbang93.com/fabric-meta");
+            url.replace("https://maven.fabricmc.net", "https://bmclapi2.bangbang93.com/maven");
+
+            // Forge
+            url.replace("https://files.minecraftforge.net/maven", "https://bmclapi2.bangbang93.com/maven");
+
+            // NeoForge
+            url.replace("https://maven.neoforged.net/releases", "https://bmclapi2.bangbang93.com/maven");
+
+            // Libraries
+            url.replace("https://libraries.minecraft.net", "https://bmclapi2.bangbang93.com/maven");
         } else if (mirrorType == MirrorDownload::MirrorType::Custom) {
             // For custom mirrors, use the user-provided URL
             replacementUrl = s->get("MojangDownloadsMirrorURL").toString();
@@ -151,9 +165,13 @@ QList<Net::NetRequest::Ptr> Library::getDownloads(const RuntimeContext& runtimeC
             url.replace("https://launcher.mojang.com/", replacementUrl);
             url.replace("https://piston-data.mojang.com/", replacementUrl);
             url.replace("https://piston-meta.mojang.com/", replacementUrl);
+            url.replace("https://launchermeta.mojang.com/", replacementUrl);
+            url.replace("https://s3.amazonaws.com/Minecraft.Download/", replacementUrl);
             url.replace("http://launcher.mojang.com/", replacementUrl);
             url.replace("http://piston-data.mojang.com/", replacementUrl);
             url.replace("http://piston-meta.mojang.com/", replacementUrl);
+            url.replace("http://launchermeta.mojang.com/", replacementUrl);
+            url.replace("http://s3.amazonaws.com/Minecraft.Download/", replacementUrl);
         }
         return url;
     };
@@ -232,14 +250,15 @@ QList<Net::NetRequest::Ptr> Library::getDownloads(const RuntimeContext& runtimeC
             }
         }
     } else {
-        auto raw_dl = [this, raw_storage]() {
+        auto raw_dl = [this, raw_storage, settings]() {
             if (!m_absoluteURL.isEmpty()) {
                 return m_absoluteURL;
             }

             if (m_repositoryURL.isEmpty()) {
                 // Check for LibrariesURL override setting
-                auto librariesUrl = APPLICATION->settings()->get("LibrariesURL").toString();
+                auto s = settings ? settings : APPLICATION->settings();
+                auto librariesUrl = s->get("LibrariesURL").toString();
                 if (!librariesUrl.isEmpty()) {
                     return librariesUrl + raw_storage;
                 }
```

### `launcher/minecraft/update/AssetUpdateTask.cpp`
在资源更新任务中添加对 `s3.amazonaws.com` 的镜像支持。

```diff
--- a/launcher/minecraft/update/AssetUpdateTask.cpp
+++ b/launcher/minecraft/update/AssetUpdateTask.cpp
@@ -48,6 +48,8 @@ void AssetUpdateTask::executeTask()
         urlString.replace("http://launchermeta.mojang.com/", replacementUrl);
         urlString.replace("http://launcher.mojang.com/", replacementUrl);
         urlString.replace("http://piston-meta.mojang.com/", replacementUrl);
+        urlString.replace("https://s3.amazonaws.com/Minecraft.Download/", replacementUrl);
+        urlString.replace("http://s3.amazonaws.com/Minecraft.Download/", replacementUrl);
         indexUrl = QUrl(urlString);
     }
```

### `launcher/minecraft/MirrorDownload.cpp`
更新 BMCLAPI 的定义，使其提供 FML Libraries 的下载地址。

```diff
--- a/launcher/minecraft/MirrorDownload.cpp
+++ b/launcher/minecraft/MirrorDownload.cpp
@@ -29,13 +29,12 @@ const std::array<MirrorDownload::MirrorTypeInfo, 3> MirrorDownload::MirrorTypes
             ""   // FMLLibsURL - use BuildConfig.FMLLIBS_BASE_URL
         },
         // BMCLAPI - https://bmclapi2.bangbang93.com
-        // Note: BMCLAPI does not host FML Libraries, so this will fall back to default
         {
             QT_TRANSLATE_NOOP("APIPage", "BMCLAPI"),
             "https://bmclapi2.bangbang93.com/",
             "https://bmclapi2.bangbang93.com/assets/",
             "https://bmclapi2.bangbang93.com/maven/",
-            ""  // FML Libraries - use default (BMCLAPI doesn't provide this)
+            "https://bmclapi2.bangbang93.com/maven/"  // FML Libraries
         },
         // Custom - User-provided URLs
         {
```

### `launcher/ui/pages/global/APIPage.cpp`
在应用设置时，正确应用镜像源配置中的 `FMLLibsURL`。

```diff
--- a/launcher/ui/pages/global/APIPage.cpp
+++ b/launcher/ui/pages/global/APIPage.cpp
@@ -312,7 +312,7 @@ void APIPage::applySettings()
         s->set("MetaURLOverride", "");  // Keep default for Prism Launcher meta
         s->set("ResourceURL", mirrorInfo.defaultAssetsUrl);
         s->set("LibrariesURL", mirrorInfo.defaultLibrariesUrl);
-        s->set("FMLLibsURL", "");
+        s->set("FMLLibsURL", mirrorInfo.defaultFMLLibsUrl);
         s->set("MojangDownloadsMirrorURL", "");  // BMCLAPI URL is handled in code, not stored
     }
```

## 2. 测试代码

### `tests/CMakeLists.txt`
注册新的测试用例。

```diff
--- a/tests/CMakeLists.txt
+++ b/tests/CMakeLists.txt
@@ -15,6 +15,12 @@ ecm_add_test(MojangVersionFormat_test.cpp LINK_LIBRARIES Launcher_logic Qt${QT_V
 ecm_add_test(Library_test.cpp LINK_LIBRARIES Launcher_logic Qt${QT_VERSION_MAJOR}::Test
     TEST_NAME Library)

+ecm_add_test(BMCLAPITest.cpp LINK_LIBRARIES Launcher_logic Qt${QT_VERSION_MAJOR}::Test
+    TEST_NAME BMCLAPI)
+
+ecm_add_test(BMCLAPICompareTest.cpp LINK_LIBRARIES Launcher_logic Qt${QT_VERSION_MAJOR}::Test
+    TEST_NAME BMCLAPICompare)
+
 ecm_add_test(ResourceFolderModel_test.cpp LINK_LIBRARIES Launcher_logic Qt${QT_VERSION_MAJOR}::Test
     TEST_NAME ResourceFolderModel)
```

### `tests/BMCLAPITest.cpp` (New)
单元测试：验证 `Library::getDownloads` 是否正确重写了各种类型的 URL。

```cpp
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
    // ... (Mock implementation) ...
};

class BMCLAPITest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { ... }
    void testBMCLAPIRewrite() { ... }
    void testMojangRewrite() { ... }
    void testOfficialMirror() { ... }
private:
    std::shared_ptr<MockSettingsObject> m_settings;
    std::unique_ptr<HttpMetaCache> m_cache;
};

QTEST_GUILESS_MAIN(BMCLAPITest)
#include "BMCLAPITest.moc"
```

### `tests/BMCLAPICompareTest.cpp` (New)
集成测试：对比 BMCLAPI 和 Mojang 官方 API 返回的元数据结构和版本号是否一致。

```cpp
#include <QTest>
#include <QNetworkAccessManager>
// ... (includes)

class BMCLAPICompareTest : public QObject {
    Q_OBJECT
private:
    QNetworkAccessManager *manager;
    QByteArray fetch(const QUrl &url) { ... }

private slots:
    void initTestCase() { manager = new QNetworkAccessManager(this); }
    void compareVersionManifest() {
        // Compares version_manifest_v2.json from Mojang and BMCLAPI
        // Checks if latest release matches and if URLs are preserved/mirrored structure
    }
};

QTEST_GUILESS_MAIN(BMCLAPICompareTest)
#include "BMCLAPICompareTest.moc"
```

## 3. 其他

### `.gitignore`
```diff
--- a/.gitignore
+++ b/.gitignore
@@ -73,3 +73,4 @@ flatbuild

 .history/
 .trae/documents/*
+Testing/Temporary/*
```
