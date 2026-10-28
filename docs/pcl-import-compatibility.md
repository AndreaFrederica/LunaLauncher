# PCL 整合包导入兼容性

本文记录 Luna Launcher 对 Plain Craft Launcher (PCL) 整合包的兼容边界。分析依据为 PCL 源码提交
`639de1b48a44326cbd5465579295cecf23d9056a` 和 `PCL-Modpack-Format-Spec.md`。

## 包格式

PCL 勾选“包含启动器”后导出的 `.zip` 是一个外层包装，核心内容仍是标准 Modrinth mrpack：

```text
outer.zip
├── Plain Craft Launcher.exe
├── PCL/                     # PCL 启动器全局个性化配置
└── modpack.mrpack           # 标准 Modrinth 整合包
```

Luna 支持在根目录或一级子目录中查找 `modpack.mrpack`、`modpack.zip`，只把这一个归档成员流式提取到临时文件，
验证其根目录含有 `modrinth.index.json` 后重新进入现有 Modrinth 导入流程。

PCL 也会直接导出扩展名为 `.zip` 或 `.mrpack` 的标准 Modrinth 包。此类包没有 PCL 外层结构，唯一可靠的 PCL
实例语义标记是 `overrides/PCL/Setup.ini` 或 `overrides/PCL/config.json`。Luna 始终先按 mrpack 导入，overrides
和索引文件全部落地后再探测并转换 PCL 配置，因此不能依赖压缩包文件名、外层启动器或 Logo 判断包来源。
添加整合包页面会把 PCL 标记为实验性支持；实际检测到外层 PCL 包装或实例内 PCL 配置后，导入成功提示还会明确
说明可能存在兼容性问题，并引导用户在首次启动前检查 PCL 兼容设置和 `lunaui/migration/pcl-report.json`。

PCL 还常见把整个游戏目录直接压缩的“普通 ZIP”，没有 Modrinth 或 MMC 清单。Luna 通过
`<前缀>/versions/<版本>/<版本>.json` 识别这类包；如果包含多个版本，会在导入时要求选择。选中的游戏目录会转换为
MMC 实例结构：`instance.cfg`、`mmc-pack.json`、`patches/net.minecraft.json` 和 `minecraft/`。版本目录中的自定义客户端
JAR 与包内 Maven 库会复制到实例本地库；未内嵌的库保留下载信息。原始版本 JSON 保存在
`pcl-import/original/`，转换明细保存在 `lunaui/migration/pcl-plain-report.json`。

ZIP 导入路径支持传统 ZipCrypto 密码，包括普通 PCL ZIP、MMC/Prism ZIP、Modrinth 包和 PCL 外层包装。Luna 检测到
加密成员后会显示密码输入框，并在正式解压前读取一个加密文件验证密码；密码不会写入实例或报告，也不会根据文件名硬编码
默认值。AES ZIP 是否可用取决于当前 libarchive 构建，无法解密时会作为导入错误返回。

旧式安装素材包（例如只有版本 JSON 和待合并 `.class` 文件、没有 `versions/<版本>/<版本>.jar` 的 MITE 安装包）不是
可启动的 PCL 实例。Luna 不会猜测基底 JAR 或自动修改 Minecraft JAR；这类包需要独立、可验证的补丁合并器，目前列为
不支持。

安全约束：

- 不解压、不检查、不执行外层 EXE。
- 不导入外层 `PCL/`，因为它属于启动器全局配置，可能包含个人数据。
- 只接受一层包装，内层包未解压大小上限为 8 GiB。
- 同时发现多个候选内层包时中止并明确报错。
- 内层 mrpack 原有的路径越界检查继续生效。

## 实例设置转换

mrpack 中的 `overrides/PCL/Setup.ini` 和可选的 `overrides/PCL/config.json` 会随 overrides 保留为实例的
`minecraft/PCL/`。Luna 在创建实例、完成 mrpack 文件下载后读取它们，应用能够保持语义的设置，并生成：

```text
<instance>/lunaui/
├── manifest.json
├── generated/pcl-compat.json
└── migration/pcl-report.json
```

