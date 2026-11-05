# 多实例存储、原版目录转换与客户端/服务端同步计划

> 状态：计划已完成首个可用版本，并已加入外部来源证据模型、混合来源识别、Classic/Neo UI 展示和 JSRT recognizer 扩展入口；后续只需按真实目录样本继续扩大识别器和兼容性回归。

## 0. 已落地基础能力（当前版本）

- 新增 `InstanceStore`/`InstanceStoreRegistry`，以 JSON 保存多个实例目录、格式、启用状态、可写状态和默认安装位置。
- 当前注册表文件位于启动器数据根的 `instance-stores.json`，JSON 顶层字段使用 `InstanceStores`；读取同时兼容早期草案中的 `stores` 字段。
- 首次启动会把旧的单值 `InstanceDir` 自动登记为 Prism Store，并继续保留该设置作为兼容别名。
- 自动登记 `<启动器安装目录>/UserData/instances`：普通模式默认禁用，便携模式默认启用并作为新建默认位置；安装目录旁的 `instances` 不会被自动扫描。
- `InstanceList` 已支持同时扫描多个启用的 Prism Store，分别监听目录变化，并对跨 Store 的重复实例目录名采取首个 Store 优先策略。
- 启动状态 API 的数据位置结果包含 `instanceStores`，为后续设置页提供统一数据源。
- 已加入 `VanillaCompatibleProvider`：扫描带 `versions/<id>/<id>.json` 的 PCL/HMCL/Mojang 目录，识别方言、版本隔离布局，并在每个实例目录内保存 `luna.settings.json`。
- 已加入 Store 管理 API：`instance-store.list`、`instance-store.add`、`instance-store.update`、`instance-store.remove`。删除只移除注册，不删除目录；自动便携 Store 不能被删除。
- 已加入 `VanillaConverter`：导入原版兼容实例到 Prism 副本，导出 Prism 内容到原版兼容副本；导出遇到 `mmc-pack.json`、`patches/` 等 MMC 专属数据时会阻塞并返回报告。
- PCL CE/HMCL 导入会递归展开 `inheritsFrom` 和 `patches`，标准 Forge、NeoForge、Fabric、Quilt 会生成对应 MMC loader 组件；自定义 loader、手动替换 JAR、无法完整映射的运行时会保留为本地组件，并把原始/解析后的 JSON 和专属目录放入 `luna-imported`，同时在转换报告中明确警告。
- 经典界面的转换入口已使用 `VanillaConversionTask`，会询问新实例名称和分组，显示阶段进度弹窗；完成后展示所有转换警告，失败时保留源目录并显示具体原因。
- 已加入 `SyncProfile`/`SyncEngine`：保存客户端/服务端（一对多 server IDs）、common/clientOnly/serverOnly mod 与配置声明，生成哈希差异预览；配置文件支持基于保存内容基线的三方合并，无法合并时报告冲突，并以安全写入应用无冲突变更。
- 双向同步配置会保存 `sync.baseHashes`；应用成功后更新基线，从而识别“仅一侧改变”和“双方同时改变”。
- 已加入 `instance.store-transfer`，支持停止运行中的 Prism 实例后在两个 Prism Store 之间安全 copy/move，使用 staging 目录和目标存在检查。
- 原版版本 JSON 会在读取时递归展开 `inheritsFrom`，合并父子 libraries/arguments，并拒绝继承环；导入时将展开结果写入 Prism 的 `net.minecraft` patch。
- 原版导出会允许可表达的 vanilla 基础组件和 Fabric、Forge、NeoForge、Quilt 标准 loader 组件，并把其 libraries、arguments、mainClass 合并到版本 JSON；普通 `mods/config/saves/resourcepacks/shaderpacks` 仍可复制。
- `vanilla.loader.install` 可为已有原版版本生成继承型 loader profile；PCL/HMCL 方言扩展字段通过导出参数写入版本 JSON，不会向外部目录写入 MMC 文件。
- 全局设置页已显示已注册 Store，并可添加 Prism 或 vanilla-compatible 目录；新建实例对话框会选择启用的可写 Prism Store，便携模式切换仍独立于实例迁移。
- `instance.create` 支持 `targetStoreId`，原版导入支持 `targetStoreId`；因此 CLI、Neo UI 和桌面创建流程都能明确选择安装目标。
- `sync.profile.list/remove` 已提供配置管理，计划/应用操作在省略根目录时会根据绑定的客户端/服务端实例自动解析游戏目录。
- API 已提供 `vanilla-instance.list`、`instance.convert.import`、`instance.convert.export`、`sync.profile.save`、`sync.profile.plan`、`sync.profile.apply`。

