# PCL.Download 集成到 LunaLauncher — 交接文档

> 编写日期：2026-08-20
> 状态：**已实现**（2026-08-21）
>
> 实现情况摘要：
> - Phase 1 ✅ `PCL.Download.csproj` 已配置 NativeAOT（`PublishAot`+`NativeLib=Shared`），`Exports.cs` 导出全部 20 个 C API；`dotnet publish -r win-x64` 通过，产出 6.8MB `PCL.Download.dll`，dumpbin 验证全部导出符号
> - Phase 2 ✅ `launcher/net/PclDownloadLibrary.h/.cpp`（合并了文档中 PclDownloadApi/PclDownloadBackend 两个任务，提供 QString/QUrl 封装 + `PclDownloadBridge` 事件桥）
> - Phase 3 ✅ `launcher/net/PclDownloadTask.h/.cpp`（继承 `Net::Download`，与 Aria2Download 同构，非文档草稿中的 NetRequest）；`Download.cpp` 两个 factory 已接入，PCL 优先、aria2 后备、Qt 保底；**进度/完成/失败采用原生回调主动推送（非轮询）**
> - Phase 4 ✅ `libraries/PCL.Download/meson.build` 改为 NativeAOT publish + RID 检测；根 `meson.build` 增加 `pcl-download-deploy` 目标把 DLL 拷到构建目录 launcher exe 旁
> - Phase 5 ✅ 编译期守卫改为与 `JAVA_DOWNLOADER_ENABLED` 相同的运行时开关：`BuildConfig.PCL_DOWNLOAD_ENABLED`（BuildConfig.h / BuildConfig.cpp.in）；CMakeLists.txt 同步添加新源文件；未做 aria2 编译期互斥（改为运行时候退链）
> - UI 设置 ✅ 独立设置页 `ui/pages/global/PclDownloadPage.h/.cpp`（注册于 `Application.cpp`，在 aria2 页之后），含 4 个设置项：`PclDownloadEnabled`（默认开）、`PclDownloadFallbackToQt`（默认开）、`PclDownloadThreadLimit`（默认 16）、`PclDownloadSpeedLimitKBps`（KB/s，0=不限）；页面显示引擎加载状态，Apply 时即时下发线程/速度限制到原生库
> - 测试 ✅ T1-T4 通过（`libraries/PCL.Download/test_smoke.py`，ctypes 直测：加载/单文件下载/多 URL 容灾）；T5-T11 待整机验证

---

## 一、项目背景

### 1.1 目标

将 PCL-CE 的下载模块（C#/.NET）以 **NativeAOT 共享库** 形式集成到 LunaLauncher（C++/Qt），替换现有 aria2 下载后端，实现：

- 多分块并行下载（加速大文件）
- 断点续传（断网不丢进度）
- 多 URL 容灾（一个源失败自动切下一个）
- 进度追踪 / 速度限制 / 并发控制
- 不依赖 .NET 运行时（NativeAOT 编译为原生库）
- 跨平台（Windows / Linux / macOS）

### 1.2 技术选型

| 方案 | 选择 | 理由 |
|---|---|---|
| 集成方式 | NativeAOT + `[UnmanagedCallersOnly]` | 不需要装 .NET 运行时；产出原生 `.dll`/`.so`/`.dylib`；跟 aria2 一样是加载外部库 |
| 接入点 | `Download.cpp` 的 3 个 factory 方法 | 392 处调用方零改动 |
| 替换对象 | aria2 后端 | aria2 有 18 个已知 bug（见附录 A） |

### 1.3 仓库位置

| 仓库 | 路径 | 说明 |
|---|---|---|
| PCL-CE（源） | `D:\Projects\PCL-CE` | 下载模块原始实现 |
| LunaLauncher（目标） | `D:\Projcets\PrismLauncher` | 集成目标 |
| PCL.Download（已提取） | `D:\Projcets\PrismLauncher\libraries\PCL.Download` | 独立 .NET 类库，已拷贝到目标仓库 |

---

## 二、现有代码分析

### 2.1 LunaLauncher 下载架构

```
调用方 (392 处)
  │
  ├─ Net::Download::makeCached(url, entry)      ← 24 处：meta/翻译/modpack/图标
  ├─ Net::Download::makeFile(url, path)          ← 12 处：mod/Java/皮肤/更新
  ├─ Net::Download::makeByteArray(url)           ← 14 处：JSON API（不经 aria2）
  ├─ Net::ApiDownload::makeCached/File/ByteArray ← 48 处：带 API Key 的变体
  ├─ Net::Upload::makeByteArray                  ← 8 处：POST 请求
  └─ 直接 m_network->get()                       ← 13 处：authlib/nide8 等
         │
         ▼
  Download::makeCached / makeFile (Download.cpp:56-99)
         │
         ├─ Aria2Download::shouldUseFor(url)?
         │     YES → Aria2Download (WebSocket RPC → aria2c 进程)
         │     NO  → 普通 Qt QNetworkAccessManager
         │
         ▼
  NetJob (并发任务管理器，继承 ConcurrentTask)
         │
         ├─ canDelegateWholeQueueToAria2() → 批量委托给 aria2
         └─ 正常调度 → 按 max_concurrent 逐步执行
```

### 2.2 关键文件清单

| 文件 | 职责 | 行数 |
|---|---|---|
| `launcher/net/Download.h/.cpp` | 下载 facade，factory 方法入口 | 105 |
| `launcher/net/NetJob.h/.cpp` | 批量下载任务管理 | 254 |
| `launcher/net/NetRequest.h/.cpp` | 下载请求基类 | — |
| `launcher/net/Aria2Download.h/.cpp` | aria2 下载任务实现 | 282 |
| `launcher/net/Aria2Manager.h/.cpp` | aria2 进程管理 + WebSocket RPC | 922 |
| `launcher/net/HttpMetaCache.h/.cpp` | 缓存系统 | — |
| `launcher/net/MetaCacheSink.h/.cpp` | 缓存写入 + MD5 验证 | — |
| `launcher/net/FileSink.h/.cpp` | 文件写入 | — |
| `launcher/net/ByteArraySink.h` | 内存写入 | — |
| `launcher/tasks/ConcurrentTask.h/.cpp` | 并发任务基类 | — |

