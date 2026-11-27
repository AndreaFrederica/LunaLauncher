# 启动器自动更新

Windows、Linux AppImage 与 Linux 便携目录包使用各自的更新协议 1。系统包仍由对应的包管理器更新，不使用 Windows 安装器或上游 Prism 更新源。

## Windows

- Classic 与 NeoUI 共用外部更新器，统一下载新单文件 `*-Setup.exe`。
- 标准安装、便携安装和自定义数据目录更新到**当前程序目录**，不重新执行安装向导、HMCL/PCL 数据迁移或实例目录注册。
- `.luna-install.json` 在每个程序目录保存安装清单和界面选择；旧版本没有本地记录时，仅当全局安装记录的路径匹配当前安装时才使用它。当前数据路径标记优先于历史记录。
- Release 必须包含 `windows-x64-update.json`（或 `windows-arm64-update.json`）。清单记录协议版本、二进制实际版本、架构、是否带 NeoUI、安装包名称、大小和 SHA-256。
- 只选择与架构、界面能力匹配的 Setup EXE；校验文件、ZIP、其他系统附件不会成为安装器。带 NeoUI 的安装不会更新成缺少 NeoUI 的包。
- 版本比较保留日期和同日修订号，去掉 tag 的 `v` 前缀。检查时选择有兼容清单的最新版本，正式版与预发布遵守用户通道设置。降级需要显式允许。
- 用户点击更新后，Classic 正常退出，NeoUI 由 Tauri 统一退出并回收 CLI。外部更新器显示下载进度，安装器显示等待退出、解压、替换、验证和重启进度。
- NeoUI 主界面按共享开关和检查间隔定时检查，跳过首次设置、运行游戏和界面忙碌阶段；发现新版通知用户，安装仍由用户点击触发。网络失败延迟五分钟重试。
- 工作进程位于临时目录，只等待当前程序目录内的进程退出，不执行全局 `taskkill`。有游戏运行时不交接更新。
- 在同盘临时目录完整校验并解压后，旧目录改名保留；保留 `UserData`、数据路径标记，以及不与新版程序冲突的未知文件（冲突文件留在旧备份），删除旧安装清单中已被新版移除的程序文件。更新安装器、安装记录与 Windows 当前用户的卸载、URL/文件关联注册；不重新建立快捷方式。
- 便携布局若从程序目录旁的 Setup.exe 启动，更新会在验证新程序后按原文件名替换该入口。旧安装可通过当前或备份中的安装器识别外部入口，识别到唯一匹配时才会替换。外部入口被占用导致替换失败时，恢复原程序目录。
- 新程序必须在 15 秒内通过 `--version` 检查。替换或验证等可捕获错误会恢复原目录及用户数据；成功后在旁边保留 `.luna-update-backup-*`，路径写入 `.luna-update.json` 与更新日志。
- 重启保持发起更新的 Classic/NeoUI 以及原有数据目录（Classic 使用 `--dir`，NeoUI 使用其原生支持的 `LUNA_LAUNCHER_DATA`）。日志为数据目录下的 `logs/prism_launcher_update.log`，失败另写 `.prism_launcher_update.fail`。

### 发布

用户手动打开新版 Windows Setup 时，安装器会离线比较包内记录的版本和已安装的 `lunalauncher.exe --version`。发现新版会显示新旧版本和安装路径，确认后进入更新进度窗口。安装器可识别登记过的安装、默认安装目录和位于 Setup 旁边且有安装记录的便携目录，也可在向导里选择现有安装。手动打开 Setup 不需要 GitHub 更新清单。

本地 `scripts/pixi_sfx.py` 与 GitHub Windows 打包任务都调用 `scripts/windows_update_manifest.py`，从实际发货的 `lunalauncher.exe --version` 读取版本，并计算最终 Setup EXE 的 SHA-256。清单必须与对应 EXE 一同上传 Release。

CI 将 `v11.0.2`、`v11.0.2-20261118`、`v11.0.2-20261118r1` 归为正式版本，其他后缀默认预发布；手动发布仍遵守用户选择。日期后的 `rN` 会传入 `LUNA_RELEASE_REVISION`，确保同日修订实际进入二进制版本。