生成文件含源文件 SHA-256 和生成器版本。PCL 中已有 Luna 原生对应项的字段会直接写入实例设置，并继续由 Luna 的
常规实例设置页面管理；`pcl-compat.json` 只显示迁移状态、缺失文件位置以及必须显式信任的 PCL 特有操作，不再重复提供
内存、Java、JVM 参数、服务器地址、窗口标题等编辑器。
`pcl-report.json` 逐项记录 `mapped`、`mapped-approximate`、`mapped-with-prompt`、`mapped-noop`、`ignored-cache`、
`unsupported` 或 `invalid`。原始 `Setup.ini` 不会被修改。

普通 ZIP 的 `pcl-plain-report.json` 还会使用 `mapped-local`、`mapped-instance-local`、`download-required`、
`download-unverified`、`repaired-download-metadata`、`mapped-platform` 和 `ignored-launcher-global`，以区分内嵌文件、
启动时需要下载的文件、无法验证的下载、只针对当前操作系统完成的参数转换以及未导入的 PCL 全局状态。

当前可靠映射：

| PCL 字段 | Luna 字段或行为 |
|---|---|
| `CustomInfo` | `notes` |
| `Logo` + `LogoCustom` | 自定义图片按内容哈希安装为 Luna 实例图标，同时限制路径必须位于游戏目录内 |
| PCL 内置 `Logo` URI | 仅使用独立 PCL 资源目录中完全同名的许可兼容资源；如 `Anvil.png` 使用 Lucide `anvil.svg`，不做语义映射 |
| `VersionAdvanceJvm` | 保存到 `JvmArgs`；引用的 Java agent 均随实例存在时启用 `OverrideJavaArgs` |
| `VersionAdvanceGame` | 追加到 `ExtraGameArgs` |
| `VersionAdvanceRun` + 等待模式 | 保留为独立的 Windows `cmd.exe` 步骤，须在生成的迁移页中明确信任后才启用；启用后追加在全局启动前命令之后 |
| `VersionServerEnter` | `JoinServerOnLaunch=true` + 地址 |
| `VersionRamType=2` | 跟随 Luna 全局内存设置 |
| `VersionRamType=1` + `VersionRamCustom` | 按 PCL 的分段公式换算为 MiB |
| `VersionRamType=0` | 改用 Luna 原生全局/默认内存策略 |
| `VersionArgumentJavaV2=0` | 使用 Luna 的全局/自动 Java 选择 |
| `VersionArgumentJavaV2=1` | 改用 Luna 原生自动 Java 选择；PCL 精确范围保留在报告中 |
| `VersionArgumentJavaV2=2` | 在实例游戏目录内查找 Java 候选但不启用，之后仍由 Luna 原生实例 Java 设置选择 |
| `VersionArgumentJavaV2=3` | 不导入来源机器的 Java 路径，继续使用 Luna 原生实例 Java 设置 |
| `config.json/InstanceForcedJava` | 仅在迁移报告中保留 PCL 选择的 Java 版本和来源路径，不导入来源机器路径；Java 仍由 Luna 原生实例设置管理 |
| `VersionArgumentTitle` | 实例窗口标题覆盖 |
| `VersionArgumentInfo` | Minecraft `version_type` 覆盖 |
| `VersionArgumentIndie*` | 无操作，Luna 实例天然隔离 |
| `VersionAdvanceDisableJLW/LUA` | 无操作，Luna 不使用对应 PCL 补丁 |

`VersionAdvanceJvm`、`VersionAdvanceGame`、`VersionArgumentTitle`、`VersionArgumentInfo` 仅在值不含 PCL
运行时替换标记时自动映射。诸如 `{user}`、`{date}`、`{setup:...}` 的值依赖账户、时间或 PCL 本地状态，
导入时不能正确求值，因此会保留在迁移报告中要求人工处理，不会把未展开的字面量写入启动设置。