### 2.3 现有 aria2 集成方式（PCL.Download 参照同样的模式）

aria2 是通过以下方式集成的：

1. `Aria2Manager` 单例管理 aria2c 外部进程
2. 通过 HTTP JSON-RPC 添加/取消下载
3. 通过 WebSocket 接收实时事件推送
4. `Aria2Download` 继承 `Download`，重写 `executeTask()` / `abort()`
5. `Download::makeCached` / `makeFile` 中判断是否走 aria2

PCL.Download 集成方式类似，但更简单（不需要外部进程/IPC）：

1. `PclDownloadLibrary` 加载 NativeAOT 共享库（`LoadLibrary` / `dlopen`）
2. 通过 C 函数指针直接调用（无 RPC/进程间通信）
3. 进度通过轮询函数获取（`pcl_download_get_progress`）
4. `PclDownloadTask` 继承 `NetRequest`，重写 `executeTask()` / `abort()`
5. `Download::makeCached` / `makeFile` 中判断是否走 PCL.Download

### 2.4 下载调用方完整列表

#### `Net::Download::makeCached`（5 处）

| 文件:行号 | 下载内容 |
|---|---|
| `translations/TranslationsModel.cpp:680` | 翻译索引 `index_v2.json` |
| `translations/TranslationsModel.cpp:721` | 单个语言包 |
| `ui/pages/modplatform/ftb/FtbListModel.cpp:241` | FTB pack 图标 |
| `news/NewsChecker.cpp:63` | RSS 新闻 |
| `java/download/ArchiveDownloadTask.cpp:41` | Java JRE 压缩包 |

#### `Net::Download::makeFile`（6 处）

| 文件:行号 | 下载内容 |
|---|---|
| `updater/prismupdater/PrismUpdater.cpp:769` | 更新二进制 |
| `modplatform/ftb/FTBPackInstallTask.cpp:310` | FTB mod 文件 |
| `ui/dialogs/skins/SkinManageDialog.cpp:231` | 披风图片 |
| `ui/dialogs/skins/SkinManageDialog.cpp:419` | 皮肤图片 |
| `ui/dialogs/skins/SkinManageDialog.cpp:480` | 他人皮肤 |
| `java/download/ManifestDownloadTask.cpp:158` | JRE 文件 |

#### `Net::Download::makeByteArray`（14 处）

| 文件:行号 | 下载内容 |
|---|---|
| `updater/prismupdater/PrismUpdater.cpp:1139` | GitHub releases JSON |
| `modplatform/ftb/FTBPackInstallTask.cpp:95` | FTB 版本 JSON |
| `ui/pages/modplatform/ftb/FtbListModel.cpp:99` | FTB 列表 JSON |
| `ui/pages/modplatform/ftb/FtbListModel.cpp:151` | FTB 搜索 JSON |
| `ui/dialogs/skins/SkinManageDialog.cpp:478` | UUID 查询 |
| `ui/dialogs/skins/SkinManageDialog.cpp:479` | 皮肤 URL |
| `ui/dialogs/ProfileSetupDialog.cpp:164` | 用户名可用性 |
| `modplatform/legacy_ftb/PackFetchTask.cpp:63` | Legacy FTB 列表 |
| `minecraft/auth/steps/YggdrasilProfileStep.cpp:84` | Yggdrasil profile |
| `minecraft/auth/steps/UnifiedPassMetaStep.cpp:60` | UnifiedPass 元数据 |
| `minecraft/auth/steps/MinecraftProfileStep.cpp:24` | MC profile |
| `minecraft/auth/steps/GetSkinStep.cpp:26` | 皮肤数据 |
| `minecraft/auth/steps/EntitlementsStep.cpp:33` | 权益信息 |
| `java/download/ManifestDownloadTask.cpp:69` | JRE manifest |

#### `Net::ApiDownload::makeCached`（19 处）

| 文件:行号 | 下载内容 |
|---|---|
| `InstanceImportTask.cpp:104` | Modpack ZIP |
| `meta/BaseEntity.cpp:167` | Meta 版本清单 |
| `ui/MainWindow.cpp:1097` | Modpack 文件 |
| `modplatform/legacy_ftb/PackInstallTask.cpp:84` | Legacy FTB ZIP |
| `modplatform/atlauncher/ATLPackInstallTask.cpp:642` | AT 配置 ZIP |
| `modplatform/atlauncher/ATLPackInstallTask.cpp:749` | AT mod (直链) |
| `modplatform/atlauncher/ATLPackInstallTask.cpp:759` | AT mod (托管) |
| `modplatform/atlauncher/ATLPackInstallTask.cpp:772` | AT 依赖 mod |
| `minecraft/Library.cpp:206` | MC 库 JAR (有 SHA1) |
| `minecraft/Library.cpp:211` | MC 库 JAR (无校验) |
| `ui/pages/modplatform/technic/TechnicModel.cpp:301` | Technic 图标 |
| `ui/pages/modplatform/modrinth/ModrinthModel.cpp:257` | Modrinth 图标 |
| `ui/pages/modplatform/legacy_ftb/ListModel.cpp:274` | Legacy FTB 图标 |
| `ui/pages/modplatform/flame/FlameModel.cpp:115` | CurseForge 图标 |
| `ui/pages/modplatform/atlauncher/AtlListModel.cpp:200` | AT 图标 |
| `ui/widgets/VariableSizedImageObject.cpp:144` | 内联图片 |
| `modplatform/technic/SingleZipPackInstallTask.cpp:50` | Technic ZIP |
| `minecraft/update/LegacyFMLLibrariesTask.cpp:75` | FML 库 |
| `minecraft/update/AssetUpdateTask.cpp:64` | 资源索引 |

