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

> **⚠️ Build System Change: Meson Now Required**
>
> Luna Launcher has switched from **CMake** to **Meson** as its primary build system. The `CMakeLists.txt` is retained for compatibility but is **no longer actively maintained** and may have issues.
>
> **If you are backporting features from Luna to Prism Launcher or another fork:** Please be aware that build-related changes in Luna are written for Meson (`meson.build`, `meson_options.txt`, `scripts/pixi_meson.py`). You will need to adapt them to CMake if your target still uses CMake.
>
> See the [Build Guide](https://lunalauncher.sirrus.cc/build.html) for updated instructions.

---

## Features

> **Note:** Luna is an enhanced fork of Prism Launcher, adding extra features on top of Prism's foundation while maintaining full compatibility with Prism Launcher instances and functionality.
>
> We welcome developers to backport Luna's features to the upstream Prism Launcher. Luna does not actively submit PRs due to limited developer time—all Luna maintainers are hobbyist developers who also maintain other open source projects. If you're interested in contributing, please reach out to us!

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

This project uses **Meson** as its build system. The recommended way to build is via **Pixi**, which handles dependencies and environment setup automatically.

### Quick Start (Pixi)

#### Prerequisites

- [Pixi](https://pixi.sh/latest/installation/)

**Windows:** You must run commands from the **x64 Native Tools Command Prompt for VS 2022** (or corresponding VS version).

**Linux:** Ensure `gcc`, `g++`, `pkg-config`, and Qt 6 development libraries are available (Pixi will install most of them).

```bash
# 1. Configure
pixi run configure

# 2. Build
pixi run build

# 3. Install
pixi run install
```

#### Available Tasks

| Command | Description |
|---------|-------------|
| `pixi run configure` | Configure Meson build directory |
| `pixi run build` | One-shot setup + compile with Meson |
| `pixi run install` | Build and install with Meson |
| `pixi run deploy` | Run `windeployqt` and copy runtime DLLs (Windows only) |
| `pixi run install_qt` | Download/install Qt into `third_party/qt` |

#### Build Profiles

| Profile | Description |
|---------|-------------|
| `release` (default) | Release build with static libraries |
| `debug` | Debug build with shared libraries |
| `linux-x64-gcc-release` | Linux cross-compile release build |

```bash
# Build in debug mode
pixi run build --profile debug

# Build for Linux (cross-compile from Windows)
pixi run build --profile linux-x64-gcc-release
```

### Linux Native Build

On a Linux machine, you can also use Pixi directly:

```bash
# Install dependencies (Arch example)
sudo pacman -S meson ninja gcc pkgconf qt6-base qt6-svg qt6-imageformats qt6-5compat quazip-qt6 cmark

# Configure
pixi run configure

# Build
pixi run build

# Install
pixi run install
```

### Manual Meson Build

If you prefer not to use Pixi:

```bash
# Install dependencies (Ubuntu/Debian example)
sudo apt install meson ninja-build gcc g++ pkg-config qt6-base-dev qt6-svg-dev qt6-imageformats-dev qt6-5compat-dev libquazip1-qt6-dev libcmark-dev zlib1g-dev libarchive-dev liblz4-dev libzstd-dev liblzma-dev libbz2-dev libqrencode-dev

# Configure
meson setup build --buildtype=release --wrap-mode=forcefallback

# Build
meson compile -C build

# Install
meson install -C build --no-rebuild
```

#### Useful Meson Options

| Option | Description |
|--------|-------------|
| `-Dbuild_testing=false` | Disable tests (default) |
| `-Dbuild_updater=false` | Disable auto-updater |
| `-Denable_java_downloader=true` | Enable Java auto-downloader |
| `-Ddisable_ownership_check=false` | Disable ownership verification |
| `-Dgamemode=enabled` | Enable GameMode support (Linux) |
| `-Db_vscrt=md` | Use MSVC dynamic runtime (Windows) |

---

## Detailed Build Instructions

### Getting the Source

Clone the source code using git, and grab all the submodules:

```bash
git clone --recursive https://github.com/AndreaFrederica/LunaLauncher.git
cd LunaLauncher
```

The rest of the documentation assumes you have already cloned the repository.

### IDE Setup

#### VS Code

1. Install the [Meson extension](https://marketplace.visualstudio.com/items?itemName=mesonbuild.mesonbuild) for Meson support
2. Install the [C/C++ extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools)
3. Open the project folder - VS Code should auto-detect the Meson build
4. Build tasks are available via `Ctrl+Shift+B`

#### CLion

1. Open CLion → File → Open → Select source folder
2. CLion will automatically detect the Meson project
3. Settings → Build → Meson → Set build directory if needed
4. Build and Run with the buttons

#### Qt Creator

1. Install Qt Creator
2. File → Open File or Project → Select the project root (Qt Creator detects `meson.build`)
3. Configure the project when prompted
4. Press the "Run" button

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
- Go through `meson.build` and change any upstream API keys to your own, or set them to empty strings (`""`) to disable the related functionality.
- For packaged system builds, set `--prefix=/usr`. Avoid `/usr/local` for distro-managed packages.

For Fedora/COPR packaging, see [packaging/fedora/README.md](packaging/fedora/README.md).

---

## License

All launcher code is available under the **GPL-3.0-only** license.

The logo and related assets are licensed under **CC BY-SA 4.0**.