旧 Release 没有协议 1 清单时不提供自动安装，需手动安装一次新版以进入新更新链路。同日二次发版应提升 `release_revision`，避免二进制版本相同而无法识别为更新。

### 验证与边界

自动测试覆盖文件替换、旧文件移除、Unicode 路径、用户数据与自定义标记保留、安装记录隔离、失败回滚、损坏/越界/带用户数据的包拒绝、架构/协议/界面过滤和日期/修订比较。测试使用临时目录，Windows 注册测试使用隔离注册表分支。

Windows 验证入口：`pixi run installer-shell-test`；Meson 的 `WindowsUpdateManifest_test`；`tests/windows_update_smoke.py` 使用真实 Release SFX 在临时便携目录验证替换、校验和回滚；`tests/windows_update_handoff_smoke.py` 在临时安装内使用真实 NeoUI 和 MCP，替换外部更新器为无副作用程序以验证交接响应、原生退出和 CLI 回收。完整在线升级仍需发布带清单的新 Release 后验证。

当前 SHA-256 以 HTTPS GitHub Release 清单为可信来源，尚未引入独立离线签名。`--version` 验证不等于完整 UI 启动健康检查。断电/强制结束工作进程后的自动恢复、备份保留数量及临时目录自动清理仍需后续完善；保留备份可用于人工恢复。成功后的备份只保留旧程序，`UserData` 已移回当前目录，人工恢复时必须保留当前用户数据。旧安装若没有文件清单，会保守保留未知文件。外部游戏/实例数据不会为更新被复制或清理。

## Linux AppImage

### 打包入口

在原生 Linux x86_64 或 WSL2 内使用单独的 Linux 源码/构建环境，避免与 Windows 共用 `.pixi/envs/default`。宿主机需要 GTK3、WebKitGTK 4.1 等开发依赖；默认 Classic 包中的 CurseForge 下载窗口同样使用 WebKit。

```sh
pixi run package-appimage
# 同时包含 NeoUI 和 Classic
pixi run package-appimage-webui
```

任务依次通过 Pixi 配置、编译、Meson 安装，再执行 `scripts/pixi_appimage.py`。打包输入是同一 profile 的安装树；默认 4 个构建任务，并限制 C++ 链接并发为 1；AppImage 任务启用预编译头，`LUNA_BUILD_JOBS` 可显式调整任务数。Cargo 默认 1 个任务，可单独用 `CARGO_BUILD_JOBS` 调整。普通配置可用 `LUNA_USE_PCH=true` 启用同一选项。打包脚本不运行全局 Meson，也不修改原安装树。

- `scripts/appimage-tools.json` 固定 linuxdeploy、Qt/GTK 部署插件、appimagetool 和 type-2 runtime 的下载地址与 SHA-256。工具校验失败会停止，不自动接受新版本。`continuous` 工具更新也需要手动审核并更新锁文件。
- `AppDir/usr` 保存应用、Qt 及 sidecar 运行库，根目录使用自己的 AppRun、desktop 文件和 Luna 图标。排除实例、账号、UserData 和安装状态标记，并去除静态 fallback 依赖安装的 SDK 头文件、静态库和 CMake/pkg-config 开发文件。
- AppRun 自动选择 NeoUI（带 NeoUI 时），`--version`/CLI 参数使用 Classic；可用 `--luna-entry classic|neo` 显式选择。更新器以 `--luna-entry updater` 启动，取得独立的 runtime 挂载。
- 按实际安装的 NeoUI/CurseForge 下载窗口部署宿主 GTK SDK 的资源、GLib schemas、WebKitGTK 辅助进程及其依赖，避免误读 Pixi GLib SDK 的模块目录；两种 WebKit 入口都加载 GTK hook，并使用部署插件要求的工作目录。CurseForge 辅助程序通过包装脚本启动，避免从实例数据目录启动时找不到 WebKit 子进程。Qt 的 OpenSSL 动态加载库、无窗口健康检查需要的 offscreen 平台插件及锁定的 C++ 运行库也显式打包。
- 完成依赖部署后实际执行打包产物的 `--version`。产物为 `release-assets/LunaLauncher-Linux-x64-<实际版本>[-webui].AppImage`、SHA-256 文件，以及 `linux-x64-appimage-update.json`。清单从实际程序读取版本；三者须一起发布。
- Linux CI 使用 Ubuntu 22.04 作为构建基线。AppImage 仍依赖宿主内核、glibc、图形驱动和桌面服务，打包不代表可以运行在任意旧发行版。旧 sharun/zsync action 仅供已停用的历史工作流参考，当前发布入口使用上述 Pixi 脚本。
- 无 FUSE 时可使用 runtime 的 `--appimage-extract-and-run`。更新校验始终使用此模式，避免新包的验证依赖 FUSE。