#### `Net::ApiDownload::makeFile`（6 处）

| 文件:行号 | 下载内容 |
|---|---|
| `modplatform/flame/FlameInstanceCreationTask.cpp:580` | CurseForge mod |
| `minecraft/AssetsUtils.cpp:285` | MC 资源文件 |
| `ResourceDownloadTask.cpp:48` | 通用资源文件 |
| `modplatform/modrinth/ModrinthInstanceCreationTask.cpp:259` | Modrinth mod |
| `modplatform/modrinth/ModrinthInstanceCreationTask.cpp:267` | Modrinth 备用源 |
| `modplatform/technic/SolderPackInstallTask.cpp:117` | Technic Solder mod |

#### `Net::ApiDownload::makeByteArray`（23 处）

| 文件:行号 | 下载内容 |
|---|---|
| `meta/CleanroomMeta.cpp:183` | Cleanroom Maven 元数据 |
| `modplatform/atlauncher/ATLPackInstallTask.cpp:91` | AT 版本 JSON |
| `modplatform/ResourceAPI.cpp:24` | 资源搜索结果 |
| `modplatform/ResourceAPI.cpp:89` | 资源版本列表 |
| `modplatform/ResourceAPI.cpp:210` | 依赖解析 |
| `modplatform/ResourceAPI.cpp:299` | 项目元数据 |
| `ui/pages/modplatform/technic/TechnicPage.cpp:168` | Technic pack 信息 |
| `ui/pages/modplatform/technic/TechnicPage.cpp:269` | Solder modlist |
| `ui/pages/modplatform/technic/TechnicModel.cpp:160` | Technic 搜索 |
| `ui/pages/modplatform/atlauncher/AtlOptionalModDialog.cpp:162` | AT 可选 mod 列表 |
| `ui/pages/modplatform/atlauncher/AtlListModel.cpp:102` | AT pack 列表 |
| `modplatform/technic/SolderPackInstallTask.cpp:76` | Solder pack 元数据 |
| `modplatform/modrinth/ModrinthAPI.cpp:18` | Modrinth hash 查询 |
| `modplatform/modrinth/ModrinthAPI.cpp:110` | Modrinth 搜索 |
| `modplatform/modrinth/ModrinthAPI.cpp:138` | Modrinth 分类 |
| `modplatform/legacy_ftb/PackFetchTask.cpp:57` | Legacy FTB 公开列表 |
| `modplatform/legacy_ftb/PackFetchTask.cpp:81` | Legacy FTB 私有包 |
| `modplatform/hangar/HangarAPI.cpp:415` | Hangar 版本列表 |
| `modplatform/flame/FlameCheckUpdate.cpp:62` | CurseForge 更新检查 |
| `modplatform/flame/FlameAPI.cpp:44` | CurseForge 指纹匹配 |
| `modplatform/flame/FlameAPI.cpp:79` | CurseForge mod 描述 |
| `modplatform/flame/FlameAPI.cpp:163` | CurseForge 文件信息 |
| `modplatform/flame/FlameAPI.cpp:187` | CurseForge 分类 |

#### 直接 `m_network->get()`（13 处，不经过 Net::Download 框架）

| 文件:行号 | 下载内容 | 说明 |
|---|---|---|
| `minecraft/auth/AuthlibInjectorDownload.cpp:61` | authlib-injector 版本 JSON | BMCLAPI 镜像 |
| `minecraft/auth/AuthlibInjectorDownload.cpp:81` | authlib-injector 版本 JSON | 官方源 |
| `minecraft/auth/AuthlibInjectorDownload.cpp:126` | authlib-injector JAR | 二进制下载 |
| `minecraft/auth/AuthlibInjectorDownloadTask.cpp:69` | 同上（Task 变体） | — |
| `minecraft/auth/AuthlibInjectorDownloadTask.cpp:88` | 同上 | — |
| `minecraft/auth/AuthlibInjectorDownloadTask.cpp:133` | 同上 | — |
| `minecraft/auth/Nide8AuthDownload.cpp:55` | Nide8 auth JAR | — |
| `minecraft/auth/Nide8AuthDownloadTask.cpp:61` | 同上（Task 变体） | — |
| `minecraft/auth/Nide8AuthDownloadTask.cpp:61` | 同上 | — |
| `minecraft/online/YukariConnectDownload.cpp:67` | YukariConnect release JSON | GitHub/Gitee |
| `minecraft/online/YukariConnectDownload.cpp:166` | YukariConnect 二进制 | — |
| `minecraft/online/TerracottaDownload.cpp:80` | Terracotta release JSON | GitHub/Gitee |
| `minecraft/online/TerracottaDownload.cpp:138` | Terracotta 二进制 | — |

> **注意**：这 13 处直接调用不走 `Net::Download` facade，PCL.Download 集成**不会自动覆盖**它们。
> 如需覆盖，需要单独修改这些文件。

---

## 三、PCL.Download 库现状

### 3.1 源码位置

```
libraries/PCL.Download/
├── meson.build                  ← Meson 构建（需改为 NativeAOT 编译）
├── PCL.Download.csproj          ← .NET 10 类库项目
├── Config/
│   └── DownloadConfig.cs        ← 线程数/速度限制配置
├── Internal/
│   ├── LoadState.cs             ← 状态枚举
│   ├── LoaderBase.cs            ← 加载器基类（状态机+进度+事件）
│   ├── SafeList.cs              ← 线程安全列表
│   ├── FileChecker.cs           ← 文件完整性校验（MD5/SHA1/SHA256）
│   └── LogHelper.cs             ← 日志门面
├── Downloader/
│   └── FileDownloader.cs        ← 核心下载器（多源容灾+分块并发+断点续传）
├── Models/
│   ├── DownloadFile.cs          ← 下载文件状态模型
│   └── NetworkEnums.cs          ← NetState 枚举
├── Loaders/
│   ├── LoaderDownload.cs        ← 批量下载编排（并发+重试+进度聚合）
│   └── LoaderDownloadUnc.cs     ← UNC 路径文件复制
└── Management/
    └── NetManager.cs            ← 全局下载任务管理单例
```