### 外部实例来源识别（多启动器混合修改）

外部目录不能再用单个 `dialect` 字符串表示来源。一个实例可能先后被 HMCL、PCL、MultiMC/Prism 或其他启动器写过，因此当前模型增加了 `ExternalInstanceProvenance`：

```json
{
  "providers": [
    {
      "providerId": "pcl",
      "displayName": "PCL",
      "iconId": "pcl",
      "matchedPaths": ["PCL.ini"],
      "matchedFields": [],
      "confidence": 75,
      "canReadInstanceSettings": true,
      "canEditCanonicalMetadata": true
    }
  ],
  "primaryProvider": "pcl",
  "mixed": false,
  "badgeId": "pcl"
}
```

`providers` 是平等的证据集合；`primaryProvider` 只为旧版调用方提供默认文字，不参与冲突裁决。检测到多个非原版来源时，`mixed=true`、`badgeId=mixed`，界面显示混合来源角标并保留所有证据。官方原版只作为没有其他来源证据时的基线来源，不会把“HMCL 修改过的原版目录”误判成普通官方实例。

识别器已经拆到 `ExternalLauncherRecognizerRegistry`，内置 HMCL、PCL、MultiMC/Prism 和官方原版都注册为同一接口的 provider。扫描器只提交目录、布局和版本 JSON，注册表负责运行所有识别器并合并结果；以后新增 XMCL、PolyMC 或其他启动器只需注册新的 provider，不需要修改 `VanillaCompatibleProvider` 的分支逻辑。

Classic 和 Neo UI 使用同一份 provenance 数据：实例设置增加“来源与修改痕迹”页面，列出来源、命中的文件/字段、置信度、读写能力和冲突提示；列表角标使用 `badgeId`。API 提供 `vanilla-instance.provenance`、`external-launchers.list-recognizers` 和受限的 `external-launchers.icon`，Neo UI 通过 `neo.task.start` 异步调用它们。

JSRT 只作为可选 provider 扩展层：脚本返回标准化的 `id/displayName/iconId/detect/evidence/capabilities`，C++ 对返回值做 schema 校验、路径限制、大小限制和置信度合并。脚本不能直接改写实例 JSON，也不能返回任意图标路径；图标只引用 `iconId`，由安装目录资源索引解析到 `external-launchers/<id>/icon.png`（可附带 `manifest.json` 和许可证说明）。识别失败、超时或格式错误的脚本会被单独记录为 recognizer 错误，不影响内置识别器和实例启动。

此外还提供 `instance.store-transfer` API：`{ "instance": "...", "targetStoreId": "...", "mode": "copy|move" }`。它只接受 Prism 源实例，外部原版实例必须先走显式转换流程。
- 外部原版实例已通过 `VanillaInstance` 适配器进入 `InstanceList`；Store 的“可写”能力由用户单独控制。启用可写时，实例编辑、内存/Java 等 Luna 设置、普通 mod/config/saves 和标准 loader 操作直接写回原实例目录；Prism 的 `mmc-pack.json`、`patches/` 等组件控制数据仍只写入启动器数据根的 `foreign-runtime/<stable-id>`，不会污染 HMCL/PCL/原版目录。

这些能力已经覆盖 Provider、Store 管理、双向转换、跨 Store 迁移和带基线的同步引擎，并已接入更新完成后的自动同步。版本继承和标准 loader 会转换为原版可读的版本 JSON；不支持的 Prism 专属组件仍会在转换前明确阻塞。

## 1. 目标

将当前单一 Prism 实例目录扩展为多个实例存储，并统一支持：