### 更新行为

Classic 和 NeoUI 共用外部更新器。AppImage 更新协议只替换处于 AppRun 环境中的实际 AppImage；带明确清单的便携 tar.gz 使用下述独立协议。系统包和沙盒分发不会改写应用目录。

更新器按架构、协议和 NeoUI 能力筛选专用清单，复用日期/修订号比较与下载进度。下载后检查大小、SHA-256、ELF 架构与 type-2 标识。用户停止游戏和后台任务后才交接更新。

在 AppImage **同目录**建立独占更新锁并写入临时文件，复制完成后再次校验，设置执行权限并 fsync；新包先通过版本健康检查，再为旧文件建立唯一备份，使用 POSIX rename 原子替换。新路径上的健康检查失败时还原旧文件。成功保留 `<原文件名>.backup-<UUID>`，在更新日志和成功标记记录备份路径。符号链接、多个硬链接、只读文件或不可写目录会拒绝更新，不尝试提权。

仅替换 AppImage 文件，保留原路径和 `.home`/`.config` 便携数据目录；使用原数据目录和当前 Classic/NeoUI 界面重启。启动新 runtime 前清理旧挂载的库和 Qt 插件环境，避免继续读取旧版库。

### 验证入口与边界

```sh
LUNA_BUILD_TESTING=true pixi run configure release
pixi run test-appimage-update
# 额外执行真实 AppImage 运行、原子替换、备份可运行及 .home/.config 数据保留验证：
pixi run python scripts/appimage_smoke.py release-assets/<产物名称>.AppImage
# 实际 AppImage 的 MCP 更新状态、设置及参数验证（隔离数据目录）：
pixi run python tests/appimage_api_smoke.py --image release-assets/<产物名称>.AppImage
# 有桌面显示（或 xvfb-run）时，验证真实 NeoUI/WebKit 与原生 MCP 接口：
GDK_BACKEND=x11 xvfb-run -a pixi run python tests/appimage_neo_smoke.py --image release-assets/<产物名称>.AppImage
```

测试覆盖清单筛选、中文/空格路径、损坏包、架构错误、符号链接/硬链接、只读文件、更新锁、文件被外部修改以及健康检查前后失败的回滚。真实发行版兼容性、Wayland/X11 与 NeoUI 桌面验证必须分别记录，不能用 WSL 中通过单元测试替代。

2026-11-18 已在 WSL2 Ubuntu 22.04 x86_64 中，通过 `pixi run package-appimage-webui` 生成并验证 `11.0.2-20261118` 的实际产物：Classic 与更新器版本检查、FUSE 挂载启动、无需 FUSE 的解压启动、真实文件原子替换与备份运行、`.home`/`.config` 保留均通过；AppImage 测试 7 项、共享更新清单测试 5 项全部通过，无跳过。隔离用户目录的 MCP 状态/设置/无效请求检查通过；Xvfb/X11 下实际 NeoUI、WebKit 辅助进程和原生 MCP 桥检查通过。构建使用 4 个任务、1 个 C++ 链接任务与单线程压缩。该结果不代表已验证 Wayland、其他发行版、完整 CurseForge 下载流程或 GitHub 在线跨版本升级。