### 3.2 外部依赖

| 依赖 | 版本 | 说明 |
|---|---|---|
| `Downloader` NuGet 包 | 5.9.4 | 分块并发下载引擎 |
| .NET 10.0 SDK | 10.0.111+ | 编译时需要 |
| 无运行时依赖 | — | NativeAOT 自包含 |

### 3.3 已内置的修复

| 问题 | 修复内容 |
|---|---|
| 断点续传无效 | `EnableAutoResumeDownload = true` |
| 失败时删临时文件 | 仅在取消/全部失败时清理 |
| 下载前强制清理 | 不再删除已有临时文件（支持续传） |

### 3.4 当前编译状态

```
✅ dotnet build — 0 警告 0 错误
✅ 目标框架 net10.0
✅ 输出 PCL.Download.dll (43KB)
❌ 尚未配置 NativeAOT 编译
❌ 尚未添加 [UnmanagedCallersOnly] 导出
```

---

## 四、实现任务清单

### Phase 1：C# 导出层

| # | 任务 | 文件 | 详细说明 | 预估工时 |
|---|---|---|---|---|
| 1.1 | 配置 NativeAOT 编译 | `PCL.Download.csproj` | 添加：`<PublishAot>true</PublishAot>` `<OutputType>Library</OutputType>` `<IsAotCompatible>true</IsAotCompatible>` | 0.5h |
| 1.2 | 创建 C API 导出 | `Exports.cs`（新建） | 见下方 §4.1 API 设计。所有导出方法必须：`static`、`[UnmanagedCallersOnly]`、参数仅 blittable 类型 | 3h |
| 1.3 | 字符串 marshal 封装 | `Exports.cs` 内 | 输入：`Marshal.PtrToStringUTF8(IntPtr)` → `string`；输出：`Marshal.StringToCoTaskMemUTF8(string)` → `IntPtr`；释放：`Marshal.FreeCoTaskMem(IntPtr)` | 含在 1.2 |
| 1.4 | 异步同步包装 | `Exports.cs` 内 | 所有 `async Task` 方法包一层 `Task.Run(() => ...).GetAwaiter().GetResult()` | 含在 1.2 |
| 1.5 | 验证各平台编译 | 命令行 | `dotnet publish -r win-x64 -c Release` → `PCL.Download.dll`；`-r linux-x64` → `libPCL.Download.so`；`-r osx-arm64` → `libPCL.Download.dylib` | 1h |

#### 4.1 C API 导出设计

```csharp
using System.Runtime.InteropServices;

namespace PCL.Download;

public static class Exports
{
    // ========== 初始化/销毁 ==========
    
    [UnmanagedCallersOnly(EntryPoint = "pcl_download_init")]
    public static int Init()
    {
        // 设置默认日志、初始化 NetManager
        return 0;
    }

    [UnmanagedCallersOnly(EntryPoint = "pcl_download_shutdown")]
    public static void Shutdown()
    {
        // 取消所有活跃下载、清理资源
    }

    // ========== 单文件下载 ==========
    
    [UnmanagedCallersOnly(EntryPoint = "pcl_download_file")]
    public static int DownloadFile(IntPtr urlsJsonPtr, IntPtr localPathPtr, 
                                     IntPtr hashPtr, long expectedSize)
    {
        string urlsJson = Marshal.PtrToStringUTF8(urlsJsonPtr) ?? "[]";
        string localPath = Marshal.PtrToStringUTF8(localPathPtr) ?? "";
        string? hash = Marshal.PtrToStringUTF8(hashPtr);
        
        // 解析 JSON URL 数组
        // 创建 DownloadFile + LoaderDownload
        // 启动下载
        // 返回 task_id
    }

    [UnmanagedCallersOnly(EntryPoint = "pcl_download_file_wait")]
    public static int DownloadFileWait(int taskId)
    {
        // 阻塞等待 task 完成
        // 返回 0=成功, 1=失败, 2=取消
    }

    [UnmanagedCallersOnly(EntryPoint = "pcl_download_file_abort")]
    public static void DownloadFileAbort(int taskId)
    {
        // 取消指定任务
    }

    // ========== 批量下载 ==========
    
    [UnmanagedCallersOnly(EntryPoint = "pcl_download_batch_start")]
    public static int BatchStart(IntPtr filesJsonPtr)
    {
        string filesJson = Marshal.PtrToStringUTF8(filesJsonPtr) ?? "[]";
        // 解析 JSON，创建 List<DownloadFile>
        // 创建 LoaderDownload
        // 返回 batch_id
    }

    [UnmanagedCallersOnly(EntryPoint = "pcl_download_batch_get_state")]
    public static int BatchGetState(int batchId)
    {
        // 0=运行中, 1=完成, 2=失败, 3=取消
    }

    [UnmanagedCallersOnly(EntryPoint = "pcl_download_batch_get_progress")]
    public static double BatchGetProgress(int batchId)
    {
        // 0.0 ~ 1.0
    }

    [UnmanagedCallersOnly(EntryPoint = "pcl_download_batch_get_progress_detail")]
    public static IntPtr BatchGetProgressDetail(int batchId)
    {
        // 返回 JSON：每个文件的进度/速度/状态
        // 调用方必须用 pcl_free_string 释放
    }

    [UnmanagedCallersOnly(EntryPoint = "pcl_download_batch_wait")]
    public static int BatchWait(int batchId) { /* 阻塞等待 */ }

    [UnmanagedCallersOnly(EntryPoint = "pcl_download_batch_abort")]
    public static void BatchAbort(int batchId) { /* 取消全部 */ }

    // ========== 全局状态 ==========
    
    [UnmanagedCallersOnly(EntryPoint = "pcl_download_get_speed")]
    public static long GetSpeed() => NetManager.Instance.Speed;

    [UnmanagedCallersOnly(EntryPoint = "pcl_download_get_active_threads")]
    public static int GetActiveThreads() => NetManager.Instance.ThreadCount;

    // ========== 错误处理 ==========
    
    [UnmanagedCallersOnly(EntryPoint = "pcl_download_get_error")]
    public static IntPtr GetError(int taskId)
    {
        // 返回错误字符串，调用方必须用 pcl_free_string 释放
    }

    [UnmanagedCallersOnly(EntryPoint = "pcl_free_string")]
    public static void FreeString(IntPtr ptr) => Marshal.FreeCoTaskMem(ptr);

    // ========== 配置 ==========
    
    [UnmanagedCallersOnly(EntryPoint = "pcl_download_set_thread_limit")]
    public static void SetThreadLimit(int limit) 
        => DownloadConfig.NetTaskThreadLimit = limit;

    [UnmanagedCallersOnly(EntryPoint = "pcl_download_set_speed_limit")]
    public static void SetSpeedLimit(long bytesPerSec)
        => DownloadConfig.NetTaskSpeedLimitHigh = bytesPerSec;
}
```