- 多个 Prism 格式实例目录。
- PCL、HMCL 和官方原版兼容目录。
- 共享游戏目录和版本隔离目录。
- 普通模式与便携模式切换。
- Prism 格式与原版兼容格式之间的双向转换。
- 客户端实例与服务端实例绑定。
- 客户端/服务端专属 mod、通用 mod 以及配置文件的同步。

设计的核心边界是：Minecraft 游戏内容尽量跨格式共享；`mmc-pack.json`、`patches/`、`instance.cfg` 和 Prism 组件覆盖等 MMC 专属控制数据只能在 Prism 格式中保存，不能被悄悄丢弃或写入原版目录。

## 2. 当前实现的限制

现有实现把实例列表绑定到一个 `InstanceDir`：

- [`Application.cpp`](../launcher/Application.cpp) 注册单个 `InstanceDir`，并用它创建 `InstanceList`。
- [`InstanceList.cpp`](../launcher/InstanceList.cpp) 只扫描包含 `instance.cfg` 的一级子目录。
- 实例创建的临时目录和提交路径固定在当前实例目录下。
- [`MinecraftInstance.cpp`](../launcher/minecraft/MinecraftInstance.cpp) 默认把 `minecraft` 或 `.minecraft` 视为游戏目录。
- [`PackProfile.cpp`](../launcher/minecraft/PackProfile.cpp) 固定从 `mmc-pack.json` 和 `patches/` 加载组件。
- 全局设置页目前只有一个实例目录输入框。

因此本计划需要先抽象实例存储和运行时布局，再接入原版兼容目录；不能把多个路径简单拼接成一个字符串，也不能让原版目录伪装成带有 Prism 配置的实例。

## 3. 实例存储模型

### 3.1 `InstanceStore`

每个实例存储拥有稳定 ID、显示名称、绝对路径、格式和能力声明。建议配置结构如下：

```json
{
  "InstanceStores": [
    {
      "id": "prism-main",
      "name": "主实例目录",
      "path": "D:/Luna/instances",
      "format": "prism",
      "origin": "user",
      "enabled": true,
      "writable": true,
      "defaultForNewInstances": true
    },
    {
      "id": "hmcl",
      "name": "HMCL",
      "path": "D:/Projects/HMCL",
      "format": "vanilla-compatible",
      "origin": "user",
      "enabled": true,
      "writable": false,
      "defaultForNewInstances": false
    }
  ]
}
```

`format` 至少包含：

- `prism`
- `vanilla-compatible`

PCL、HMCL 和官方原版都使用 `vanilla-compatible`，具体来源作为 dialect 或探测结果保存，不建立三套目录实现。

每个 Store 独立负责：

- 扫描和加载实例。
- 文件监听和刷新。
- 创建、复制、迁移和删除能力。
- 分组文件或 sidecar 元数据。
- 临时目录和提交事务。

### 3.2 多个 Prism 存储

所有 `format=prism` 的 Store 都可以同时启用。例如：

```text
D:/Luna/instances
E:/Minecraft/TestInstances
F:/Backup/PrismInstances
```

每个 Prism Store 保留自己的：

```text
<store>/instgroups.json
<store>/.tmp/
<store>/<instance>/instance.cfg
<store>/<instance>/mmc-pack.json
```

创建、导入、复制和 modpack 安装时，目标 Store 从所有可写 Prism Store 中选择。

### 3.3 稳定实例 ID

物理目录名不应再作为唯一身份。需要增加逻辑 `lunaInstanceId`：

- Prism 实例保存在 `instance.cfg`。
- 外部实例的 Luna 设置保存在实例目录内的 `luna.settings.json`。
- 迁移后保留逻辑 ID。
- 内部索引使用 `storeId + instanceId`。
- 现有默认 Store 中的旧 ID 继续兼容 CLI 和 API。

同名实例可以存在于不同 Store，UI 中显示来源 Store；只有在同一 Store 内发生冲突时才要求重命名。

### 3.4 组和监听器

Prism Store 继续使用各自的 `instgroups.json`。外部实例的分组、名称、图标、备注和快捷方式保存到实例目录内的 `luna.settings.json`。一个全局实例模型聚合所有 Store，但不把不同 Store 的分组文件混写。

