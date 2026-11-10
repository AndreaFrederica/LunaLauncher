# LunaLauncher 独立安装壳与 SFX 发布计划

## 目标

实现一个独立的 Rust/egui 安装壳，用于把现有 `install-release` 目录组装为可执行的 Windows SFX 发布包，同时保留普通 ZIP 发布包。用户可以双击 SFX 完成安装，也可以使用 7-Zip、WinRAR 等工具查看和手动提取其中的资源。

安装壳与 Qt 启动器解耦，不改变现有启动器的 Qt DLL 布局，不把安装逻辑放入 `lunalauncher.exe`、`lunalauncher-cli.exe` 或 NeoUI 主程序。

## 约束

- 安装壳是独立 Rust 项目，放在仓库单独目录，可脱离主工程单独构建和测试。
- 安装壳使用 egui，不能依赖 Qt、C++ Launcher 内部库或 NeoUI 前端运行时。
- 安装壳源代码按 MPL-2.0 发布；现有启动器和发布内容继续按 GPLv3 及各自原许可证发布。
- 现有 `meson install`、Qt 部署和 `install-release` 目录布局保持不变。
- 发布内容使用具有明确文件名编码标记的 ZIP：非 ASCII 条目使用 UTF-8/EFS 标志，ASCII 条目保持编码不变性；大型内容使用 ZIP64。
- 默认压缩算法使用 Deflate，以保持 Windows、7-Zip、WinRAR 和其他常见工具的兼容性。
- 解压必须防止绝对路径、路径穿越、重复目标和符号链接逃逸。
- SFX 组装完成后再进行 Windows 代码签名。

## 产物和目录

计划新增独立项目目录：

```text
installer-shell/
├─ Cargo.toml
├─ src/
├─ LICENSE-MPL-2.0
├─ README.md
└─ tests/
```

正式发布产物：

```text
LunaLauncher-Windows-x64-<version>.zip
LunaLauncher-Windows-x64-<version>-Setup.exe
```

SFX 的物理布局：

```text
[installer-shell.exe][footer metadata][UTF-8 ZIP payload]
```

footer 至少包含 magic、payload 偏移、payload 大小、格式版本和 SHA-256。壳程序从 footer 定位自身附带的 ZIP；归档工具从 ZIP 的中央目录读取内容。footer 放在 ZIP 前面，避免压缩软件报告归档末尾存在私有尾数据。

## 用户流程

### 首次运行

1. 校验 SFX payload 的格式、长度和 SHA-256。
2. 显示首屏选择：进入安装向导、仅解压文件，或以归档查看器打开。
3. 归档查看器显示 UTF-8 ZIP 条目、支持过滤，并允许选择目录后提取全部文件。
4. 安装向导询问安装目录。
5. 询问默认绑定入口：Classic UI、NeoUI，或同时创建两个入口。
6. 询问是否创建桌面快捷方式，并提供开始菜单快捷方式选项。
7. 将 ZIP 原子解压到目标目录的临时目录，校验完成后切换为正式目录。
8. 创建快捷方式并启动用户选择的入口。
9. 只有解压、快捷方式和启动全部成功后才写入安装清单和完成配置。

如果用户只查看或解压文件，或者安装向导中途失败/关闭，安装完成配置不会写入；下次启动仍显示首屏选择。

### 后续运行

- 直接读取已保存的安装配置并启动默认入口。
- 允许通过命令行重新打开设置或切换默认入口。
- 如果安装目录缺失或清单校验失败，进入修复/重新安装流程。

### 卸载

- 提供独立的卸载入口，可由壳程序自身的 `--uninstall` 模式实现。
- 删除由安装清单创建的快捷方式、开始菜单项、注册信息和安装文件。
- 默认保留用户数据目录；提供明确选项后才删除用户数据。
- 卸载程序运行期间不能直接删除自身，应使用临时 helper 或延迟删除机制。

## 独立命令接口

安装壳必须能够在没有 SFX payload 的情况下独立测试：

```text
installer-shell --payload app.zip --install
installer-shell --payload app.zip --extract <directory>
installer-shell --payload app.zip --list
installer-shell --uninstall <directory>
installer-shell --reset-config
```

正式 SFX 运行时不需要显式传入 `--payload`，默认从自身 footer 定位归档。

## 许可证边界