### Phase 2：C++ 封装层

| # | 任务 | 文件 | 详细说明 | 预估工时 |
|---|---|---|---|---|
| 2.1 | NativeAOT 库加载器 | `PclDownloadLibrary.h/.cpp`（新建） | 跨平台封装：Windows 用 `LoadLibraryW` / `GetProcAddress`；Linux/macOS 用 `dlopen` / `dlsym`。库文件名：Win=`PCL.Download.dll`, Linux=`libPCL.Download.so`, macOS=`libPCL.Download.dylib` | 2h |
| 2.2 | C 函数指针声明 | `PclDownloadApi.h`（新建） | 所有 `[UnmanagedCallersOnly]` 导出函数的 `typedef`。例如：`typedef int (*pcl_download_file_fn)(const char*, const char*, const char*, int64_t);` | 1h |
| 2.3 | Qt 风格封装类 | `PclDownloadBackend.h/.cpp`（新建） | 单例类，封装所有 API 调用。提供 `QUrl`/`QString` 接口，内部做 UTF-8 转换。提供信号：`downloadFinished(int taskId, bool success)` | 3h |
| 2.4 | 字符串生命周期 | `PclDownloadBackend.cpp` 内 | 传入：`QString::toUtf8().constData()`；返回：接收后立即 `QString::fromUtf8(ptr)` 然后 `pcl_free_string(ptr)` | 含在 2.3 |

#### 2.1 `PclDownloadLibrary.h` 参考实现

```cpp
#pragma once

#include <QLibrary>
#include <QString>

class PclDownloadLibrary {
public:
    static PclDownloadLibrary& instance();
    
    bool load();  // 加载共享库，返回是否成功
    bool isLoaded() const;
    QString errorString() const;

    // 函数指针（从库中动态获取）
    int   (*init)();
    void  (*shutdown)();
    int   (*download_file)(const char*, const char*, const char*, int64_t);
    int   (*download_file_wait)(int);
    void  (*download_file_abort)(int);
    int   (*batch_start)(const char*);
    int   (*batch_get_state)(int);
    double(*batch_get_progress)(int);
    int   (*batch_wait)(int);
    void  (*batch_abort)(int);
    long long(*get_speed)();
    int   (*get_active_threads)();
    const char*(*get_error)(int);
    void  (*free_string)(const char*);
    void  (*set_thread_limit)(int);
    void  (*set_speed_limit)(long long);

private:
    PclDownloadLibrary() = default;
    QLibrary m_lib;
    bool m_loaded = false;
};
```

#### 2.1 `PclDownloadLibrary.cpp` 参考实现

```cpp
#include "PclDownloadLibrary.h"

PclDownloadLibrary& PclDownloadLibrary::instance() {
    static PclDownloadLibrary inst;
    return inst;
}

bool PclDownloadLibrary::load() {
    if (m_loaded) return true;

#ifdef Q_OS_WIN
    m_lib.setFileName("PCL.Download");
#elif defined(Q_OS_MACOS)
    m_lib.setFileName("PCL.Download");  // Qt 自动加 lib 前缀和 .dylib 后缀
#else
    m_lib.setFileName("PCL.Download");  // Qt 自动加 lib 前缀和 .so 后缀
#endif

    if (!m_lib.load()) {
        return false;
    }

    // 获取所有函数指针
    #define LOAD_FUNC(name, type) \
        name = reinterpret_cast<type>(m_lib.resolve(#name)); \
        if (!name) return false;

    LOAD_FUNC(init,                 int(*)());
    LOAD_FUNC(shutdown,             void(*)());
    LOAD_FUNC(download_file,        int(*)(const char*, const char*, const char*, int64_t));
    LOAD_FUNC(download_file_wait,   int(*)(int));
    LOAD_FUNC(download_file_abort,  void(*)(int));
    LOAD_FUNC(batch_start,          int(*)(const char*));
    LOAD_FUNC(batch_get_state,      int(*)(int));
    LOAD_FUNC(batch_get_progress,   double(*)(int));
    LOAD_FUNC(batch_wait,           int(*)(int));
    LOAD_FUNC(batch_abort,          void(*)(int));
    LOAD_FUNC(get_speed,            long long(*)());
    LOAD_FUNC(get_active_threads,   int(*)());
    LOAD_FUNC(get_error,            const char*(*)(int));
    LOAD_FUNC(free_string,          void(*)(const char*));
    LOAD_FUNC(set_thread_limit,     void(*)(int));
    LOAD_FUNC(set_speed_limit,      void(*)(long long));

    #undef LOAD_FUNC

    m_loaded = true;
    init();
    return true;
}
```