每个启用的 Store 都有独立 `QFileSystemWatcher` 和刷新任务。路径必须 canonicalize，并按 canonical path 去重，处理大小写差异、符号链接和 Windows junction。

## 4. 便携模式与启动器数据位置

### 4.1 固定便携目录

便携模式固定使用：

```text
<启动器安装目录>/UserData/
<启动器安装目录>/UserData/instances
```

不再自动扫描或支持启动器旁边的 `<启动器目录>/instances`。

当前项目已经有 portable 检测和数据位置切换基础，应在其上调整，而不是另建一套便携逻辑。

### 4.2 模式切换不强制迁移实例

启动器模式和实例 Store 是两个独立概念。

切换模式时：

- 只切换启动器配置档和数据根选择。
- 不自动移动实例目录。
- 不自动复制或删除 PCL/HMCL 目录。
- 不自动迁移所有账号、缓存、主题和 Java。
- 现有系统 Prism Store 继续原地保留。
- 便携 `UserData/instances` 作为另一个 Prism Store 参与管理。

默认切换操作是“切换配置位置，不复制数据”。可以提供可选迁移项：

- 复制启动器设置。
- 复制账号信息。
- 复制缓存。
- 导入现有实例 Store。

这些选项不能和实例移动绑定。

### 4.3 默认启用规则

```text
普通模式：
  <启动器目录>/UserData/instances 默认注册但不启用。

便携模式：
  <启动器目录>/UserData/instances 自动启用，并作为默认新建位置。

用户手动启用：
  任意模式下都可以启用，之后按用户选择保留。

用户手动禁用：
  不因模式切换而强制重新启用。
```

候选 Store 可以记录：

```json
{
  "id": "portable-userdata",
  "name": "便携版实例目录",
  "path": "D:/Luna/UserData/instances",
  "format": "prism",
  "origin": "portable-derived",
  "enabled": false,
  "autoEnableWhenPortable": true,
  "writable": true
}
```

如果当前 `InstanceDir` 已经解析到该路径，应合并为同一个 Store，不能重复显示。

## 5. 原版兼容目录

### 5.1 一个统一 Provider

PCL、HMCL 和官方原版都由一个 `VanillaCompatibleProvider` 处理。差异保存在 `VanillaDialect` 和运行时布局中，不分裂成多个实例系统。

Provider 需要识别：

- `versions/<id>/<id>.json`。
- 版本 JAR。
- `libraries`。
- `assets`。
- `inheritsFrom`、`jar`、`arguments`、`mainClass`。
- 共享游戏目录。
- 版本隔离目录。
- PCL/HMCL 扩展字段。

### 5.2 统一运行时布局

```cpp
struct MinecraftRuntimeLayout {
    QString versionId;
    QString versionJson;
    QString clientJar;
    QString gameRoot;
    QString versionsRoot;
    QString librariesRoot;
    QString assetsRoot;
    bool isolated;
};
```

布局模式至少包括：

- `SharedGameRoot`：版本共用 `saves`、`mods`、`config`。
- `IsolatedVersionRoot`：每个版本拥有独立游戏目录。
- `ExplicitGameDir`：由版本 JSON 或用户配置指定。
- `AutoDetect`：根据结构和文件位置检测。

不能只根据存在 `versions/<id>` 判断是否隔离。

### 5.3 外部实例 Luna 配置

外部实例目录不写入 Prism 专属组件文件：

- `instance.cfg`
- `mmc-pack.json`
- `patches/`

Luna 自己的名称、图标、分组、备注、快捷方式、Java、内存、窗口和启动参数直接保存在实例目录：

```text
<instance-root>/luna.settings.json
```

实例配置使用可读 JSON：

```json
{
  "formatVersion": 1,
  "settings": {
    "OverrideMemory": true,
    "MaxMemAlloc": 4096,
    "JvmArgs": "-XX:+UseG1GC"
  }
}
```

不读取、不迁移旧的 Luna sidecar 文件；新格式只认实例目录内的 `luna.settings.json`。实例移动时该文件随实例一起移动。

实例的稳定身份仍由 Store ID、canonical path 和版本 ID 推导；`luna.settings.json` 随实例目录移动，不依赖外部路径。

