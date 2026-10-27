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
- **Mirror API Support**
- **Fluent Themes**: Built-in Fluent Dark/Light themes and icon resources
- **New UI Layout**: Optional experimental 3-column layout (requires restart)
- **Server Preview**: Quick server preview access from the toolbar
- **Custom Models**: Support custom player models including the Yes Steve model
- **Schematic Files**: Resource management support for Minecraft schematic files
- **Cross-platform**: Available for Windows, Linux, and macOS
- **NeoUI Formwork**: Use any web tool to build your UI

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
| `pixi run install-webui` | Configure, compile, install and deploy launcher + optional Neo Web UI |
| `pixi run run-webui` | Run the installed Neo UI |
| `pixi run webui-assets` | Type-check and export a standalone Web UI bundle |
| `pixi run webui-dev` | Start frontend browser preview |

For the complete Web UI workflow, bridge API, external bundles, and lifecycle
contract, see [Web UI development](docs/WEBUI-DEVELOPMENT.md) (Chinese).
`install-webui` defaults to `install-webui-release`; the standard `install` task
keeps Neo UI disabled. Both workflows use Pixi's pnpm/Rust and Meson.

#### Build Profiles

| Profile | Description |
|---------|-------------|
| `release` (default) | Release build with static libraries |
| `debug` | Debug build with shared libraries |
| `linux-x64-gcc-release` | Linux cross-compile release build |