### Phase 3：接入下载框架

| # | 任务 | 文件 | 详细说明 | 预估工时 |
|---|---|---|---|---|
| 3.1 | PclDownloadTask 类 | `PclDownloadTask.h/.cpp`（新建） | 继承 `Net::NetRequest`。构造时接收 `QUrl` + 目标路径。`executeTask()` 中：URL 列表 → JSON → `pcl_download_file()` → 轮询进度 → 完成/失败 | 4h |
| 3.2 | 修改 `Download::makeCached` | `Download.cpp:56-69` | 在 aria2 判断之前加 `#ifdef Launcher_ENABLE_PCL_DOWNLOAD` 分支，调用 `PclDownloadTask` | 0.5h |
| 3.3 | 修改 `Download::makeFile` | `Download.cpp:86-99` | 同上 | 0.5h |
| 3.4 | 更新源文件列表 | `source_lists/meson.build` | 添加新的 `.h` / `.cpp` 文件 | 0.5h |

#### 3.2 `Download.cpp` 修改参考

```cpp
#if defined(LAUNCHER_APPLICATION)
#include "net/PclDownloadTask.h"  // 新增
#include "net/Aria2Download.h"
#endif

auto Download::makeCached(QUrl url, MetaEntryPtr entry, Options options) -> Download::Ptr
{
#ifdef Launcher_ENABLE_PCL_DOWNLOAD
    // PCL.Download 优先（仅文件下载，不处理 byte array）
    if (PclDownloadTask::shouldUseFor(url)) {
        return PclDownloadTask::makeCached(std::move(url), entry, options);
    }
#elif defined(LAUNCHER_APPLICATION)
    if (Aria2Download::shouldUseFor(url) && entry->isStale() && !QFileInfo::exists(entry->getFullPath())) {
        return Aria2Download::makeCached(std::move(url), entry, options);
    }
#endif
    // ... 原有 Qt 下载逻辑不变
}

auto Download::makeFile(QUrl url, QString path, Options options) -> Download::Ptr
{
#ifdef Launcher_ENABLE_PCL_DOWNLOAD
    if (PclDownloadTask::shouldUseFor(url)) {
        return PclDownloadTask::makeFile(std::move(url), std::move(path), options);
    }
#elif defined(LAUNCHER_APPLICATION)
    if (Aria2Download::shouldUseFor(url)) {
        return Aria2Download::makeFile(std::move(url), std::move(path), options);
    }
#endif
    // ... 原有 Qt 下载逻辑不变
}

// makeByteArray 不改 — API 请求不需要多连接加速
```

#### 3.1 `PclDownloadTask.h` 参考

```cpp
#pragma once

#include "net/NetRequest.h"
#include <QUrl>
#include <QTimer>

class PclDownloadTask : public Net::NetRequest {
    Q_OBJECT

public:
    using Ptr = shared_qobject_ptr<PclDownloadTask>;

    explicit PclDownloadTask(QUrl url, QString targetPath);
    ~PclDownloadTask() override;

    static bool shouldUseFor(const QUrl& url);
    static Net::NetRequest::Ptr makeCached(QUrl url, MetaEntryPtr entry, Net::Options options);
    static Net::NetRequest::Ptr makeFile(QUrl url, QString path, Net::Options options);

    bool abort() override;
    int replyStatusCode() const override;
    QNetworkReply::NetworkError error() const override;
    QString errorString() const override;

protected:
    void executeTask() override;

private slots:
    void pollProgress();

private:
    QUrl m_url;
    QString m_targetPath;
    int m_taskId = -1;
    QTimer m_pollTimer;
    QNetworkReply::NetworkError m_error = QNetworkReply::NoError;
    QString m_errorString;
    int m_statusCode = -1;
};
```

### Phase 4：Meson 构建集成

| # | 任务 | 文件 | 详细说明 | 预估工时 |
|---|---|---|---|---|
| 4.1 | 更新 PCL.Download meson.build | `libraries/PCL.Download/meson.build` | 改用 `dotnet publish -r <RID>` 产出原生共享库 | 1h |
| 4.2 | 添加 RID 检测 | `libraries/PCL.Download/meson.build` | 根据 `host_machine.system()` + `host_machine.cpu_family()` 自动选 RID | 含在 4.1 |
| 4.3 | 更新安装路径 | `libraries/PCL.Download/meson.build` | 安装到 `lib_dest_dir` 而非 `jars_dest_dir` | 含在 4.1 |
| 4.4 | CI 跨平台验证 | CI 配置 | Windows + Linux 各编译一次 | 1h |

#### 4.1 `meson.build` 修改参考