## 6. 原版格式的能力边界

### 6.1 允许的游戏内容

以下内容属于 Minecraft 游戏内容，可以在原版/PCL/HMCL 与 Prism 之间复制或同步：

- `mods/`
- `config/`
- `saves/`
- `resourcepacks/`
- `shaderpacks/`
- `options.txt`
- 服务器列表、截图和日志
- 普通 loader 需要的版本文件和库
- `.index/*.pw.toml` 等 Luna/Prism mod 元数据

PCL 转换代码中的 `copyGameFiles()` 已经采用类似策略：排除 `assets`、`libraries`、`versions` 和 PCL 启动器全局文件，同时复制其他游戏内容。

### 6.2 MMC 专属数据

以下内容不能直接写入原版格式：

- `mmc-pack.json`
- `patches/*.json`
- `instance.cfg`
- Prism 组件依赖图
- 无法展平的组件覆盖
- 无法表达的 jar mod 或 Java agent
- Prism 管理型 modpack 状态

目标格式不支持这些数据时必须阻止转换并列出具体原因，不能静默忽略。

## 7. 双向转换器

转换器拆成：

```text
VanillaRuntimeReader
VanillaCompatibilityChecker
VanillaWriter
```

### 7.1 原版兼容格式 → Prism

流程：

1. 读取并展开版本继承。
2. 解析 Minecraft、loader、主类、库和 assets。
3. 生成 Prism 的 `net.minecraft` 和 loader 组件。
4. 复制 mods、config、saves、资源包和 shader。
5. 保留 `.index` 元数据。
6. 生成 `instance.cfg` 和 `mmc-pack.json`。
7. 生成转换报告。

源目录默认保留。成功后再让用户选择是否删除源实例。

### 7.2 Prism → 原版兼容格式

流程：

1. 对组件图做兼容性预检查。
2. 将可表达的组件合并为最终版本 JSON。
3. 生成版本目录和主 JAR 引用。
4. 复制普通游戏内容和 `.index` 元数据。
5. 拒绝写入 `mmc-pack.json`、`patches` 等 MMC 文件。
6. 生成转换报告。

如果存在阻塞数据，转换失败；用户不能通过默认的“忽略”按钮丢弃这些数据。可以另外提供“仅导出可兼容游戏内容”，但它必须明确标记为内容导出，不得显示为完整格式转换。

### 7.3 单实例迁移

模式切换不迁移实例；实例迁移是独立操作：

```text
复制实例到...
迁移实例到...
```

Prism → Prism：

- 检查实例未运行。
- 在目标 Store 的 `.tmp` 中复制或移动。
- 校验文件和 `instance.cfg`。
- 更新 Store 索引、分组、快捷方式和绑定关系。
- 成功后再删除源目录。

原版兼容 → Prism：执行转换，源目录默认保留。

Prism → 原版兼容：先执行兼容性检查；有 MMC 阻塞数据时拒绝迁移。

共享布局和版本隔离布局之间的迁移必须明确选择目标布局，并重新计算 `gameRoot`、版本目录和游戏内容位置。

## 8. 新建和安装流程

新建实例、导入和 modpack 安装增加目标 Store 选择：

```text
安装到：
  主实例目录
  便携版实例目录
  测试实例目录
  PCL/HMCL 原版目录
```

选择原版兼容 Store 时，再选择：

```text
(o) 版本隔离
( ) 共享游戏目录
```

原版格式直接创建和 loader profile 生成通过 `vanilla.loader.install` 已加入；复杂 modpack 安装仍走 Prism 转换副本流程。

## 9. 客户端/服务端绑定与同步

### 9.1 `SyncProfile`

新增同步配置，放在：

```text
UserData/sync-profiles/<profile-id>.json
```

支持一个客户端绑定一个或多个服务端；旧的 `server.instanceId` 字段继续兼容，新的 `server.instanceIds` 用于一对多。