`VersionAdvanceJvm` 中的 `-javaagent:` 相对路径还会以实例游戏目录为基准检查。若文件不存在、越出实例目录，
或使用不可移植的绝对路径，Luna 会保留原始 JVM 参数但关闭 `OverrideJavaArgs`，并在生成的迁移页与迁移报告中
列出缺失项和放置规则。相对路径须按原目录结构放入游戏目录，例如 `GraphicsFixer.jar` 应放在
`<实例>/minecraft/GraphicsFixer.jar`，`agents/a.jar` 应放在 `<实例>/minecraft/agents/a.jar`；绝对路径或越界路径
须先改为实例内路径。对安全的相对路径，生成的迁移页会提供按钮，通过受实例目录限制的 `openFolder` 动作创建并打开
对应放置目录；文件就位后在 Luna 原生实例 Java/JVM 参数设置中启用。`Setup.ini` 只记录参数，不包含下载 URL；Luna
不会为缺失的可执行 JAR 猜测下载来源。

PCL 命令变量映射为：

```text
{verpath}       -> ${INST_DIR}
{version_path}  -> ${INST_DIR}
{verindie}      -> ${INST_MC_DIR}
{version_indie} -> ${INST_MC_DIR}
{minecraft}     -> ${INST_MC_DIR}
{java}java.exe / {java}javaw.exe -> ${INST_JAVA}
{name}          -> ${INST_NAME}
```

`VersionAdvanceRunWait=False` 仍列为不支持，因为 Luna 的启动任务必须等待启动前步骤结束。非 Windows 系统也不会运行
PCL 的 `cmd.exe` 命令。等待型命令和整合包内置 Java 都属于可执行内容，导入时默认禁用；启动前命令可在生成的迁移页
显式确认，内置 Java 则只能通过 Luna 原生实例 Java 设置选择。Luna 使用独立追加步骤而不是 `OverrideCommands`，因此
不会替换用户的全局启动前命令。

普通 ZIP 内嵌的 assets 保存在实例根目录 `assets/`，并由 `UseLocalAssets=true` 隔离使用，避免导入时覆盖 Luna 的全局
assets。MMC ZIP 导出器会包含该目录，但原版 Prism/MultiMC 不识别 `UseLocalAssets`，所以此项只能保证数据随包保留，
不能保证由原版 Prism/MultiMC 直接启动时使用相同资源；该限制会保留在兼容性报告中。

若普通 ZIP 内含的 `legacy` 索引与 Mojang 官方 1.6.4 索引完全一致，但没有带齐索引引用的对象，Luna 不会把这个目录
误判为完整的本地资源库，而是关闭 `UseLocalAssets`，让已有的标准资源更新流程补齐文件。自定义、损坏或缺失的索引无法
安全地映射到 Mojang 下载源，仍保留为实例本地资源，并在报告中列出无法验证或缺少的部分。

## MITE 与 FishModLoader

普通 ZIP 的版本 JSON 同时满足以下条件时，Luna 会把它识别为 MITE/FishModLoader 实例，而不是普通 Fabric 或
Legacy Fabric 实例：主类为 `net.xiaoyu233.fml.relaunch.client.Main`，且库列表包含
`net.xiaoyu233.fishmodloader:fishmodloader:<版本>`。FishModLoader 虽然内嵌了 Fabric Loader 与 Mixin 的衍生实现，
但它不是可替换为标准 `net.fabricmc.fabric-loader` 组件的普通 Legacy Fabric 加载器。

转换后组件按启动覆盖顺序拆分为：

1. `net.minecraft`：使用 `Setup.ini` 的 `VersionOriginal`（例如 1.6.4）接入 Luna/Prism 的标准 Minecraft 元数据与下载。
2. `org.lunalauncher.mite`：保留整合包提供的已修改 Minecraft 核心 JAR 和旧式游戏参数；MITE 是核心替换，不只是加载器。
3. `net.xiaoyu233.fishmodloader`：保留 FishModLoader 入口类、运行库、Mixin/Fabric 运行时字段和 Java 约束。

若版本 JSON 没有 `javaVersion`，Luna 会读取实际启动主类的 class 文件版本。例如 class major 61 会生成 Java 17 约束，
不会沿用 Minecraft 1.6.4 通常使用的 Java 8。无法同时确认主类和 Maven 坐标时不做专用拆分，继续使用普通 ZIP 的单组件
兼容路径，避免把其他自定义核心误识别成 MITE。

