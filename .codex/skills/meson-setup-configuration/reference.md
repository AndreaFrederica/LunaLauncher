# Reference: meson setup Command-Line Options

## Installation directories

| Option | Purpose | Example |
|--------|---------|---------|
| `--prefix PATH` | Installation root | `--prefix=/usr/local` |
| `--bindir PATH` | Executable directory | `--bindir=bin` |
| `--datadir PATH` | Data file directory | `--datadir=share` |
| `--includedir PATH` | Header directory | `--includedir=include` |
| `--libdir PATH` | Library directory | `--libdir=lib` |
| `--libexecdir PATH` | Library executable dir | `--libexecdir=libexec` |
| `--localedir PATH` | Locale data | `--localedir=share/locale` |
| `--mandir PATH` | Manual pages | `--mandir=share/man` |
| `--infodir PATH` | Info pages | `--infodir=share/info` |

## Build configuration

| Option | Purpose | Values |
|--------|---------|--------|
| `--buildtype` | Build type | plain, debug, debugoptimized, release, minsize |
| `--debug` | Include debug symbols | boolean |
| `--optimization {0,1,2,3,s,g}` | Optimization level | 0=none, 3=max, s=size, g=debug |
| `--strip` | Strip binaries on install | boolean |
| `--backend` | Generator backend | ninja, vs, vs2022, xcode, none |
| `--genvslite {vs2022}` | VS multiconfig helper | Visual Studio helper |

## Feature control

| Option | Purpose | Values |
|--------|---------|--------|
| `--auto-features {auto,enabled,disabled}` | Override all auto features | auto, enabled, disabled |
| `-Doption=value` | Set any option | project-defined options |
| `--warnlevel {0,1,2,3,everything}` | Compiler warning level | 0=quiet, 3=verbose |
| `--werror` | Treat warnings as errors | boolean |
| `-Db_coverage` | Code coverage | true, false |
| `-Db_sanitize` | Sanitizers | address, memory, thread, undefined |
| `-Db_lto` | Link-time optimization | true, false, thin |
| `-Db_pie` | Position-independent exec | true, false |
| `-Db_staticpic` | Static libs with PIE | true, false |
| `-Db_pgo` | Profile-guided optimization | off, generate, use |

## Dependency management

| Option | Purpose | Example |
|--------|---------|---------|
| `--wrap-mode` | Wrap dependency handling | default, nofallback, nodownload, forcefallback |
| `--force-fallback-for NAMES` | Force fallback for specific dependencies | `--force-fallback-for=zlib,openssl` |
| `--pkg-config-path PATH` | pkg-config search path | `--pkg-config-path=/usr/lib/pkgconfig` |
| `--cmake-prefix-path PATH` | CMake package path | `--cmake-prefix-path=/usr/share/cmake` |

## Cross-compilation and machine files

| Option | Purpose | Example |
|--------|---------|---------|
| `--native-file FILE` | Native file for build-machine tools | `--native-file native.ini` |
| `--cross-file FILE` | Cross file for target-machine tools | `--cross-file arm-linux.ini` |

## Build directory management

| Option | Purpose | Effect |
|--------|---------|--------|
| `--reconfigure` | Reconfigure existing build | Keeps artifacts, updates config |
| `--wipe` | Wipe and reconfigure | Removes artifacts, starts fresh |
| `--clearcache` | Clear dependency cache | Removes cached dependency info |
| `-C DIRECTORY` | Build directory for all commands | `meson compile -C mybuild` |

## Miscellaneous

| Option | Purpose |
|--------|---------|
| `--errorlogs` | Print logs from failing tests |
| `--install-umask UMASK` | Umask for installed files |
| `--layout {mirror,flat}` | Build layout |
| `-v, --verbose` | Verbose output |
| `--fatal-meson-warnings` | Treat Meson warnings as errors |

## Examples

**Setup with custom prefix:**
```bash
meson setup build --prefix=/opt/myapp
```

**Setup with LLVM/Clang and LTO:**
```bash
CC=clang CXX=clang++ meson setup build -Db_lto=thin --optimization=3
```

**Setup with AddressSanitizer:**
```bash
meson setup build -Db_sanitize=address
```

**Setup for ARM cross-compilation:**
```bash
meson setup build --cross-file arm-linux.ini
```

**Setup with code coverage:**
```bash
meson setup build -Db_coverage=true
meson compile -C build
meson test -C build
ninja -C build coverage-html
```

**Setup with Python module configuration:**
```bash
meson setup build   -Dpython.platlibdir=/usr/lib/python3.10   -Dpython.purelibdir=/usr/lib/python3/dist-packages
```

## Common mistakes

- putting standards or compiler flags into `[properties]`
- forgetting that `meson setup` locks the initial compiler choice
- relying on old build directories after changing the toolchain
- treating build types as a substitute for project policy

## Related setup guidance

See `meson-setup-configuration/SKILL.md` for authoritative language standard selection details, including the recommended `default_options` block.
