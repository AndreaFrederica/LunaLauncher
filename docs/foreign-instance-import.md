# 原版目录包与自定义客户端 JAR 的共用导入

## 范围

PCL/HMCL 打包的 `.minecraft/versions/<实例 ID>/<实例 ID>.json` 目录包，与磁盘上已有的原版目录使用相同解析、复制和转换流程。PCL/HMCL 配置是附加信息，不构成另一种 Minecraft 存储格式。

MITE、FishModLoader、GTNH 等只作为兼容样例。保留自定义客户端 JAR 的能力不能依赖这些名称、主类或库坐标。

## 分开记录的身份

| 信息 | 含义 | 示例 |
| --- | --- | --- |
| profileId | 外部实例的目录与 JSON 身份 | `1.6.4-MITE`、`GTNH-2.9.0-B-V1.2.1` |
| minecraftVersion | 基础 Minecraft 版本 | `1.6.4`、`1.7.10` |
| clientArtifact | 实际启动的完整客户端 JAR、大小和哈希 | 已修改的核心或包内原版客户端 |
| loader | 由运行配置识别的加载器及版本 | FishModLoader、Forge |
| runtime | 主类、游戏/JVM 参数、Java 要求与运行库 | HMCL bootstrap、RFB、Java 17/25 |

优先使用明确的基础版本证据：HMCL game patch、FML 参数、Forge Maven 坐标、继承关系、PCL `VersionOriginal`。实例 ID 仅作最后的兼容回退，不能据此断言客户端是官方原版。

## 客户端保留契约

1. 包内或源目录已有的完整客户端 JAR 是本次导入的权威输入，不因其与官方哈希不同而替换。
2. 记录源文件 SHA-256、SHA-1、字节数和 JSON 声明的 SHA-1。比较结果只表示“符合声明”“不同于声明”或“未经验证”，不代表哈希声明本身来自官方。
3. 复制后重新计算哈希；不一致即中止提交，保留源实例。
4. 转换为 MMC 时，客户端成为实例本地的 `mainJar`，附带复制结果的 SHA-1。缺失或损坏时报告错误，不回退下载官方 JAR。
5. 导入为原版格式时保留版本 JSON、库、资源及客户端 JAR。改名后同步调整实例 ID 与自己的 JAR 引用。默认隔离，HMCL/PCL/Luna 的目录设置应一致。
6. 外部原版实例启动、依赖补全和离线检查使用相同的客户端路径。已保留客户端缺失或损坏时停止修复，不能用基础版本的官方客户端悄悄替代。
7. 原始 JSON、导入记录和源哈希保留在目标内，便于审查与恢复；不修改源目录，不向全局官方客户端缓存写入修改版核心。

“已 patch 的完整 JAR”与“待应用补丁”分开处理。完整 JAR 直接保留；旧式 `class` 覆盖安装包通过下述预处理生成 JAR 后复用同一流程。未知二进制差分仍不支持，不执行包内安装程序或脚本。

## 旧式 class 覆盖安装包

- 自动发现 `<版本>/<版本>.json` 与同目录 `class/**/*.class`；不依赖 MITE 名称，不扫描资源、模组和库目录里的伪配置。已有同名完整客户端 JAR 时优先使用完整实例路径。
- 基础版本优先使用 JSON 的 `inheritsFrom`、`minecraftVersion`、`jar` 声明；无声明时只接受明确的数字发行版本前缀（例如 `1.6.4-MITE`），并要求该版本能在启动器元数据中解析。声明冲突、配置不完整时给出具体错误。
- 下载基础客户端时必须有元数据提供的 SHA-1 与字节数；下载层和合并前各校验一次。基底下载只写入导入临时目录。
- 将 `class` 内容打成根目录正确的 Jar Mod，复用 Prism 的 `MMCZip::createModdedJar`，移除基底签名。整理、合并、校验均在文件工作线程执行，提供逐文件进度和取消能力。
- 合并后逐项核对所有覆盖文件的 SHA-256，检查启动主类存在。保留补丁 JAR、原始 JSON、基底及结果哈希和覆盖文件清单于实例的 `luna-installer/`。
- 同目录含 `pack.mcmeta` 的资源包 ZIP 自动复制到隔离实例的 `resourcepacks/`。包内 EXE、脚本不进入最终实例，也不会被执行。
- 生成标准 `versions/<版本>/<版本>.json/.jar` 临时布局后，继续走原有 MMC/原版目标选择及事务转换，保持默认隔离；自定义客户端继续受防覆盖校验保护。

### class 覆盖包导入验证（2026-11-17）

通过 `pixi run install-webui release` 构建部署，使用 `tests/legacy_jarmod_import_smoke.py` 调用部署后的 CLI，在独立临时数据目录中验证：