PCL Logo 转换遵循 MMC/Prism 的普通自定义实例图标机制：导入时按图像内容生成稳定的 `iconKey`，把图片作为
文件型图标安装，而不是让实例继续引用 PCL 的 WPF `pack://` URI。之后从 Luna 导出 MMC zip 时，现有导出器会把
`<iconKey>.<扩展名>` 写入实例根目录；原版 Prism/MultiMC 可按 `instance.cfg` 的 `iconKey` 重新导入。PCL-CE 中许可
兼容的 Lucide 资源完整保存在 `launcher/resources/pcl/lucide/`，但仅 `Anvil`、`Egg` 与当前 PCL Block Logo 真正同名；
`Fabric`、`CobbleStone` 等不会被替换成含义相近但图案不同的 Luna 图标，并会列为 `unsupported`。
使用内置 Lucide 图标时，完整的 ISC/Feather MIT 文本也会以带内容哈希的文件名写入实例根目录，随 MMC zip 一同导出。

## 已有功能但需要用户选择

以下能力 Luna 已经具备，但不能仅凭 PCL 配置安全地自动绑定，所以迁移报告会要求用户确认：

- `VersionServerLogin`：Luna 已有 Microsoft、离线、统一通行证、Yggdrasil/Authlib Injector 账户体系，
  但导入包不能代替用户登录或猜测应绑定的本地账户。`VersionServerNide`、`VersionServerAuthServer`、
  `VersionServerAuthRegister`、`VersionServerAuthName` 会作为添加/选择对应账户时的参考信息写入迁移报告。
- `VersionArgumentJavaV2=1/3`、`VersionArgumentJavaRange`：Luna 会继续使用原生自动或实例 Java 设置，
  PCL 的精确补丁范围与其本地 `VersionArgumentJavaSelect` 记录仅作为迁移参考，不能跨机器无损映射。
- `VersionAdvanceDisableModUpdate`：Luna 的 Mod 更新本来就是用户主动操作；若要求严格禁止更新，仍需新增实例级 UI 锁。
- `DisplayType`：可近似映射到实例分组，但 PCL 的隐藏/分类语义与 Luna 分组不等价。

## 不能等价映射

- `VersionAdvanceAssets`、`VersionAdvanceAssetsV2`：禁用资源、库、客户端 JAR 校验会削弱 Luna 的完整性保证。
- `VersionRamOptimize=1`：PCL 的 Windows 进程工作集清理没有 Luna 对应项。
- `IsStar`：Luna 当前没有实例收藏状态，且 PCL 导出时会写为 `False`。
- `VersionAdvanceGC`：PCL 预设依赖运行时 Java 版本；在未完成等价预设表和验证前不猜测 JVM 参数。
- 外层 `PCL/Custom.xaml`：它是可执行语义的 WPF UI，不能安全、通用地转换为声明式 LunaUI。
- 外层 `Pictures`、`Musics`、`Help`、`hints.txt` 等启动器个性化内容：属于 PCL 全局界面，不属于实例。

以下是 PCL 缓存字段，Luna 忽略并以 mrpack/组件元数据为准：`State`、`Info`、`ReleaseTime`、
`VersionFabric`、`VersionForge`、`VersionNeoForge`、`VersionOptiFine`、`VersionLiteLoader`、
`VersionVanilla`、`VersionVanillaName`。

普通 ZIP 根目录中的 `PCL/` 和 `PCL.ini` 也属于 PCL 启动器的全局缓存或个性化状态，不会复制到实例。只有选中
版本目录下的 `PCL/Setup.ini`、`PCL/config.json`、Logo 和其他实例文件会合并到 `minecraft/PCL/` 后参与转换。

## LunaUI 扩展

LunaUI 现在支持 `input`/`lineedit`、`textarea`、`number`/`spinbox`、`accountselect` 控件，以及：

- 控件字段 `sourceSetting`，用于从已注册的实例设置读取当前值。
- 动作 `setInstanceSetting`，用于把控件值写回已注册的实例设置。
- JS API `launcher.getInstanceSetting(name)` 和 `launcher.setInstanceSetting(name, value)`。

写入仅允许已经由实例注册的设置名，不能用 LunaUI 临时创建任意设置。