```bash
# Build in debug mode
pixi run build debug

# Build for Linux (cross-compile from Windows)
pixi run build linux-x64-gcc-release
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

| Option | Description | Default |
|--------|-------------| ------- |
| `-Dbuild_testing` | Disable tests (default) | `false` |
| `-Dbuild_updater` | Disable auto-updater | `false` |
| `-Denable_java_downloader` | Enable Java auto-downloader | `true` |
| `-Ddeveloper_mode_build` | Enable developer mode | `false` |
| `-Dgamemode` | Enable GameMode support (Linux) | `enabled` |
| `-Db_vscrt` | Use MSVC dynamic runtime (Windows) | `md` |

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

<!-- contributors:start -->
## Contributors

Luna Launcher is built upon Prism Launcher and the work of its contributors. We sincerely thank everyone who made the upstream project possible.

### Luna contributors

Authors identified from Luna-specific commits, plus manually acknowledged contributors.

- Andrea Frederica
- AndreaFrederica
- AuraElicase
- copilot-swe-agent\[bot\]
- kasuganomeow
- Octol1ttle
- Trial97
- W-874

### Luna co-authors

Co-authors identified from Luna-specific commits.

- AndreaFrederica
- Claude

### Other Luna contributors

_No contributors identified yet._

### Prism Launcher contributors

Authors identified from the complete history of Prism Launcher's default branch.

<details>
<summary>Show upstream contributors</summary>

- 0x189D7997
- 2lay
- 44trent3
- A\_D
- Aaron
- Aaron Sonin
- abhicommands
- AbigailBuccaneer
- ACGaming
- Adrian
- Adrien
- ADudeCalledLeo
- Affe Ali
- AhmedAliElashmawy
- Aiden J Erickson
- alcremieow
- Alejandro González
- Alex
- Alex Sieberer
- Alexandru Ionut Tripon
- Alexia
- AlexTMjugador
- Alfio
- Alfred Wingate
- AliceDTRH
- Anceph
- Andrei Damian
- Andrew
- Andrew Okin
- Andrey Kurlin
- Anirudh Vempati
- Anthony Parrott
- Apix
- AppleTheGolden
- arch1t3cht
- Archy
- Arcitec
- arnfaldur
- ashka
- Ashleigh
- ashuntu
- asie
- ask6155
- AtomHare
- Awqre
- Babbaj
- BalkanMadman
- Ben Westover
- Benjamin Hoffmeyer
- Bensuperpc
- bexnoss
- bit6tream
- Blake Batson
- bolli24
- Bowser65
- Brottweiler
- bunchy7s
- byquanton
- captivator
- Carl Philipp Reh
- Cart
- cat
- cecilia
- Celeste Peláez
- Charles Milette
- Charlotte 🦝 Delenk
- chmodsayshello
- chouzz
- Chris Lane
- Christopher Hasse
- Chrono
- Chrono-byte
- cidkid
- cidkidnix
- Cinnamon
- circuit10
- clague
- Cleo John
- clickdevin
- Colbie Dison
- ColonelGerdauf
- comp500
- coolguy1842
- CreeperzEdge
- crpz1
- cullvox
- dada513
- Dallas Strouse
- Danhoby
- Daniel Huang
- Daniel Schemp
- Danny
- Danny Dorazio
- David Kaluta
- DavidoTek
- deadmeu
- deiseil
- Desoroxxx
- desvaters
- DioEgizio
- Dominic Monroe
- Dominik Pakuła
- Dylan
- Dylan Renwick
- Dylan Schooner
- DylanJOrr
- Dāvis Mosāns
- Echo J
- Edgar Onghena
- Edgars Cirulis
- Edgars Cīrulis
- Edvin Bryntesson
- eff3ry
- Eldred Habert
- Elise
- Elliott Tallis
- Elros
- embeddedt
- Eonfge
- erik.lundstedt
- ErogigGit
- Evan Goode
- Ezekiel Smith
- Fayne Aldan
- Felix Schnabel
- Finian Wright
- Finn Behrens
- flcmc
- flow
- fn2006
- forkiesassds
- Forkk
- Fourmisain
- frapank
- frigtear
- FyberOptic
- Ghalid
- Ghosty
- Gideon9212
- Gingeh
- Glitch
- glowiak
- Graeme Geldenhuys
- Greenphlem
- guihkx
- Guilherme Espada
- Gytis Ivaskevicius
- Hamish Arblaster
- hanlie
- Harry Peach
- haykam821
- Hazel
- Hazel Hofmann
- he3als
- helpimnotdrowning
- Henrique Kirch Heck
- Hibi
- Hubcapp
- hyperfang8
- HyperSoop
- iambob314
- iarspider
- icelimetea
- iGerman00
- Ilan Joselevich
- Ilia
- ImperatorStorm
- Indrale Dnyaneshwar
- Irgendwer01
- Ismail
- isoraqathedh
- IThundxr
- iTrooz
- Ivan Ip
- Ivan Puntiy
- izder456
- Işık
- Jackson Huffstutler
- Jacob Gogichaishvili
- Jakub Wroński
- James Beddek
- James Zhou
- Jamie Mansfield
- Jan Dalheimer
- Jan200101
- Jannis Lübke
- Janrupf
- Japa
- Jeffery Tolmie
- Jeremy Lorelli
- Jim Ramsay
- Jitter
- jn64
- Johannes Michael Andreas Merl
- John C. Allwein
- Joona
- jopejoe1
- Joseph Turner
- Joshua Anderson
- Joshua Goins
- Joshua Stone
- Josiah Glosson
- Joël Troch
- K Henriksson
- Kaeptm Blaubaer
- Kaiyang Wu
- Kat Tschan
- Kationor
- kb1000
- Kenneth Chew
- Keri
- KGB-8375
- Kilobyte
- Kilobyte22
- Kim2091
- Kode
- Kolja
- KosmX
- ktheticdev
- kumquat-ir
- Kurumi78
- LAHarbottle
- leia uwu
- Leland Liu
- Lenny McLennington
- LennyMcLennington
- Leo
- leo78913
- leozamboni
- LocalSpook
- Loetkolben
- LostLuma
- Ludgie
- Lukas Eschbacher
- Lukas Haigner
- lumiscosity
- m0nstrum
- MandipJoshi
- maple!
- Marcelo Hernandez
- Marco von Rosenberg
- Mark Deneen
- maskers
- Mason Rocha
- Matej Kafka
- Matthew\_Cash
- matthewperiut
- Max Nordlund
- max96at
- mctaylors
- Megumin
- meguminloli
- Metehan
- Mia Moir
- Michael Tyson
- MinecraftZuriki
- Mitchell Skaggs
- MMK21Hub
- moehreag
- Moresteck
- morsz
- mortu
- Mrazek, Petr
- MrMelon
- Muhammad Sameer Adnan
- Muslim
- Mustafa Çalışkan
- NandoBF
- Nathan
- ncete
- nea
- NewoIsTaken
- Nic
- Nightly
- Nikhil B
- Nikita Mitasov
- Noah
- Noah Mayr
- NotNite
- Octol1ttle
- OldWorldOrdr
- oosh
- Oreoezi
- Orochimarufan
- OverMighty
- Owen Wang
- Ozynt
- p-schneider
- Pagwin
- PandaNinjas
- Pavel Dydyshka
- Petr Mrazek
- Petr Mrázek
- Philip T
- Philipp David
- phit
- Piper McCorkle
- porter milton
- Puqns67
- Qaz
- QazCetelic
- Rachel Powers
- Radu Butoi
- Rafael Ristovski
- RaptaG
- Redson
- RedsonBr140
- redstrate
- Regular Baf
- renner
- renovate\[bot\]
- Reuben Sonnenschein
- rfl890
- Richard Voigtmann
- Richard-Rogalski
- Riley Richard
- Robb
- robotbrain
- robotbrainify
- Russell Banks
- Ryan Cao
- SabrePenguin
- Sambhav Saggi
- Sameer
- Samisafool
- Samuel Rakitničan
- Samuel Stidham
- SandaruKasa
- Santiago Cézar
- Sebastian
- Sebastian Rueth
- Sebastian Rüth
- Sebastían
- Sefa Eyeoglu
- Sergey Shatunov
- Sergio
- seth
- Seth Flynn
- Sheldon Corkery
- sinidiem
- Sky
- Skyler Grey
- so5iso4ka
- Sofia
- solonovamax
- Soni L
- Sophie Marie
- Soup
- Soup of the tomato kind
- Spencer Burris
- srakitnican
- src\_resources
- sshcrack
- StaticRocket
- Stefan
- Sticks
- Stiepen
- Stiepen22
- stoltsvensk
- Stypox
- swirl
- SwitchAxe
- swurl
- Taeyeon Mori
- TakSuyu
- Taylor Smock
- Tayou
- Technous285
- telans
- Temm
- Thalia Nero
- TheCodex6824
- TheCourierNV
- TheDoctorsLife
- TheEvilSkeleton
- TheFrenchGhosty
- TheKodeToad
- TheLastRar
- theMackabu
- TheOPtimal
- therealfarfetchd
- Thomas Kain
- Thomas Sirack
- thonkdifferent
- Tianhao Chai
- tildejustin
- Tim Flynn
- timoreo
- timoreo22
- tjw123hh
- tobfos
- Tobias Möritz
- tobimori
- Toby Foster
- Tomasz
- Tomasz Kramkowski
- TreyRuffy
- Trial97
- Trisave
- Trung Lê
- txtsd
- umutcagand
- Una
- unnameduser5000
- UnownPlain
- Vance
- Vedant
- Victor
- Vir Chaudhury
- Vishrut Sachan
- Vladislav Laetansky
- Wechirok
- WhiXard
- William Gray
- Wouink
- XandarNull
- Xander
- xnrand
- yash
- Yihe Li
- Yundi Xu
- Zeke
- Zetvue
- Zipdox
- 快乐的老鼠宝宝
- 雪鈴 SnowLin

</details>
<!-- contributors:end -->

---

## License

All old Prism launcher code is available under the **GPL-3.0-only** license.

Some new Luna code is available under the **MPL2.0** license. **You can use it in your project.**

The logo and related assets are licensed under **CC BY-SA 4.0**.