- 实际 `MITE 1.6.4 R196 (1).zip` 分别导入 MMC 和原版目标成功，均识别为 Minecraft 1.6.4；1,186 个覆盖条目与源包逐项一致，未覆盖的原版 JAR 内容逐项一致，签名目录已移除，资源包和原始 JSON 正确保留，源 ZIP 未变。
- MMC 配置使用 Java 8；原版目标保持隔离布局、不含 MMC 控制文件，且不会为修改后的客户端保留官方替换下载地址。
- 不同名称及 `versions/` 包装的模拟包复用相同识别流程；基础版本冲突、无法确定基底、无效 class 和空校验值均明确失败，不提交半成品实例。下载基础客户端时取消会等待临时目录清理后结束。
- `tests/verify_foreign_import.py` 的已有格式/目录选择、取消及自定义客户端防覆盖回归通过。测试日志位于本地 `build-meson-debug/legacy-class-installer-final-test.log`。本次验证范围为导入及产物完整性，未启动游戏。

## 共用流程

1. 识别正式整合包清单或原版目录候选；过滤 assets/libraries/mods 等资源目录里的伪版本 JSON。
2. 交互导入时选择目标格式，再选择同格式且启用、可写的存储位置。多个匹配位置每次都选择；只有一个时显示实际目标。无原版位置则提示先配置，不自动创建原版存储、不改变已有位置格式。
3. 在临时目录解压并解析继承链、HMCL patch 栈、PCL 配置和实际运行文件。
4. 共用客户端保留、Java 检测与库解析。Java 未声明时检查实际主类字节码，不能只根据基础 Minecraft 版本选择 Java。
5. 写入所选格式并验证，再提交实例。原版目标不经过 MMC 往返转换，不写入 `instance.cfg`、`mmc-pack.json` 或 Prism `patches`。

MMC 转换保留完整自定义运行配置，加载器身份与运行配置分离。不再要求为了保留 MITE 的核心而创建 MITE 专有的核心替换流程。

## 资源与平台参数

保留库的 Maven 路径、下载 URL、校验信息和 native classifiers。优先使用经过校验的包内文件；自定义本地库缺失时明确报告，不能默认从 Mojang 仓库推导出有效地址。

转换 MMC 的参数按当前平台规则处理并去除启动器自己生成的 classpath/native 参数；游戏参数保留必要引号。原始 JSON 中的条件规则保留在备份中。原版导入保持原始条件规则，供目标平台启动器解释。

官方资源索引不完整时走标准补全；自定义资源不能仅因目录存在就宣称完整，也不能无依据替换为官方索引。

## 验证要求

- 不含 MITE/FishModLoader 特征的修改版客户端同样保留、转换与启动路径选择正确。
- JSON 声明官方下载时，本地修改版仍优先且复制哈希相同。
- 本地客户端删除或篡改后，依赖更新明确失败且不发起替换客户端的下载。
- 原版格式改名、隔离、继承链与共享库冲突处理正确；源文件不变。
- MITE 的基础版本为 1.6.4，FishModLoader 的 Java 17 要求来自实际 class；GTNH 基础版本为 1.7.10，Java 25、HMCL bootstrap、RFB、资产索引和组合 native JAR 保留。
- 无匹配存储、格式不匹配与取消选择不会写入实例；多个匹配存储每次询问。
- 导入和元数据测试通过不等于游戏启动成功；实际启动与 HMCL 互操作验证需另行记录。

## 2026-11-16 验证记录

使用 `pixi run install-webui release` 构建、安装，测试数据通过独立临时用户目录与存储位置隔离，不修改源整合包。

- 六组定向单元测试通过：`ImportedClientProfile_test`、`PCLPack_test`、`Library_test`、`VanillaInstanceLayout_test`、`VanillaProviderSync_test`、`VanillaInstall_test`。覆盖通用修改版客户端、哈希校验、继承缺失/循环、父版本改名冲突、目录布局与外部运行配置。
- `tests/verify_foreign_import.py` 通过部署后的 CLI 验证：每次格式/目录选择、无匹配存储拒绝、格式冲突拒绝、取消不写入、空原版位置创建 `.minecraft/versions`、默认隔离，以及原版与 MMC 客户端删除/篡改后拒绝替换下载。
- 从真实 MITE R196 v1.3.0 与 GTNH 2.9.0 HMCL 完整包中提取版本 JSON、客户端和选定的运行库生成精简测试包，两种目标格式均通过导入。源 JSON 备份与客户端 SHA-256 一致；MITE 识别为 MC 1.6.4 / Java 17，GTNH 为 MC 1.7.10 / Java 25。
- MITE 原版目标的活动 JSON 补入从入口字节码推断的 Java 17，原始 JSON 保留。其不完整的官方 legacy assets 不再启用 `UseLocalAssets` 跳过资源补全。
- 未声明 Java 的正式旧版（Minecraft 1.16 及之前）在内部运行配置中保留 Java 8 默认值，避免落回全局 Java 21/25；显式声明及入口检测出的 Java 要求优先。
- GTNH 保留 HMCL bootstrap、RFB JVM 参数与原平台条件；MMC 目标的 Windows native 包仍为 16,483,083 字节、23 个 DLL，没有替换成单个 LWJGL 核心 native JAR。
- `pixi run webui-check` 类型检查通过。
- `tests/verify_library_download_patches.py` 回归通过：Qt/PCL 下载后端的默认关闭、手动启用、URL 回退、错误内容校验、缓存复用及包内 native 优先逻辑正常。