当前是完整文件下载，未实现 zsync 增量更新和独立签名；SHA-256 的可信来源是 HTTPS GitHub Release 清单。健康检查只保证程序能报告预期版本，不代表完整界面正常。备份自动清理、断电恢复提示与其他 Linux 发行版验证仍待完善。首次进入新协议需要手动安装新 AppImage；完整在线升级需发布带专用清单的 Release 后验证。

## Linux 便携目录包

### 打包与运行

```sh
pixi run package-linux-portable
# 同时包含 NeoUI 和 Classic
pixi run package-linux-portable-webui
```

便携包复用 AppImage 的 Pixi/Meson 构建和依赖部署流程，输出 `LunaLauncher-Linux-x64-<实际版本>[-webui]-portable.tar.gz`、SHA-256 文件与独立的 `linux-x64-portable-update.json`。它包含 Qt、GTK、WebKit、OpenSSL、C++ 运行库及辅助程序，不再把缺少这些依赖的原始安装树直接压缩发布。压缩为单线程，默认构建 4 个任务，C++ 链接并发为 1。

将包解压到一个**独立、可写**的目录，通过根目录的 `./AppRun` 启动；带 NeoUI 时默认进入 NeoUI，`./AppRun --luna-entry classic` 进入 Classic。不要直接启动 `usr/bin` 内的程序，否则会绕过依赖环境与兼容性检查。默认数据目录是 `usr/UserData`；沿用现有 `LUNA_LAUNCHER_DATA`、`usr/.luna-data-dir`、`usr/.luna-use-system-data` 规则，Classic 与 NeoUI 共用数据位置。整个目录可以移动，数据默认随之移动。

包根目录的 `.luna-portable.json` 记录版本、架构、glibc 下限、界面能力以及程序文件的大小、SHA-256 或相对符号链接。没有这个标记的普通解压包、系统安装目录不会自动启用目录替换更新。

### 发行版兼容性

同一架构的包可以覆盖多个发行版，但不是任意 Linux 通用。打包脚本扫描实际发货 ELF 的 `GLIBC_*` 符号，取最高要求写入清单；AppRun 在启动前检查 CPU 和宿主 glibc，更新器同样过滤不满足要求的新版本。当前 x64 包要求 **glibc >= 2.35**，由 Ubuntu 22.04 基线构建。

因此 Ubuntu 20.04 / Debian 11 等 glibc 2.31 系统不能直接运行当前包；Debian 12、较新 Ubuntu/Fedora/Arch 在 glibc 版本上达到下限，仍需分别验证实际桌面兼容性。Alpine/musl 不支持当前 glibc 构建；ARM64 需要另一份原生构建。不要为了运行启动器手动覆盖系统 glibc。

图形驱动、内核、X11/Wayland、音频和桌面服务仍由宿主提供，随包部署运行库不能消除这些差异。AppImage 也存在这类边界。需要支持更旧系统时，应降低整条 Qt/Rust/.NET/WebKit 构建链的基线并实际验证；只修改清单中的版本数字无效。deb/rpm 应分别交给发行版包管理器，Flatpak/Snap 交给对应运行时与商店更新。

### 更新与数据保护

Classic、NeoUI 和 MCP 共用外部更新器，按协议、架构、glibc 和 NeoUI 能力选择便携包；不会把带 NeoUI 的安装更新成缺少 NeoUI 的包，也不会选用 AppImage 或 Windows Setup。

更新器先将清单内的程序及运行库复制到独立临时目录并校验，从该目录运行工作进程，避免替换自身仍使用的库。工作进程等待启动器退出，显示下载、解压、验证和数据保留进度。在目标同级目录独占加锁、校验压缩包、限量解压，拒绝越界路径、硬链接、特殊文件、逃逸/损坏符号链接、重复路径及夹带用户数据的包，再校验完整程序文件清单并执行真实版本健康检查。

随后将旧目录改名为唯一 `.backup-<UUID>`，将用户数据、数据位置标记及不冲突的未知文件移至新目录，移除新版不再提供的旧程序文件，通过 rename 安装并再次验证。可捕获的替换/验证失败会撤回数据移动并还原旧目录；恢复失败保留备份和暂存目录供人工处理。冲突的未知文件留在备份并记录日志，外部实例和数据目录不复制、不删除。

