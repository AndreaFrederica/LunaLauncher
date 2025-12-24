<p align="center">
<img alt="Luna Launcher" src="/program_info/lunalauncher.png" width="40%">
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

```powershell
# 1. Bootstrap MSYS2 environment (first time only)
.\tools\msys2\bootstrap.ps1

# 2. Configure & build
.\tools\msys2\run.ps1 configure
.\tools\msys2\run.ps1 build

# 3. Install
.\tools\msys2\run.ps1 install
```

**Available Commands:**

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

Edit `msys2.toml` to add/remove packages, then run `.\tools\msys2\sync.ps1`.

Or use the helper scripts:
```powershell
.\tools\msys2\add.ps1 mingw-w64-ucrt-x86_64-qt6-tools
.\tools\msys2\remove.ps1 mingw-w64-ucrt-x86_64-qt6-tools
```

**Inside MSYS2 Shell:**

```powershell
.\tools\msys2\shell.ps1
```

Then use bash equivalents:
```bash
./tools/msys2/sync.sh
./tools/msys2/run.sh configure
./tools/msys2/run.sh build
```

#### Option B: Pixi + MSVC

Uses Pixi with MSVC toolchain, managed via `pixi.toml`. Requires [Pixi](https://pixi.sh) to be installed.

```powershell
# 1. Install Pixi (if not already installed)
# Visit https://pixi.sh/latest/installation/

# 2. Install dependencies and configure
pixi run configure

# 3. Build
pixi run build

# 4. Install
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