```meson
# libraries/PCL.Download/meson.build

dotnet_prog = find_program('dotnet', required: false)

if not dotnet_prog.found()
  warning('dotnet SDK not found. PCL.Download will not be built.')
  pcl_download_lib = disabler()
else
  # 确定 RID
  if is_windows
    if host_machine.cpu_family() == 'x86_64'
      dotnet_rid = 'win-x64'
    elif host_machine.cpu_family() == 'aarch64'
      dotnet_rid = 'win-arm64'
    else
      dotnet_rid = 'win-x86'
    endif
    pcl_download_lib_name = 'PCL.Download.dll'
  elif is_linux
    if host_machine.cpu_family() == 'x86_64'
      dotnet_rid = 'linux-x64'
    elif host_machine.cpu_family() == 'aarch64'
      dotnet_rid = 'linux-arm64'
    elif host_machine.cpu_family() == 'arm'
      dotnet_rid = 'linux-arm'
    else
      error('Unsupported Linux architecture for PCL.Download')
    endif
    pcl_download_lib_name = 'libPCL.Download.so'
  else
    # macOS
    if host_machine.cpu_family() == 'x86_64'
      dotnet_rid = 'osx-x64'
    else
      dotnet_rid = 'osx-arm64'
    endif
    pcl_download_lib_name = 'libPCL.Download.dylib'
  endif

  pcl_download_source_root = meson.current_source_dir()
  pcl_download_output_dir = meson.current_build_dir() / 'publish'

  # 收集源文件
  pcl_download_sources = run_command(
    find_program('python'), '-c',
    'import os; root = r"@0@"; [print(os.path.relpath(os.path.join(dp, f), root)) for dp, dn, fn in os.walk(root) for f in fn if f.endswith((".cs", ".csproj"))]'.format(pcl_download_source_root),
    check: true,
  ).stdout().strip().split('\n')

  pcl_download_lib = custom_target(
    'PCL.Download',
    input: pcl_download_sources,
    output: pcl_download_lib_name,
    command: [
      dotnet_prog, 'publish',
      pcl_download_source_root / 'PCL.Download.csproj',
      '-c', 'Release',
      '-r', dotnet_rid,
      '--self-contained', 'true',
      '-o', pcl_download_output_dir,
      '--nologo', '-v', 'quiet',
    ],
    install: true,
    install_dir: library_dest_dir,
    console: true,
  )

  # 手动重建目标
  run_target(
    'pcl-download-rebuild',
    command: [
      dotnet_prog, 'publish',
      pcl_download_source_root / 'PCL.Download.csproj',
      '-c', 'Release', '-r', dotnet_rid,
      '--self-contained', 'true',
      '--nologo',
    ],
  )
endif
```

### Phase 5：Meson 选项与编译标志

| # | 任务 | 文件 | 详细说明 | 预估工时 |
|---|---|---|---|---|
| 5.1 | 确认 meson option 存在 | `meson_options.txt` | 已有 `build_pcl_download`，默认 `true` | — |
| 5.2 | 确认编译标志传递 | `buildconfig/meson.build` | 已有 `Launcher_ENABLE_PCL_DOWNLOAD` | — |
| 5.3 | 条件编译守卫 | 新建的 `.cpp` 文件 | 所有 PCL.Download 相关代码用 `#ifdef Launcher_ENABLE_PCL_DOWNLOAD` 包裹 | — |
| 5.4 | 与 aria2 互斥 | `meson.build` | 可选：`build_pcl_download=true` 时自动禁用 aria2 分支 | 0.5h |

---

## 五、已知风险与注意事项

### 5.1 NativeAOT 限制

| 限制 | 影响 | 对策 |
|---|---|---|
| 不能 `Assembly.Load` | 不能动态加载插件 | 不需要，下载库是固定的 |
| 不能 `Reflection.Emit` | 不能动态生成代码 | 不需要 |
| 导出方法必须同步 | 异步方法不能直接导出 | 用 `Task.Run(...).GetAwaiter().GetResult()` 包装 |
| 参数必须 blittable | 不能直接传 `string` | 用 `IntPtr` + `Marshal.PtrToStringUTF8` |
| 每个 RID 需要单独编译 | 不能一份二进制跨平台 | CI 为每个平台各编一份 |
| 二进制较大（~5-10MB） | 包含裁剪后的 .NET 运行时 | 可接受，aria2c 也是 ~5MB |

### 5.2 Downloader NuGet 包兼容性

| 项目 | 状态 |
|---|---|
| net10.0 支持 | ✅ NuGet 页面确认兼容 |
| NativeAOT 兼容 | ⚠️ 未确认。`Downloader` 库用了反射，NativeAOT 可能裁剪掉。需要测试。如果不兼容，需用 `--linker-feature-switch` 或回退到 hostfxr 方案 |
| 跨平台 | ✅ 纯 .NET 实现，无 P/Invoke |

### 5.3 进度推送（已实现为回调，非轮询）

最终实现**不使用轮询**。既然 NativeAOT 库以内嵌 DLL 方式运行在同一进程内，C++ 侧直接把一个 Cdecl 函数指针通过 `pcl_download_set_event_callback` 传给 .NET，.NET 侧用 C# 9 函数指针（`delegate* unmanaged[Cdecl]<...>`）直接回调：

- 事件类型：0=progress（下载进度，C# 侧节流 ~10Hz）、1=finished、2=failed、3=aborted
- 回调参数均为 blittable 类型（`int taskId, int eventType, long downloaded, long total, long speed, int threads`）
- 回调运行在 .NET 线程池线程上，C++ 侧 `PclDownloadBridge` 用 `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` 排队到主线程再发 Qt 信号，线程安全
- 原有的 `pcl_download_get_state` / `pcl_download_get_progress*` / `pcl_download_get_error` 拉取接口全部保留：错误详情仍在收到 failed 事件后按需拉取，也作为掉事件的兜底

文档初稿认为“`[UnmanagedCallersOnly]` 不能跨语言传递委托”——这不准确：不能传的是**托管委托**，传**原生函数指针**并让 C# 用函数指针调用是完全支持的（NativeAOT 兼容）。

### 5.4 `makeByteArray` 不替换

`Download::makeByteArray` 用于 JSON API 调用（14 处）和上传（8 处）。这些请求：

- 数据量小（几 KB）
- 不需要多连接加速
- 有 POST 方法（PCL.Download 只支持 GET）

所以保持走 Qt QNetworkAccessManager。

### 5.5 直接 `m_network->get()` 的 13 处

authlib-injector、Nide8、YukariConnect、Terracotta 等直接调用 `QNetworkAccessManager::get()`，不经过 `Net::Download` facade。这些不在本次集成范围内。如需覆盖，需单独修改。

---

## 六、测试计划