```json
{
  "formatVersion": 1,
  "id": "survival-server",
  "name": "生存服同步",
  "client": { "instanceId": "prism-main:survival-client" },
  "server": { "instanceId": "prism-main:survival-server" },
  "mods": {
    "common": ["modrinth:fabric-api", "modrinth:lithium"],
    "clientOnly": ["modrinth:sodium", "modrinth:iris"],
    "serverOnly": ["modrinth:carpet"]
  },
  "configs": {
    "common": ["config/lithium.properties"],
    "clientOnly": ["config/sodium-options.json"],
    "serverOnly": ["config/carpet.conf"]
  },
  "sync": {
    "mode": "bidirectional",
    "autoAfterUpdate": true,
    "conflictPolicy": "ask"
  }
}
```

### 9.2 mod 规则

分类优先级：

1. 配置中的显式项目 ID。
2. Modrinth、CurseForge 或其他 provider ID。
3. Fabric/Forge/NeoForge 元数据中的 mod ID。
4. 文件名回退。

未声明的 mod 默认不自动同步。`plugins/` 属于服务端插件，默认不进入客户端同步。

### 9.3 配置文件规则

不能默认同步整个 `config/`。只同步声明的文件或 glob；可以为每个规则指定：

- `replace`：整文件替换。
- `three-way`：基于上次同步基线进行三方合并。
- `manual`：只生成冲突报告。

世界、日志、账号、服务器地址和客户端视频/按键配置默认不参与同步。

### 9.4 双向冲突检测

同步状态保存上次基线、客户端哈希和服务端哈希：

| 客户端 | 服务端 | 处理 |
|---|---|---|
| 未变化 | 已变化 | 服务端同步到客户端 |
| 已变化 | 未变化 | 客户端同步到服务端 |
| 同时变化且内容相同 | 自动标记已同步 |
| 同时变化且内容不同 | 显示冲突并要求选择 |
| 两边未变化 | 跳过 |

mod 使用项目 ID、版本和文件哈希比较；配置文件使用哈希和内容比较。

### 9.5 同步流程

1. 检查客户端和服务端未运行。
2. 加载同步配置和上次基线。
3. 检查 Minecraft 版本和 loader 兼容性。
4. 扫描双方声明的 mod 和配置文件。
5. 生成新增、更新、删除和冲突计划。
6. 显示差异预览。
7. 在临时目录执行并备份目标文件。
8. 原子替换文件。
9. 更新同步状态。

客户端或服务端单独完成更新后发出 `InstanceUpdated` 事件。如果绑定配置开启 `autoAfterUpdate`，则生成同步计划；发生双侧修改、冲突或实例正在运行时必须等待用户处理。

## 10. UI 计划

设置页改为实例存储表格，包含：

- 名称
- 路径
- 格式
- 来源
- 启用状态
- 可写状态
- 默认安装位置

实例右键菜单增加：

- 复制到其他 Store
- 迁移到其他 Store
- 转换为 Prism 实例
- 导出为原版兼容格式
- 查看来源和运行时布局
- 管理客户端/服务端绑定
- 同步客户端和服务端

当前桌面 UI 已提供“复制到另一个实例目录”和外部原版实例的“转换为 Prism 副本”入口；转换副本仍保留原目录，跨 Store 移动则通过 `instance.store-transfer` API 执行并显式选择 `mode=move`。

外部实例编辑窗口包含只读的“版本”页，用于查看解析后的 Minecraft、Forge、NeoForge、Fabric 或 Quilt 组件及版本；版本替换、安装 Loader、删除、定制等会改变 Prism 组件图的动作保持禁用，并以“转换为 Prism 副本”作为编辑路径。普通 mod、config、saves、资源包和标准 loader 操作不应错误触发格式转换警告。

## 11. 实施阶段

### 阶段一：Store 基础设施

- 引入 `InstanceStore`、Store registry 和 provider 接口。
- 将现有单一 Prism Store 封装为 `PrismInstanceProvider`。
- 支持多个 Prism Store、路径去重、独立 watcher 和目标选择。
- 迁移旧 `InstanceDir` 配置。

### 阶段二：便携模式解耦

- 固定 `<启动器目录>/UserData/instances`。
- 区分配置档切换和实例迁移。
- 默认不迁移实例和全部数据。
- 支持用户选择复制设置、账号、缓存或导入 Store。
- 普通/便携模式下正确处理 Store 启用状态。

### 阶段三：原版兼容实例

