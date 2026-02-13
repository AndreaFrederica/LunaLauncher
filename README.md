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
  Luna Launcher is a custom launcher for Minecraft that allows you to manage multiple Minecraft installations with ease.<br />
  <br />
  Visit our website at <a href="https://lunalauncher.sirrus.cc">lunalauncher.sirrus.cc</a> for more information.<br />
  <br />
  Development builds are available on <a href="https://github.com/AndreaFrederica/LunaLauncher/actions">GitHub Actions</a>.<br />
  This project is an independent <b>fork</b> of Prism Launcher and is <b>not</b> endorsed by or affiliated with the Prism Launcher project.
  <br />
  It improves accessibility by supporting community-maintained mirror APIs such as <b>BMCLAPI</b>.
</p>

---

## Features

> **Note:** Luna is an enhanced fork of Prism Launcher, adding extra features on top of Prism's foundation while maintaining full compatibility with Prism Launcher instances and functionality.
>
> We welcome developers to backport Luna's features to the upstream Prism Launcher. Luna does not proactively submit PRs due to limited developer time—all Luna maintainers are hobbyist developers who also maintain other open source projects. If you're interested in contributing, please reach out to us!

- **Server Management (In Development)**: Manage Minecraft server instances directly from the launcher, including downloading, configuring, and managing server JARs
- **P2P Multiplayer**: Built-in support for [Terracotta](https://github.com/burningtnt/Terracotta) and [YukariConnect](https://github.com/ElicaseTech/YukariConnect) - play with friends without port forwarding or complex network setup (YukariConnect is intended as an alternative to the AGPL-licensed Terracotta and can be provided as a standalone component)
- **Mirror API Support**: Built-in support for BMCLAPI and other community mirrors for faster downloads in China
- **Fluent Themes**: Built-in Fluent Dark/Light themes and icon resources
- **New UI Layout**: Optional experimental 3-column layout (requires restart)
- **Server Preview**: Quick server preview access from the toolbar
- **Custom Models**: Support custom player models including the Yes Steve model
- **Schematic Files**: Resource management support for Minecraft schematic files
- **Cross-platform**: Available for Windows, Linux, and macOS

---

## LunaUI Custom Panel Scripting

Luna Launcher supports per-instance custom setting panels under `lunaui`.

- Runtime folder: `<instance-root>/lunaui`
- Development docs: [`docs/lunaui/README.md`](docs/lunaui/README.md)
- Type definitions (`.d.ts`): [`docs/lunaui/lunaui.d.ts`](docs/lunaui/lunaui.d.ts)

The `.d.ts` file is intended for editor IntelliSense when writing `lunaui/*.js` scripts.

---

## Installation

Installation instructions and downloads will be provided once stable releases are available.

At the moment, this project is primarily intended for developers and early testers.

---

## Building

### Windows

This project provides two automated build environments for Windows:

#### Option A: MSYS2 + GCC (Recommended)

Uses MSYS2's UCRT64 GCC toolchain, managed via `msys2.toml`.

**Method 1: Using Msys2Manager (m2m) - Recommended**

[m2m](https://github.com/AndreaFrederica/Msys2Manager/releases) is a dedicated CLI tool for managing MSYS2 environments. Download the latest release and add it to your PATH.

**Prerequisites:**

- Java Development Kit 8 or later
  - Make sure that "Set JAVA_HOME variable" is enabled in the Adoptium installer
  - You'll need to set `JAVA_HOME` environment variable before building

```powershell
# 1. Set JAVA_HOME (adjust path as needed)
$env:JAVA_HOME = "C:\Program Files\Eclipse Adoptium\jdk-17.0.12.101-hotspot"

# 2. Bootstrap MSYS2 (first time only)
m2m bootstrap

# 3. Configure & build
m2m run configure
m2m run build

# 4. Install
m2m run install
```

**Available m2m Commands:**

| Command                  | Description                             |
| ------------------------ | --------------------------------------- |
| `m2m init`             | Initialize `msys2.toml` configuration |
| `m2m bootstrap`        | Download and install MSYS2              |
| `m2m run <task>`       | Run a task defined in `msys2.toml`    |
| `m2m run -l`           | List available tasks                    |
| `m2m sync`             | Sync packages from configuration        |
| `m2m add <package>`    | Add a package to configuration          |
| `m2m remove <package>` | Remove a package from configuration     |
| `m2m shell`            | Open interactive MSYS2 shell            |
| `m2m update`           | Update all MSYS2 packages               |

**Method 2: Using PowerShell Scripts**

Alternatively, use the provided PowerShell scripts:

**Prerequisites:**

- Java Development Kit 8 or later
  - Make sure that "Set JAVA_HOME variable" is enabled in the Adoptium installer
  - You'll need to set `JAVA_HOME` environment variable before building

```powershell
# 1. Set JAVA_HOME (adjust path as needed)
$env:JAVA_HOME = "C:\Program Files\Eclipse Adoptium\jdk-17.0.12.101-hotspot"

# 2. Bootstrap MSYS2 environment (first time only)
.\tools\msys2\bootstrap.ps1

# 3. Configure & build
.\tools\msys2\run.ps1 configure
.\tools\msys2\run.ps1 build

# 4. Install
.\tools\msys2\run.ps1 install
```

**Available PowerShell Commands:**

| Command                                | Description                       |
| -------------------------------------- | --------------------------------- |
| `.\tools\msys2\run.ps1 configure`    | Configure CMake                   |
| `.\tools\msys2\run.ps1 build`        | Build the project                 |
| `.\tools\msys2\run.ps1 install`      | Install to `install/`           |
| `.\tools\msys2\run.ps1 portable`     | Create portable build             |
| `.\tools\msys2\run.ps1 clean`        | Clean build directories           |
| `.\tools\msys2\sync.ps1`             | Sync packages from `msys2.toml` |
| `.\tools\msys2\add.ps1 <package>`    | Add a package                     |
| `.\tools\msys2\remove.ps1 <package>` | Remove a package                  |

**Managing Dependencies:**

Edit `msys2.toml` to add/remove packages, then run `m2m sync` or `.\tools\msys2\sync.ps1`.

Or use the helper commands:

```powershell
# Using m2m
m2m add mingw-w64-ucrt-x86_64-qt6-tools
m2m remove mingw-w64-ucrt-x86_64-qt6-tools

# Using PowerShell
.\tools\msys2\add.ps1 mingw-w64-ucrt-x86_64-qt6-tools
.\tools\msys2\remove.ps1 mingw-w64-ucrt-x86_64-qt6-tools
```

**Inside MSYS2 Shell:**

```powershell
# Using m2m
m2m shell

# Using PowerShell
.\tools\msys2\shell.ps1
```

Then use bash equivalents:

```bash
./tools/msys2/sync.sh
./tools/msys2/run.sh configure
./tools/msys2/run.sh build
```

#### Option B: Pixi + MSVC

Uses Pixi with MSVC toolchain, managed via `pixi.toml`. Requires [Pixi](https://pixi.sh) and Visual Studio to be installed.

**Prerequisites:**

- Visual Studio 2022 (or Build Tools)
- [Pixi](https://pixi.sh/latest/installation/)

**Important:** You must run commands from the **x64 Native Tools Command Prompt for VS 2022** (or corresponding VS version). This can be found under:

```
Start Menu > Visual Studio 2022 > x64 Native Tools Command Prompt for VS 2022
```

```powershell
# From x64 Native Tools Command Prompt:

# 1. Check / configure MSVC
pixi run check_msvc

# 2. Install Qt
pixi run install_qt

# 3. Install vcpkg
pixi run install_vcpkg

# 4. Configure
pixi run configure

# 5. Build
pixi run build

# 6. Install
pixi run install
```

**Available Tasks:**

| Command                | Description             |
| ---------------------- | ----------------------- |
| `pixi run check_msvc`   | Verify MSVC toolchain (sets up env if needed) |
| `pixi run install_qt`   | Download/install Qt via Pixi                  |
| `pixi run install_vcpkg`| Install vcpkg dependencies                    |
| `pixi run configure`    | Configure CMake                               |
| `pixi run build`        | Build the project                             |
| `pixi run install`      | Install to `install/`                         |
| `pixi run portable`     | Create portable build                         |

### Other Platforms

For Linux, macOS, or manual builds on Windows without the automated environments, detailed build instructions follow below.

---

## Detailed Build Instructions

### Getting the Source

Clone the source code using git, and grab all the submodules:

```bash
git clone --recursive https://github.com/AndreaFrederica/LunaLauncher.git
cd LunaLauncher
```

The rest of the documentation assumes you have already cloned the repository.

### Manual Build on Windows with MSYS2

If you prefer not to use the automated scripts, you can follow these manual steps.

**Dependencies:**

- [MSYS2](https://www.msys2.org/) - Software Distribution and Building Platform for Windows
- Java Development Kit 8 or later
  - Make sure that "Set JAVA_HOME variable" is enabled in the Adoptium installer

**Preparing MSYS2:**

1. Open one of the shortcuts from the MSYS2 folder in the Start menu
2. We recommend building using the UCRT64 or CLANG64 msystem of MSYS2
3. Install helpers:

   ```bash
   pacman -Syu pactoys git mingw-w64-ucrt-x86_64-binutils
   ```
4. Install all build dependencies using pacboy:

   ```bash
   pacboy -S toolchain:p cmake:p ninja:p qt6-base:p qt6-5compat:p qt6-svg:p qt6-imageformats:p quazip-qt6:p extra-cmake-modules:p ccache:p
   ```

   Alternatively you can use Qt 5 (for older Windows versions):

   ```bash
   pacboy -S toolchain:p cmake:p ninja:p qt5-base:p qt5-svg:p qt5-imageformats:p quazip-qt5:p extra-cmake-modules:p ccache:p
   ```

**Compile from command line:**

```bash
# Navigate to the source folder
cd /path/to/LunaLauncher

# Configure CMake (Debug build)
cmake -Bbuild -DCMAKE_INSTALL_PREFIX=install -DENABLE_LTO=ON -DCMAKE_BUILD_TYPE=Debug -G Ninja

# For Release build, replace Debug with Release above

# Optional: Add -DLauncher_QT_VERSION_MAJOR=5 for Qt 5
# Optional: Add -DCMAKE_CXX_COMPILER_LAUNCHER=ccache for faster recompilations

# Build
cmake --build build

# Install
cmake --install build

# For portable build (data stored in application directory)
cmake --install build --component portable
```

**Note for Qt 5 builds:** When building with Qt 5, the OpenSSL DLLs aren't automatically copied. Copy them manually:

```bash
cp /ucrt64/bin/libcrypto-1_1-x64.dll /ucrt64/bin/libssl-1_1-x64.dll install
```

Replace `ucrt64` with your msystem (e.g., `clang64`).

### Manual Build on Windows with MSVC

**Dependencies:**

- [Visual Studio](https://visualstudio.microsoft.com/) - Software Distribution and Building Platform for Windows
  - If you don't want to install the Visual Studio IDE, download 'Build Tools for Visual Studio' instead
  - Select 'Desktop development with C++'
  - CMake will be selected in the optional components
- Java Development Kit 8 or later
- [Qt](https://www.qt.io/)
  - For Qt 6 (Qt 6.6.2 is recommended), 'Qt 5 Compatibility Module' & 'Qt Image Formats' are required
  - For Qt 5 (Qt 5.15.2 is recommended), OpenSSL Toolkit is required

**Compile from command line:**

You will need to run commands from **x64 Native Tools Command Prompt for VS 2022** (or corresponding VS version).

```cmd
REM Navigate to the source folder
cd C:\Path\To\LunaLauncher

REM Configure CMake (adjust Qt path as needed)
cmake -Bbuild -DCMAKE_INSTALL_PREFIX=install -DENABLE_LTO=ON -DCMAKE_PREFIX_PATH=C:\Qt\6.6.2\msvc2019_64\lib\cmake

REM Build (Debug)
cmake --build build --config Debug -- /p:UseMultiToolTask=true /p:EnforceProcessCountAcrossBuilds=true

REM Install
cmake --install build --config Debug

REM For portable build
cmake --install build --config Debug --component portable
```

**Note for Qt 5 builds:** Copy OpenSSL DLLs manually:

```cmd
robocopy C:\Qt\Tools\OpenSSL\Win_x64\bin\ install libcrypto-1_1-x64.dll libssl-1_1-x64.dll
```

### Linux and macOS

For Linux and macOS builds, please refer to the upstream Prism Launcher build instructions:

- [https://prismlauncher.org/wiki/development/](https://prismlauncher.org/wiki/development/)

The build process and requirements are largely identical, aside from project naming.

---

## IDEs and Tooling

### ccache

ccache is a compiler cache that speeds up recompilation by caching previous compilations.

- **MSYS2:** Install with `pacboy -S ccache:p` and add `-DCMAKE_CXX_COMPILER_LAUNCHER=ccache` to CMake
- **MSVC:** Requires ccache 4.7.x or newer. Copy `ccache.exe` and rename to `cl.exe`, then add `/p:CLToolExe=cl.exe /p:CLToolPath=<path to ccache cl>` to build arguments

### VS Code

1. Install the [C/C++ extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools)
2. Configure C/C++: Edit Configurations (UI)
   - Add Qt include folder to `includePath`
   - Add `-L/{path to Qt}/lib` to `compilerArgs`
   - Set `compileCommands` to `${workspaceFolder}/build/compile_commands.json`
   - Set `cppStandard` to `c++17` or higher
3. Reconfigure CMake with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`

Example `.vscode/c_cpp_properties.json`:

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

1. Open CLion → File → Open → Select source folder
2. Settings → Build, Execution, Deployment → Toolchains
   - Set CMake, Make, C Compiler, C++ Compiler, Debugger
3. Settings → Build, Execution, Deployment → CMake
   - Set Build directory to `build`
4. Create a new configuration with Target: All targets
5. Build and Run with the buttons

### Qt Creator

1. Install Qt Creator via MSYS2: `pacboy -S qt-creator:p`
2. Open Qt Creator from MSYS2 shell: `qtcreator`
3. File → Open File or Project → Select `CMakeLists.txt`
4. Click "Configure Project" at the bottom right
5. Press the "Run" button

---

## Acknowledgements

Luna Launcher would not be possible without the work of earlier projects and their contributors.

- **Prism Launcher** — for maintaining a modern, open, and community-driven Minecraft launcher.
- **MultiMC** — the original project that laid the foundation for many third-party Minecraft launchers.

We sincerely thank all contributors to these projects for their long-standing efforts in the Minecraft community.

---

## Forking / Redistribution Policy

You are free to fork, redistribute, and provide custom builds as long as you follow the terms of the [license](LICENSE).

If you make code changes (rather than only packaging):

- Make it clear that your fork is **not** Prism Launcher and is **not** endorsed by or affiliated with the Prism Launcher project.
- Go through [CMakeLists.txt](CMakeLists.txt) and change any upstream API keys to your own, or set them to empty strings (`""`) to disable the related functionality.

If you are building this software for a distribution, please set `Launcher_BUILD_PLATFORM` to an appropriate identifier (for example: `archlinux`, `fedora`, `nixpkgs`).

---

## License

All launcher code is available under the **GPL-3.0-only** license.

The logo and related assets are licensed under **CC BY-SA 4.0**.