- `installer-shell.exe` 的源代码和二进制壳按 MPL-2.0 发布，并保留 MPL 版权和许可证声明。
- `lunalauncher.exe`、`lunalauncher-cli.exe`、`luna-neo-ui.exe` 及其资源继续按 GPLv3 或其原许可证发布。
- SFX 是包含多个许可证组件的发布聚合物，发布目录必须同时附带 MPL-2.0、GPLv3 和第三方许可证文件。
- 安装壳不链接 GPLv3 Launcher 代码，只通过文件、命令行和子进程接口交互。

## 实施阶段

### 1. 项目骨架

- [x] 创建 `installer-shell/` 独立 Cargo 项目。
- [x] 加入 MPL-2.0 文件头、许可证文件和独立 README。
- [x] 建立 egui 窗口、命令行解析和平台抽象层。
- [x] 在没有 Launcher 源码或 Qt 环境的情况下完成独立构建。

### 2. 归档格式

- [x] 实现 ZIP payload 读取和 ZIP64 支持。
- [x] 生成并验证 UTF-8/EFS 文件名标记；ASCII 文件名必须不依赖本地代码页。
- [x] 实现路径安全检查、冲突检查、临时目录解压和原子提交。
- [x] 增加坏包、路径穿越、重复条目检查和非 UTF-8 条目的测试。

### 3. SFX 组装

- [x] 实现 payload footer 的读取和校验。
- [x] 编写独立组装工具，将 `installer-shell.exe` 与 `install-release` ZIP 组合。
- [x] 验证 SFX 可执行、可被 7-Zip 打开，并能提取到完整 ZIP 内容。
- [ ] 如果发布流程启用代码签名，则在 SFX 组装之后签名。

### 4. 安装配置

- [x] 实现 Classic UI、NeoUI 和双入口绑定。
- [x] 实现安装目录、桌面快捷方式和开始菜单快捷方式。
- [x] 实现安装清单、配置持久化和重新运行逻辑。
- [x] 实现卸载、保留用户数据和删除快捷方式。
- [x] 首次运行提供安装、仅解压和归档查看器三种入口。
- [x] 仅查看/解压或未完成向导时不写入完成配置，下次继续显示首屏。

### 5. 发布接入

- [x] 保持 `pixi run install` 和 `install-release` 语义不变。
- [x] 增加独立壳程序的构建任务。
- [x] 在发布阶段从现有 `install-release` 生成普通 ZIP。
- [x] 在发布阶段从同一 ZIP 生成 Windows SFX。
- [x] 为普通 ZIP、SFX、哈希文件和许可证文件增加 CI 验证。

### 6. 验证

- [x] 在 Windows 上通过 CLI 测试首次安装、重新安装、入口配置持久化和卸载。
- [x] 使用 7-Zip 验证 ZIP/SFX 行为。
- [x] 使用中文和带空格的文件名验证编码和路径处理。
- [x] 验证 SHA-256、异常 payload 拒绝和原子安装；代码签名留给具备签名凭据的发布环境。
- [x] 验证 shell 项目可以独立构建、测试和发布。

## 非目标

- 不在本阶段重排 Qt DLL 或现有安装目录。
- 不把 Qt 静态链接进安装壳。
- 不把安装壳逻辑迁移到 GPLv3 Launcher 主程序。
- 不用自定义私有压缩格式替代标准 ZIP。
- 不把 macOS App Bundle 和 Linux 包强行做成 Windows 风格的同一 SFX 文件；这些平台沿用各自的归档和签名方式。

## 完成标准

当以下条件全部满足时目标完成：

1. 独立 `installer-shell` 项目可以在没有主工程依赖的情况下构建和测试。
2. Windows SFX 可以双击运行，并能被至少一种主流压缩软件打开和提取。
3. 普通 ZIP 与 SFX 包含完全相同的发布内容和许可证文件。
4. 首次安装、UI 绑定、快捷方式、后续启动和卸载流程可重复验证。
5. 现有 Launcher 构建、安装和 Qt 运行时布局不需要改变。

## 当前状态

- [x] 方案确认：独立 egui 壳、标准 UTF-8 ZIP、Windows SFX、普通 ZIP 并行发布。
- [x] 方案确认：壳程序使用 MPL-2.0，Launcher 保持 GPLv3。
- [x] 开始创建 `installer-shell/` 项目。