### GTNH 完整 ZIP 与 Neo 交互复核

使用原始 `GTNH-2.9.0-流程毁灭私货直装包-V1.2.1-HMCL完整包.zip`（1,371,159,348 字节），通过部署后的 `neo.task.start` / `neo.task.status` / `neo.task.respond` 分别导入 MMC 与原版目标，未使用精简测试包。报告位于本地 `build-meson-debug/full-gtnh-20261116-004848/report.json`。

- 两种格式均成功提交；每种均逐文件比较了 21,711 个文件的 SHA-256，共 1,575,295,617 字节。范围包括包内全部 libraries/assets，以及真实版本目录中的 mods/config/saves/resourcepacks/shaderpacks/scripts/resources；客户端另行核对哈希，原始 profile JSON 备份一致。
- 两种格式均识别为 Minecraft 1.7.10，保留 Java 25、HMCL bootstrap 入口和原客户端；没有把资源目录里的伪 versions 当作可导入实例。
- 原版目标在空位置创建 `.minecraft/versions/<实例 ID>`，隔离设置正确，没有 MMC 控制文件，实例列表仅识别出一个真实实例。
- 两次导入均正确先请求目标格式，再请求对应格式的存储位置。测试设置了多个可选位置，返回的选项包含位置名称、路径与格式。Neo 共用交互弹窗消费这些请求，Endfield 使用自身的交互弹窗；没有新增独立导入向导页。本次验证覆盖任务接口及前端代码连接，未进行弹窗的视觉操作验证。
- MMC 导入及文件核验合计 104.46 秒，原版合计 116.88 秒。该次复核暴露了解压后权限整理、同步复制与转换占用后台 Qt 主线程的问题，单次状态请求最长等待 49.25 秒；当时进度查询与取消请求无法及时处理。后续修复及复测见下节。

### 大包进度与响应性修复

- 权限整理、MMC 转换、原版直导和解压临时目录清理放到文件处理线程；应用模型、实例登记和 Qt Task 信号仍在启动器主线程处理。
- 转换器提供实际工作量回调。扫描时显示已发现文件数，权限整理显示文件数/总数，实例内容复制按字节推进，原版共享运行库/资源复制及冲突校验按文件数推进。复制以 1 MiB 缓冲区处理，大文件也可更新进度并检查取消。
- 进度以 100 ms 间隔合并发布，避免大量小文件挤满主线程事件队列。删除旧的定时模拟阶段进度；未知总量的扫描、配置处理和清理阶段显示活动进度条，不伪造百分比。新旧 UI 复用 Task 进度；Neo PCL、XMCL、Endfield 的任务面板补充活动进度、阶段、数量或文件详情。
- 取消复制时清理尚未提交的临时实例，保留源文件。异步任务等待工作线程完成清理后才结束，避免临时目录提前销毁。
- 六组定向单元测试通过，新增覆盖 MMC/原版两种目标在文件复制中途取消、源文件不变、临时目标清理，以及工作线程的进度更新可被主线程事件循环接收。`tests/verify_foreign_import.py` 的格式/目录选择、取消及客户端保护回归通过。
- `tests/verify_import_progress_cancellation.py` 通过部署后的 Neo API 在复制阶段取消 MMC/原版导入：取消请求及时返回，任务等待临时目录清理完成才结束，目标实例未提交且源 ZIP 不变。导入/转换取消期间保留存储锁，避免下一任务在旧文件线程退出前开始写入。
- `tests/verify_full_directory_import.py` 对同一原始完整 GTNH ZIP 复测两种目标，每种仍有 21,711 个内容文件的 SHA-256 一致。MMC 记录到 118 次复制中间进度，原版记录到 110 次；状态请求最长等待 **0.03 秒**。导入及文件核验合计分别为 80.54 秒、92.92 秒，耗时受缓存和磁盘状态影响。报告位于 `build-meson-debug/full-gtnh-20261116-010647/report.json`。
- 最终通过 `pixi run install-webui release` 构建、部署至 `install-webui-authfix-release`。前端严格类型检查和构建通过；核对了 staging 中的任务面板、导入弹窗与当前源码一致，并登记 Endfield 面板/下载页的构建输入，确保后续修改触发重打包。

