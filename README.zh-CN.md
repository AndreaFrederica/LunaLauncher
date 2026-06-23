<p align="center">
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="./program_info/cc.sirrus.LunaLauncher.logo-darkmode.svg">
  <source media="(prefers-color-scheme: light)" srcset="./program_info/cc.sirrus.LunaLauncher.logo.source.svg">
  <img alt="Luna Launcher" src="./program_info/cc.sirrus.LunaLauncher.logo.svg" width="60%">
</picture>
</p>


<p align="center">
  <a href="README.md">English</a> | <a href="README.zh-CN.md">简体中文</a>
</p>

<p align="center">
  Luna Launcher 是一个自定义的 Minecraft 启动器，让你可以轻松管理多个 Minecraft 安装。
  <br />
  访问我们的官网 [lunalauncher.sirrus.cc](https://lunalauncher.sirrus.cc) 了解更多信息。
  <br />
  开发版本可以在 <a href="https://github.com/AndreaFrederica/LunaLauncher/actions">GitHub Actions</a> 下载。
  <br />
  <br />
  本项目是 Prism Launcher 的独立 <b>分支</b>，<b>未被</b> Prism Launcher 项目认可或与其关联。
  <br />
  通过支持社区维护的镜像 API（如 <b>BMCLAPI</b>）。
</p>

---

> **⚠️ 构建系统变更：已切换至 Meson**
>
> Luna Launcher 已从 **CMake** 切换为 **Meson** 作为主要构建系统。`CMakeLists.txt` 保留用于兼容，但**不再主动维护**，可能出现问题。
>
> **如果你正在将 Luna 的功能 backport 到 Prism Launcher 或其他分支：** 请注意，Luna 中的构建相关改动是针对 Meson 编写的（`meson.build`、`meson_options.txt`、`scripts/pixi_meson.py`）。如果你的目标项目仍使用 CMake，需要自行适配。
>
> 参阅[构建指南](https://lunalauncher.sirrus.cc/build.html)获取最新说明。

---

## 特性

> **注意：** Luna 是 Prism Launcher 的增强分支，在 Prism 的基础上额外增加了更多功能，同时保持与 Prism Launcher 实例和功能的完全兼容。
>
> 我们欢迎开发者将 Luna 的功能反向移植到上游 Prism Launcher。Luna 没有主动提交 PR 是因为开发者时间有限——Luna 的维护者全是业余开发者，同时还需要维护其他开源项目。如果您有兴趣参与开发，请随时联系我们！

- **服务端管理（开发中）**：直接从启动器管理 Minecraft 服务端实例，包括下载、配置和管理服务端 JAR 文件
- **P2P 联机**：内置 [Terracotta](https://github.com/burningtnt/Terracotta) P2P 联机功能 - 与朋友一起游戏，无需端口映射或复杂的网络配置
- **YukariConnect 联机**：内置支持 [YukariConnect](https://github.com/ElicaseTech/YukariConnect) 作为 P2P 联机服务（YukariConnect 旨在作为使用 AGPL 许可的 Terracotta 的替代实现，也可作为独立组件提供）
- **镜像 API 支持**：内置支持 BMCLAPI 和其他社区镜像，在中国地区下载更快
- **Fluent 主题**：内置 Fluent Dark/Light 主题与图标资源
- **新 UI 布局**：可选的实验性三列布局（需重启生效）
- **服务器预览**：从工具栏快速进入服务器预览
- **自定义模型**：支持自定义玩家模型（含 Yes Steve 模型）
- **原理图文件**：资源管理支持 Minecraft 原理图（schematic）文件
- **跨平台**：支持 Windows、Linux 和 macOS

---

## LunaUI 自定义面板脚本开发

Luna Launcher 支持基于实例的 `lunaui` 自定义设置面板。

- 运行目录：`<实例根目录>/lunaui`
- 开发文档：[`docs/lunaui/README.md`](docs/lunaui/README.md)
- 类型定义（`.d.ts`）：[`docs/lunaui/lunaui.d.ts`](docs/lunaui/lunaui.d.ts)

`.d.ts` 主要用于在编写 `lunaui/*.js` 时提供编辑器智能提示。

---

## 安装

正式版本的安装说明和下载将在稳定版本发布后提供。

目前，本项目主要面向开发者和早期测试者。

---

## 构建

本项目使用 **Meson** 作为构建系统。推荐通过 **Pixi** 来构建，它可以自动处理依赖和环境配置。

### 快速开始（Pixi）

#### 前置要求

- [Pixi](https://pixi.sh/latest/installation/)

**Windows：** 你必须在 **x64 Native Tools Command Prompt for VS 2022**（或对应 VS 版本）中运行命令。

**Linux：** 确保 `gcc`、`g++`、`pkg-config` 和 Qt 6 开发库可用（Pixi 会自动安装大部分）。

```bash
# 1. 配置
pixi run configure

# 2. 构建
pixi run build

# 3. 安装
pixi run install
```

#### 可用任务

| 命令 | 说明 |
|------|------|
| `pixi run configure` | 配置 Meson 构建目录 |
| `pixi run build` | 一键完成依赖准备并用 Meson 编译 |
| `pixi run install` | 使用 Meson 构建并安装到 `install/` |
| `pixi run deploy` | 运行 `windeployqt` 并复制运行时 DLL（仅 Windows） |
| `pixi run install_qt` | 下载并安装 Qt 到 `third_party/qt` |

#### 构建配置

| 配置 | 说明 |
|------|------|
| `release`（默认） | Release 构建，静态库 |
| `debug` | Debug 构建，动态库 |
| `linux-x64-gcc-release` | Linux 交叉编译 Release 构建 |

```bash
# 以 Debug 模式构建
pixi run build --profile debug

# 为 Linux 交叉编译（从 Windows）
pixi run build --profile linux-x64-gcc-release
```

### Linux 原生构建

在 Linux 机器上，可以直接使用 Pixi：

```bash
# 安装依赖（Arch 示例）
sudo pacman -S meson ninja gcc pkgconf qt6-base qt6-svg qt6-imageformats qt6-5compat quazip-qt6 cmark

# 配置
pixi run configure

# 构建
pixi run build

# 安装
pixi run install
```

### 手动 Meson 构建

如果不使用 Pixi：

```bash
# 安装依赖（Ubuntu/Debian 示例）
sudo apt install meson ninja-build gcc g++ pkg-config qt6-base-dev qt6-svg-dev qt6-imageformats-dev qt6-5compat-dev libquazip1-qt6-dev libcmark-dev zlib1g-dev libarchive-dev liblz4-dev libzstd-dev liblzma-dev libbz2-dev libqrencode-dev

# 配置
meson setup build --buildtype=release --wrap-mode=forcefallback

# 构建
meson compile -C build

# 安装
meson install -C build --no-rebuild
```

#### 常用 Meson 选项

| 选项 | 说明 |
|------|------|
| `-Dbuild_testing=false` | 禁用测试（默认） |
| `-Dbuild_updater=false` | 禁用自动更新 |
| `-Denable_java_downloader=true` | 启用 Java 自动下载 |
| `-Ddisable_ownership_check=false` | 禁用所有权验证 |
| `-Dgamemode=enabled` | 启用 GameMode 支持（Linux） |
| `-Db_vscrt=md` | 使用 MSVC 动态运行时（Windows） |

---

## 详细构建说明

### 获取源代码

使用 git 克隆源代码，并获取所有子模块：

```bash
git clone --recursive https://github.com/AndreaFrederica/LunaLauncher.git
cd LunaLauncher
```

本文档的其余部分假设您已经克隆了仓库。

### IDE 设置

#### VS Code

1. 安装 [Meson 扩展](https://marketplace.visualstudio.com/items?itemName=mesonbuild.mesonbuild) 以获得 Meson 支持
2. 安装 [C/C++ 扩展](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools)
3. 打开项目文件夹 - VS Code 会自动检测 Meson 构建
4. 通过 `Ctrl+Shift+B` 使用构建任务

#### CLion

1. 打开 CLion → File → Open → 选择源文件夹
2. CLion 会自动检测 Meson 项目
3. Settings → Build → Meson → 按需设置构建目录
4. 使用按钮构建和运行

#### Qt Creator

1. 安装 Qt Creator
2. File → Open File or Project → 选择项目根目录（Qt Creator 会检测 `meson.build`）
3. 按提示配置项目
4. 按"Run"按钮运行

---

## 致谢

Luna Launcher 离不开早期项目及其贡献者的工作。

- **Prism Launcher** — 维护了一个现代化、开源和社区驱动的 Minecraft 启动器。
- **MultiMC** — 为许多第三方 Minecraft 启动器奠定基础的原始项目。

我们真诚感谢这些项目的所有贡献者为 Minecraft 社区做出的长期贡献。

---

## 分发/重声明政策

你可以自由地分支、重新分发和提供自定义构建，只要你遵循 [许可证](LICENSE) 的条款。

如果你进行代码更改（而不仅仅是打包）：

- 明确说明你的分支 **不是** Prism Launcher，且 **不被** Prism Launcher 项目认可或与其关联。
- 检查 `meson.build` 并将所有上游 API 密钥更改为你自己的，或将它们设置为空字符串 (`""`) 以禁用相关功能。
- 如果你是在构建由发行版包管理器分发的软件包，请将 `--prefix` 设为 `/usr`，不要用 `/usr/local`。

---

## 许可证

所有启动器代码使用 **GPL-3.0-only** 许可证。

Logo 和相关资源使用 **CC BY-SA 4.0** 许可证。