| # | 测试项 | 方法 | 预期结果 |
|---|---|---|---|
| T1 | NativeAOT 编译 | `dotnet publish -r win-x64 -c Release` | 产出 `PCL.Download.dll`，无错误 |
| T2 | 库加载 | `PclDownloadLibrary::load()` | 返回 `true`，所有函数指针非空 |
| T3 | 单文件下载 | 调用 `pcl_download_file` 下载一个小文件 | 文件正确，MD5 匹配 |
| T4 | 多 URL 容灾 | 提供一个无效 + 一个有效 URL | 自动 fallback，下载成功 |
| T5 | 断点续传 | 下载大文件，中途 kill 进程，重启后再次下载 | 从断点继续，不从零开始 |
| T6 | 并发下载 | 批量下载 10 个文件 | 全部完成，进度正确 |
| T7 | 取消 | 下载中调用 `pcl_download_file_abort` | 任务取消，信号正确发出 |
| T8 | 缓存命中 | 对已存在的有效文件调 `makeCached` | 跳过下载，直接返回 |
| T9 | CurseForge 特殊处理 | 下载 CurseForge CDN 文件 | HttpClient 正确携带 API Key |
| T10 | 跨平台 | Linux 上编译并运行 | `.so` 加载成功，下载正常 |
| T11 | 回归 | 正常启动 MC、安装 modpack | 所有下载正常，无 UI 卡顿 |

---

## 七、附录

### 附录 A：Aria2 已知 Bug 列表（18 个）

| # | 严重性 | 问题 | 位置 |
|---|---|---|---|
| 1 | Critical | abort 竞态：删临时文件后仍收到 complete 通知 | `Aria2Download.cpp:272` vs `:149` |
| 2 | Critical | abort 不发 aborted/finished 信号，任务可能永远卡住 | `Aria2Download.cpp:272-280` |
| 3 | Critical | downloadFinished 最后一次 write 不检查返回值 | `Aria2Manager.cpp:133` |
| 4 | High | WebSocket 断开不重连，永久降级为轮询 | `Aria2Manager.cpp:479` |
| 5 | High | 旧进程 finished 信号可能触发虚假 restart | `Aria2Manager.cpp:422` vs `:526` |
| 6 | High | RPC 请求无超时，aria2 挂起时请求堆积 | `Aria2Manager.cpp:806` |
| 7 | High | 无下载停滞检测 | `Aria2Manager.cpp` 全局 |
| 8 | Medium | downloadOptions 无 APPLICATION_DYN 守卫 | `Aria2Manager.cpp:719` |
| 9 | Medium | ensureStarted 不等 RPC 就绪 | `Aria2Manager.cpp:460` |
| 10 | Medium | 固定端口重启全部用同一端口 | `Aria2Manager.cpp:430` vs `:555` |
| 11 | Medium | clearFinished 不清 aria2 内部结果 | `Aria2Manager.cpp:909` |
| 12 | Medium | 下载中途失败无 fallback | `Aria2Download.cpp:137` |
| 13 | Medium | m_restartAttempts 成功后不重置 | `Aria2Manager.cpp:541` |
| 14 | Medium | Aria2ExtraArgs 可覆盖安全选项 | `Aria2Manager.cpp:436-453` |
| 15 | Low | fallback 时临时文件残留 | `Aria2Download.cpp:102` |
| 16 | Low | abort 不清理 m_sink | `Aria2Download.cpp:272` |
| 17 | Low | WebSocket 无认证 | `Aria2Manager.cpp:484` |
| 18 | Low | downloadFinished 在 write error 后仍写最后一批 | `Aria2Manager.cpp:131` |

### 附录 B：PCL.Download 模块架构

```
PCL.Download (C# .NET 10 类库)
│
├─ FileDownloader (核心)
│   ├─ 多 URL 容灾：foreach url → try → catch → next
│   ├─ 分块并发：Downloader NuGet 包，ChunkCount=4
│   ├─ 断点续传：EnableAutoResumeDownload=true
│   ├─ 速度限制：MaximumBytesPerSecond
│   └─ 临时文件：.PCLDownloading 后缀，完成后 rename
│
├─ LoaderDownload (编排)
│   ├─ SemaphoreSlim 控制文件级并发
│   ├─ 重试：4 次，指数退避 300-1400ms
│   ├─ 进度聚合：Average(file.Progress)
│   └─ 状态机：Waiting → Loading → Finished/Failed/Aborted
│
├─ DownloadFile (模型)
│   ├─ 状态：NetState 枚举
│   ├─ 进度：DownloadedBytes / TotalSize / Speed / ActiveThreads
│   └─ 错误：Errors 列表（线程安全）
│
├─ NetManager (全局单例)
│   ├─ Files：Dictionary<string, DownloadFile>
│   └─ Tasks：SafeList<LoaderDownload>
│
└─ FileChecker (校验)
    ├─ MD5 / SHA1 / SHA256
    ├─ 文件大小检查
    └─ 最小大小检查
```

### 附录 C：参考资料

| 资源 | 链接 |
|---|---|
| Downloader NuGet 包 | https://www.nuget.org/packages/Downloader |
| .NET NativeAOT 文档 | https://learn.microsoft.com/en-us/dotnet/core/deploying/native-aot |
| UnmanagedCallersOnly | https://learn.microsoft.com/en-us/dotnet/api/system.runtime.interopservices.unmanagedcallersonlyattribute |
| .NET Hosting API (hostfxr) | https://learn.microsoft.com/en-us/dotnet/core/tutorials/netcore-hosting |
| PCL-CE 仓库 | https://github.com/PCL-Community/PCL-CE |
| LunaLauncher 仓库 | https://github.com/AndreaFrederica/LunaLauncher |

### 附录 D：工时估算

| Phase | 工时 | 说明 |
|---|---|---|
| Phase 1：C# 导出层 | 4h | NativeAOT 配置 + 导出函数 |
| Phase 2：C++ 封装层 | 6h | 库加载 + API 声明 + Qt 封装 |
| Phase 3：接入下载框架 | 5.5h | PclDownloadTask + 修改 Download.cpp |
| Phase 4：Meson 构建 | 2h | NativeAOT 编译 + RID 检测 |
| Phase 5：配置与互斥 | 0.5h | 编译守卫 |
| 测试 | 4h | 11 个测试项 |
| **总计** | **~22h** | 约 3 个工作日 |
