<p align="center">
<img alt="Luna Launcher" src="/program_info/lunalauncher.png" width="40%">
</p>

<p align="center">
  <a href="README.md">English</a> | <a href="README.zh-CN.md">简体中文</a>
</p>

<p align="center">
  Luna Launcher is a custom launcher for Minecraft that allows you to manage multiple Minecraft installations with ease.<br />
  <br />
  This project is an independent <b>fork</b> of Prism Launcher and is <b>not</b> endorsed by or affiliated with the Prism Launcher project.
  <br />
  It improves accessibility by supporting community-maintained mirror APIs such as <b>BMCLAPI</b>.
</p>

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

```powershell
# 1. Initialize MSYS2 environment (first time only)
m2m init

# 2. Bootstrap MSYS2
m2m bootstrap

# 3. Configure & build
m2m run configure
m2m run build

# 4. Install
m2m run install
```

**Available m2m Commands:**

| Command | Description |
|---------|-------------|
| `m2m init` | Initialize `msys2.toml` configuration |
| `m2m bootstrap` | Download and install MSYS2 |
| `m2m run <task>` | Run a task defined in `msys2.toml` |
| `m2m run -l` | List available tasks |
| `m2m sync` | Sync packages from configuration |
| `m2m add <package>` | Add a package to configuration |
| `m2m remove <package>` | Remove a package from configuration |
| `m2m shell` | Open interactive MSYS2 shell |
| `m2m update` | Update all MSYS2 packages |

**Method 2: Using PowerShell Scripts**

Alternatively, use the provided PowerShell scripts:

```powershell
# 1. Bootstrap MSYS2 environment (first time only)
.\tools\msys2\bootstrap.ps1

# 2. Configure & build
.\tools\msys2\run.ps1 configure
.\tools\msys2\run.ps1 build

# 3. Install
.\tools\msys2\run.ps1 install
```

**Available PowerShell Commands:**

| Command | Description |
|---------|-------------|
| `.\tools\msys2\run.ps1 configure` | Configure CMake |
| `.\tools\msys2\run.ps1 build` | Build the project |
| `.\tools\msys2\run.ps1 install` | Install to `install/` |
| `.\tools\msys2\run.ps1 portable` | Create portable build |
| `.\tools\msys2\run.ps1 clean` | Clean build directories |
| `.\tools\msys2\sync.ps1` | Sync packages from `msys2.toml` |
| `.\tools\msys2\add.ps1 <package>` | Add a package |
| `.\tools\msys2\remove.ps1 <package>` | Remove a package |

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

# 1. Install dependencies and configure
pixi run configure

# 2. Build
pixi run build

# 3. Install
pixi run install
```

**Available Tasks:**

| Command | Description |
|---------|-------------|
| `pixi run configure` | Configure CMake |
| `pixi run build` | Build the project |
| `pixi run install` | Install to `install/` |
| `pixi run portable` | Create portable build |

### Other Platforms

For Linux, macOS, or manual builds, please refer to the upstream build instructions:

- <https://prismlauncher.org/wiki/development/>

The build process and requirements are largely identical, aside from project naming and backend configuration.

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