完整整合包的 MMC 实际启动验证见下节；HMCL 实际启动互操作及新旧 UI 导入弹窗的视觉验证仍未完成。补丁素材包按前述旧式 class 覆盖规则处理；未知补丁格式、无法确定基底或基底缺少完整性信息的包仍明确失败。

### MMC 运行配置逐项复核与 JVM 参数修复

对 Forge 组件与实际启动参数继续核对时，发现结构化 JVM 参数转换曾逐 token 去重，导致 GTNH 源配置中的 34 个 `--add-opens` 仅剩一个，后续模块参数失去对应开关。文件复制哈希检查无法发现这一问题；此前保留了库和配置字段的结论不能替代参数序列验证。

已取消 JVM token 去重，按源顺序保留重复开关及操作数；classpath、natives 和启动器品牌仍由启动器生成。新增单测验证重复 `--add-opens` / `--add-exports` 经导入、MMC 解析和 LaunchProfile 合并后序列不变，三组相关单元测试通过。

通过 `pixi run install-webui release` 重新部署到 `install-webui-authfix-release`，并用完整 ZIP 重新导入 MMC：

- `instance.components.list` 离线加载成功，`net.minecraft` 为 `1.7.10`，`net.minecraftforge` 为 `10.13.4.1614-1.7.10`；两个自定义组件均启用且没有组件问题。Forge 的基础构建号为 `10.13.4.1614`，末尾 `-1.7.10` 保留自源包库坐标。
- 所有重复 JVM 开关/操作数对与源 JSON 一致，34 个 `--add-opens` 全部保留；Java 25、HMCL bootstrap 和 RFB 属性一致。游戏参数逐 token 比较一致，MMC 扁平本地运行库额外逐文件核对源库 SHA-256。
- 21,711 个内容文件 SHA-256 一致。运行配置和进度复核报告位于 `build-meson-debug/full-gtnh-20261116-021217/report.json`，执行日志为 `build-meson-debug/import-runtime-full-gtnh.log`。
- 既有导入实例的自定义 patch 不会因启动器更新而自动重写；要得到修正后的参数，应重新导入源包到新实例。

### GTNH 完整 MMC 实例实际启动验证（2026-11-16）

使用上述重新导入的 `GTNH-full-mmc-a`，通过部署后的 `lunalauncher-cli.exe` 启动，测试数据位于独立目录 `build-meson-debug/full-gtnh-20261116-021217/data`。仅为测试配置本机已有的 Zulu Java `25.0.3`（`D:/jvm/zulu25/bin/javaw.exe`）、最大堆 8192 MiB，以 `LunaLaunchTest` 离线身份启动，没有修改用户实际配置或源 ZIP。

- 02:19:28 开始启动，RFB / HMCL bootstrap、Forge、LWJGL3 和 Intel OpenGL 初始化成功；02:23:37 日志记录 `Forge Mod Loader has successfully loaded 366 mods`。
- 约 02:24:41 实际观察到 GTNH 2.9.0 主菜单，菜单显示 Minecraft `1.7.10`、Forge `10.13.4.1614`、`Mods (366/366)`。首次加载约 5 分钟，判定依据包括游戏窗口画面，未将 Java 进程启动视为通过。
- 点击主菜单 Quit 正常退出，02:24:58 生命周期事件返回 `exitCode: 0`、`succeeded: true`、`aborted: false`。
- 日志仍有模组警告及 Draconic Evolution 远程图片下载异常，但本次未阻断到达主菜单。未据此改动整合包内容，也不将主菜单通过解释为存档、联机或长期运行无问题。
- 完整证据保存在 `build-meson-debug/full-gtnh-20261116-021217/launch-20261116-021927/`：`verification.json`、逐事件 `events.jsonl`、完整捕获的 `console.log`、正常退出事件 `finished.json`，以及游戏日志快照。退出事件中的控制台为有界尾部，完整输出应查看独立 `console.log` 和游戏日志。

本次没有新增代码修复或重新构建，实际使用上节已通过 Pixi 部署的 `install-webui-authfix-release`；启动验证脚本通过 `pixi run python build-meson-debug/launch_full_gtnh.py` 执行。HMCL 对原版目标的实际启动互操作尚待单独验证。