- 实现 `VanillaCompatibleProvider`。
- 实现共享/隔离布局检测。
- 实现 PCL/HMCL/Mojang JSON 读取。
- 增加实例目录内的 `luna.settings.json`。
- 支持外部实例启动和游戏内容浏览。

### 阶段四：内容操作与单实例迁移

- 支持外部实例的 mods、config、saves、资源包和 shader 操作。
- 支持 `.index` 元数据。
- 实现 Prism ↔ Prism 单实例复制/迁移。
- 实现原版兼容实例导入和转换。
- 增加稳定实例 ID、快捷方式和绑定关系迁移。

### 阶段五：双向转换器

- 实现版本继承展开。
- 实现原版运行时描述和 Prism 组件生成。
- 实现兼容性预检查。
- 实现原版版本 JSON 写入。
- 实现转换报告、备份和回滚。

### 阶段六：客户端/服务端同步

- 实现 `SyncProfile`。
- 实现 mod 和配置文件声明。
- 实现单向和双向哈希同步。
- 实现冲突检测和预览。
- 接入客户端/服务端更新完成事件。
- 增加手动同步和可选自动同步。

### 阶段七：高级兼容（已完成首版）

- 标准 loader 的原版目录安装和导出。
- PCL/HMCL 扩展字段写入。
- 带内容基线的配置文件三方合并。
- 一对多客户端/服务端绑定。
- 标准 loader 组件白名单和缺失 patch 检查；后续可继续增加真实 loader 样本回归。

## 12. 测试和验收标准

需要准备以下目录夹具和测试：

- 多个 Prism Store 同时启用。
- 普通模式与便携模式切换且不移动实例。
- `<启动器目录>/UserData/instances` 的默认启用和手动启用。
- PCL/HMCL 共享布局。
- PCL/HMCL 版本隔离布局。
- 版本继承和自定义 loader JSON。
- 外部实例启动、刷新和 `luna.settings.json` 保存。
- 使用 HMCL 3.6.12 读取真实 PCL 游戏根后，Luna 扫描结果与目录关键文件保持一致。
- Prism ↔ Prism 单实例迁移。
- 原版 → Prism 转换。
- Prism → 原版的 MMC 阻塞检查。
- mods、config、saves、`.index` 的双向复制。
- 同名实例和 Store 重复路径。
- 客户端/服务端 mod 分类同步。
- 配置文件单侧修改、双侧修改和冲突。
- 实例运行中禁止迁移和同步。
- 转换或同步失败后的恢复和备份。

真实回归记录：系统现有 `C:\Users\qwe17\Downloads\HMCL-3.6.12.zip` 已在临时运行目录启动，配置指向 `D:\Projects\PCL-CE\build-pcl-compare\.minecraft`；HMCL 运行前后 `PCL.ini`、`launcher_profiles.json` 和版本 JSON 的 SHA-256 与大小均保持一致。Luna 的 `VanillaProviderSync_test` 在同一目录上通过，且识别出 PCL 方言。

外部安装验收记录：Release 安装树复制到 `C:\Users\qwe17\AppData\Local\Temp\LunaLauncher-acceptance-release-20261104-035952` 后，GUI 启动保持运行 5 秒；`tests/external_instance_store_smoke.py` 的 4 项验收全部通过，覆盖多 Store、PCL 扫描、原版导入、MMC 导出阻断、loader 安装、Store copy/move、便携模式状态和同步冲突。完整 `tests/api_sidecar_smoke.py` 为 20 passed、1 skipped（在线测试）、4 个旧测试夹具/API 形态不匹配；Release C++ 全量测试为 44 passed、0 failed。

验收要求：

- 模式切换不强制移动实例目录。
- 多个 Prism Store 可以同时显示和安装。
- PCL/HMCL 实例不需要 `instance.cfg` 或 `mmc-pack.json`。
- 普通 Minecraft 内容不会被错误判定为 MMC 专属数据。
- MMC 数据不会被静默丢弃或写入原版目录。
- 单个实例可以安全地在不同 Store 之间迁移。
- 客户端和服务端同步不会覆盖未声明的文件。
- 双方同时修改时不会无提示覆盖。
