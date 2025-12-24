<p align="center">
<img alt="Luna Launcher" src="/program_info/lunalauncher.png" width="40%">
</p>

<p align="center">
  <a href="README.md">English</a> | <a href="README.zh-CN.md">简体中文</a>
</p>

<p align="center">
  Luna Launcher 是一个自定义的 Minecraft 启动器，让你可以轻松管理多个 Minecraft 安装。<br />
  <br />
  本项目是 Prism Launcher 的独立 <b>分支</b>，<b>未被</b> Prism Launcher 项目认可或与其关联。
  <br />
  通过支持社区维护的镜像 API（如 <b>BMCLAPI</b>），提升了访问便利性。
</p>

---

## 安装

正式版本的安装说明和下载将在稳定版本发布后提供。

目前，本项目主要面向开发者和早期测试者。

---

## 构建

### Windows

本项目为 Windows 提供了两个自动化构建环境：

#### 选项 A：MSYS2 + GCC（推荐）

使用 MSYS2 的 UCRT64 GCC 工具链，通过 `msys2.toml` 管理。

**方法 1：使用 Msys2Manager (m2m) - 推荐**

[m2m](https://github.com/AndreaFrederica/Msys2Manager/releases) 是专门用于管理 MSYS2 环境的 CLI 工具。下载最新版本并将其添加到 PATH。

```powershell
# 1. 引导安装 MSYS2（仅首次）
m2m bootstrap

# 2. 配置和构建
m2m run configure
m2m run build

# 3. 安装
m2m run install
```

**可用的 m2m 命令：**

| 命令 | 说明 |
|------|------|
| `m2m init` | 初始化 `msys2.toml` 配置 |
| `m2m bootstrap` | 下载并安装 MSYS2 |
| `m2m run <task>` | 运行 `msys2.toml` 中定义的任务 |
| `m2m run -l` | 列出可用任务 |
| `m2m sync` | 同步配置中的包 |
| `m2m add <package>` | 添加包到配置 |
| `m2m remove <package>` | 从配置中删除包 |
| `m2m shell` | 打开交互式 MSYS2 shell |
| `m2m update` | 更新所有 MSYS2 包 |

**方法 2：使用 PowerShell 脚本**

或者，使用提供的 PowerShell 脚本：

```powershell
# 1. 初始化 MSYS2 环境（仅首次）
.\tools\msys2\bootstrap.ps1

# 2. 配置和构建
.\tools\msys2\run.ps1 configure
.\tools\msys2\run.ps1 build

# 3. 安装
.\tools\msys2\run.ps1 install
```

**可用的 PowerShell 命令：**

| 命令 | 说明 |
|------|------|
| `.\tools\msys2\run.ps1 configure` | 配置 CMake |
| `.\tools\msys2\run.ps1 configure_debug` | 配置 CMake（Debug 模式） |
| `.\tools\msys2\run.ps1 build` | 构建项目 |
| `.\tools\msys2\run.ps1 install` | 安装到 `install/` |
| `.\tools\msys2\run.ps1 portable` | 创建便携版 |
| `.\tools\msys2\run.ps1 clean` | 清理构建目录 |
| `.\tools\msys2\run.ps1 test` | 运行构建目录中的程序 |
| `.\tools\msys2\run.ps1 test_install` | 运行安装目录中的程序 |
| `.\tools\msys2\sync.ps1` | 同步 `msys2.toml` 中的包 |
| `.\tools\msys2\add.ps1 <package>` | 添加包 |
| `.\tools\msys2\remove.ps1 <package>` | 删除包 |

**管理依赖：**

编辑 `msys2.toml` 添加/删除包，然后运行 `m2m sync` 或 `.\tools\msys2\sync.ps1`。

或使用辅助命令：
```powershell
# 使用 m2m
m2m add mingw-w64-ucrt-x86_64-qt6-tools
m2m remove mingw-w64-ucrt-x86_64-qt6-tools

# 使用 PowerShell
.\tools\msys2\add.ps1 mingw-w64-ucrt-x86_64-qt6-tools
.\tools\msys2\remove.ps1 mingw-w64-ucrt-x86_64-qt6-tools
```

**在 MSYS2 Shell 中：**

```powershell
# 使用 m2m
m2m shell

# 使用 PowerShell
.\tools\msys2\shell.ps1
```

然后使用 bash 等价命令：
```bash
./tools/msys2/sync.sh
./tools/msys2/run.sh configure
./tools/msys2/run.sh build
```

#### 选项 B：Pixi + MSVC

使用 Pixi 配合 MSVC 工具链，通过 `pixi.toml` 管理。需要安装 [Pixi](https://pixi.sh) 和 Visual Studio。

**前置要求：**

- Visual Studio 2022（或 Build Tools）
- [Pixi](https://pixi.sh/latest/installation/)

**重要提示：** 你必须在 **x64 Native Tools Command Prompt for VS 2022**（或对应 VS 版本）中运行命令。可在以下位置找到：

```
开始菜单 > Visual Studio 2022 > x64 Native Tools Command Prompt for VS 2022
```

```powershell
# 在 x64 Native Tools Command Prompt 中：

# 1. 安装依赖并配置
pixi run configure

# 2. 构建
pixi run build

# 3. 安装
pixi run install
```

**可用任务：**

| 命令 | 说明 |
|------|------|
| `pixi run configure` | 配置 CMake |
| `pixi run build` | 构建项目 |
| `pixi run install` | 安装到 `install/` |
| `pixi run portable` | 创建便携版 |

### 其他平台

对于 Linux、macOS 或手动构建，请参考上游构建说明：

- <https://prismlauncher.org/wiki/development/>

构建过程和要求基本相同，除了项目名称和后端配置。

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
- 检查 [CMakeLists.txt](CMakeLists.txt) 并将所有上游 API 密钥更改为你自己的，或将它们设置为空字符串 (`""`) 以禁用相关功能。

如果你正在为发行版构建此软件，请将 `Launcher_BUILD_PLATFORM` 设置为适当的标识符（例如：`archlinux`、`fedora`、`nixpkgs`）。

---

## 许可证

所有启动器代码使用 **GPL-3.0-only** 许可证。

Logo 和相关资源使用 **CC BY-SA 4.0** 许可证。
