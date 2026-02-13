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

### Windows

本项目为 Windows 提供了两个自动化构建环境：

#### 选项 A：MSYS2 + GCC（推荐）

使用 MSYS2 的 UCRT64 GCC 工具链，通过 `msys2.toml` 管理。

**方法 1：使用 Msys2Manager (m2m) - 推荐**

[m2m](https://github.com/AndreaFrederica/Msys2Manager/releases) 是专门用于管理 MSYS2 环境的 CLI 工具。下载最新版本并将其添加到 PATH。

**前置要求：**

- Java Development Kit 8 或更高版本
  - 确保在 Adoptium 安装程序中启用"设置 JAVA_HOME 变量"
  - 构建前需要设置 `JAVA_HOME` 环境变量

```powershell
# 1. 设置 JAVA_HOME（根据需要调整路径）
$env:JAVA_HOME = "C:\Program Files\Eclipse Adoptium\jdk-17.0.12.101-hotspot"

# 2. 引导安装 MSYS2（仅首次）
m2m bootstrap

# 3. 配置和构建
m2m run configure
m2m run build

# 4. 安装
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

**前置要求：**

- Java Development Kit 8 或更高版本
  - 确保在 Adoptium 安装程序中启用"设置 JAVA_HOME 变量"
  - 构建前需要设置 `JAVA_HOME` 环境变量

```powershell
# 1. 设置 JAVA_HOME（根据需要调整路径）
$env:JAVA_HOME = "C:\Program Files\Eclipse Adoptium\jdk-17.0.12.101-hotspot"

# 2. 初始化 MSYS2 环境（仅首次）
.\tools\msys2\bootstrap.ps1

# 3. 配置和构建
.\tools\msys2\run.ps1 configure
.\tools\msys2\run.ps1 build

# 4. 安装
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

对于 Linux、macOS 或在没有自动化环境的 Windows 上手动构建，以下是详细的构建说明。

---

## 详细构建说明

### 获取源代码

使用 git 克隆源代码，并获取所有子模块：

```bash
git clone --recursive https://github.com/AndreaFrederica/LunaLauncher.git
cd LunaLauncher
```

本文档的其余部分假设您已经克隆了仓库。

### Windows 上使用 MSYS2 手动构建

如果您不想使用自动化脚本，可以按照以下手动步骤操作。

**依赖项：**

- [MSYS2](https://www.msys2.org/) - Windows 软件分发和构建平台
- Java Development Kit 8 或更高版本
  - 确保在 Adoptium 安装程序中启用"设置 JAVA_HOME 变量"

**准备 MSYS2：**

1. 从开始菜单的 MSYS2 文件夹中打开快捷方式
2. 我们建议使用 MSYS2 的 UCRT64 或 CLANG64 msystem 进行构建
3. 安装辅助工具：
   ```bash
   pacman -Syu pactoys git mingw-w64-ucrt-x86_64-binutils
   ```
4. 使用 pacboy 安装所有构建依赖：
   ```bash
   pacboy -S toolchain:p cmake:p ninja:p qt6-base:p qt6-5compat:p qt6-svg:p qt6-imageformats:p quazip-qt6:p extra-cmake-modules:p ccache:p
   ```

   或者使用 Qt 5（适用于较旧的 Windows 版本）：
   ```bash
   pacboy -S toolchain:p cmake:p ninja:p qt5-base:p qt5-svg:p qt5-imageformats:p quazip-qt5:p extra-cmake-modules:p ccache:p
   ```

**从命令行编译：**

```bash
# 导航到源文件夹
cd /path/to/LunaLauncher

# 配置 CMake（Debug 构建）
cmake -Bbuild -DCMAKE_INSTALL_PREFIX=install -DENABLE_LTO=ON -DCMAKE_BUILD_TYPE=Debug -G Ninja

# 对于 Release 构建，将上面的 Debug 替换为 Release

# 可选：添加 -DLauncher_QT_VERSION_MAJOR=5 以使用 Qt 5
# 可选：添加 -DCMAKE_CXX_COMPILER_LAUNCHER=ccache 以加快重新编译速度

# 构建
cmake --build build

# 安装
cmake --install build

# 创建便携版（数据存储在应用程序目录中）
cmake --install build --component portable
```

**Qt 5 构建注意事项：** 使用 Qt 5 构建时，OpenSSL DLL 不会自动复制。手动复制它们：
```bash
cp /ucrt64/bin/libcrypto-1_1-x64.dll /ucrt64/bin/libssl-1_1-x64.dll install
```
将 `ucrt64` 替换为您的 msystem（例如 `clang64`）。

### Windows 上使用 MSVC 手动构建

**依赖项：**

- [Visual Studio](https://visualstudio.microsoft.com/) - Windows 软件分发和构建平台
  - 如果不想安装 Visual Studio IDE，请下载"Visual Studio Build Tools"
  - 选择"使用 C++ 的桌面开发"
  - CMake 将在可选组件中被选中
- Java Development Kit 8 或更高版本
- [Qt](https://www.qt.io/)
  - 对于 Qt 6（推荐 Qt 6.6.2），需要"Qt 5 兼容性模块"和"Qt 图像格式"
  - 对于 Qt 5（推荐 Qt 5.15.2），需要 OpenSSL 工具包

**从命令行编译：**

您需要从 **x64 Native Tools Command Prompt for VS 2022**（或相应 VS 版本）运行命令。

```cmd
REM 导航到源文件夹
cd C:\Path\To\LunaLauncher

REM 配置 CMake（根据需要调整 Qt 路径）
cmake -Bbuild -DCMAKE_INSTALL_PREFIX=install -DENABLE_LTO=ON -DCMAKE_PREFIX_PATH=C:\Qt\6.6.2\msvc2019_64\lib\cmake

REM 构建（Debug）
cmake --build build --config Debug -- /p:UseMultiToolTask=true /p:EnforceProcessCountAcrossBuilds=true

REM 安装
cmake --install build --config Debug

REM 创建便携版
cmake --install build --config Debug --component portable
```

**Qt 5 构建注意事项：** 手动复制 OpenSSL DLL：
```cmd
robocopy C:\Qt\Tools\OpenSSL\Win_x64\bin\ install libcrypto-1_1-x64.dll libssl-1_1-x64.dll
```

### Linux 和 macOS

对于 Linux 和 macOS 构建，请参考上游 Prism Launcher 构建说明：

- <https://prismlauncher.org/wiki/development/>

构建过程和要求基本相同，除了项目名称。

---

## IDE 和工具

### ccache

ccache 是一个编译器缓存，通过缓存以前的编译来加速重新编译。

- **MSYS2:** 使用 `pacboy -S ccache:p` 安装，并在 CMake 中添加 `-DCMAKE_CXX_COMPILER_LAUNCHER=ccache`
- **MSVC:** 需要 ccache 4.7.x 或更高版本。复制 `ccache.exe` 并重命名为 `cl.exe`，然后在构建参数中添加 `/p:CLToolExe=cl.exe /p:CLToolPath=<ccache cl 路径>`

### VS Code

1. 安装 [C/C++ 扩展](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools)
2. 配置 C/C++：编辑配置 (UI)
   - 将 Qt include 文件夹添加到 `includePath`
   - 将 `-L/{Qt 路径}/lib` 添加到 `compilerArgs`
   - 将 `compileCommands` 设置为 `${workspaceFolder}/build/compile_commands.json`
   - 将 `cppStandard` 设置为 `c++17` 或更高
3. 使用 `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` 重新配置 CMake

示例 `.vscode/c_cpp_properties.json`：
```json
{
    "configurations": [
        {
            "name": "Windows (MSYS2)",
            "includePath": [
                "${workspaceFolder}/**",
                "C:/msys64/ucrt64/include/**"
            ],
            "compilerPath": "C:/msys64/ucrt64/bin/gcc.exe",
            "compilerArgs": [
                "-LC:/msys64/ucrt64/lib"
            ],
            "compileCommands": "${workspaceFolder}/build/compile_commands.json",
            "cStandard": "c17",
            "cppStandard": "c++17",
            "intelliSenseMode": "windows-gcc-x64"
        }
    ],
    "version": 4
}
```

### CLion

1. 打开 CLion → File → Open → 选择源文件夹
2. Settings → Build, Execution, Deployment → Toolchains
   - 设置 CMake、Make、C 编译器、C++ 编译器、调试器
3. Settings → Build, Execution, Deployment → CMake
   - 将构建目录设置为 `build`
4. 创建新配置，目标：所有目标
5. 使用按钮构建和运行

### Qt Creator

1. 通过 MSYS2 安装 Qt Creator：`pacboy -S qt-creator:p`
2. 从 MSYS2 shell 打开 Qt Creator：`qtcreator`
3. File → Open File or Project → 选择 `CMakeLists.txt`
4. 点击右下角的"Configure Project"
5. 按"Run"按钮

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