更新成功保留程序备份，使用原数据目录和当前 Classic/NeoUI 入口重启。备份中的用户数据已移至当前目录，人工恢复程序时须保留当前用户数据。当前下载整个归档；独立签名、断电恢复、备份保留策略及 `/tmp/luna-portable-worker-*` 工作目录自动清理仍待完善。完整在线跨版本升级需要发布包含专用清单的新 Release 后验证。

### 验证入口

```sh
LUNA_BUILD_TESTING=true pixi run configure release
pixi run test-linux-portable-update
# 使用实际发布包验证运行库隔离、真实替换、备份运行、数据保留和回滚：
pixi run python scripts/portable_smoke.py release-assets/<产物名称>-portable.tar.gz
# 实际包的 MCP 更新接口与 NeoUI/WebKit 原生桥（无 NeoUI 时省略 --neo）：
GDK_BACKEND=x11 xvfb-run -a pixi run python tests/linux_portable_runtime.py --archive release-assets/<产物名称>-portable.tar.gz --neo
pixi run python tests/appimage_packaging_test.py
```

测试使用临时实例数据，不能以版本检查或 glibc 下限满足代替真实发行版、桌面及游戏运行验证。

2026-11-18：通过 `pixi run package-linux-portable-webui`（release，4 个构建任务、1 个 C++ 链接任务）生成 `11.0.2-20261118` 实际便携包；最终产物为 240,960,818 字节，SHA-256 `fc3030d24bb5f72699ef761db082eb355d4ea11f237191d7e5613742a2760756`。同一归档在 WSL2 Ubuntu 22.04（glibc 2.35）和 Arch rolling（glibc 2.44）中通过 Classic/更新器版本检查、默认 `usr/UserData` 解析、隔离 MCP 更新状态/设置/无效请求，以及 NeoUI/WebKit 原生桥测试。两个 NeoUI 测试均使用虚拟 X11；Arch 通过带认证的 TCP 连接使用 Ubuntu 的 Xvfb。Ubuntu 的实际归档替换、独立更新器运行库、旧备份可运行、数据保留及强制健康检查失败回滚通过；空文件不再被误判为损坏。AppRun 兼容性拒绝测试与 AppImage/共享更新清单回归均通过。尚未验证物理机、Wayland、完整游戏/音频、其他发行版或 GitHub 在线跨版本更新。

## 其他平台 TODO

| 平台/分发方式 | 后续工作 | 当前处理 |
| --- | --- | --- |
| macOS `.app` / DMG | Luna 自有 Sparkle feed 与公钥；Developer ID 签名、公证；架构/通用包匹配；原子替换与重启 | 手动下载；不使用 Prism 上游 feed |
| Linux 便携 tar.gz 后续 | 更广泛的发行版/桌面验证、独立签名、断电恢复、备份与临时目录清理 | x64 目录更新已实现；旧无清单包首次手动安装 |
| Linux AppImage ARM64 | 原生打包工具锁、依赖部署和 ARM64 设备验证；通用清单架构筛选已有 | 暂不发布 ARM64 包 |
| Linux deb/rpm/系统包 | 通过发行版仓库和包管理器更新，界面提示正确命令 | 使用包管理器 |
| Flatpak / Snap | 检测沙盒分发来源，交给对应商店/管理器 | 使用 Flatpak/Snap 更新 |
| Windows ARM64 | 发布原生 ARM64 Setup 与清单，并在 ARM64 设备完成运行验证 | 筛选协议已支持；发行与设备验证 TODO |
| Windows 系统级安装 | 提权更新代理、ACL 和所有用户注册，明确与当前用户安装区别 | 当前实现针对现有当前用户安装；无权限时失败并保留旧文件 |
| Android APK | 同包名、同签名、递增 versionCode；系统安装器确认和未知来源授权；数据保留验证 | 手动安装 APK |

Windows / AppImage / 带清单 Linux 便携目录以外的 NeoUI 更新 API 明确返回未支持原因，不把“未实现”报告成已是最新版。
