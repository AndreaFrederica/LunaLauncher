---
name: meson-cross-compilation
description: Set up cross-compilation for non-native targets with Meson machine files and toolchain configuration. Use this skill whenever the user is building for a different platform or architecture, mentions a cross file or native file, asks about toolchain configuration, or hits errors related to architecture mismatches, sysroots, or missing cross-compilers.
---

# Meson Cross-Compilation

Cross-compilation means building on one machine for another machine. Meson uses machine files to describe the compiler, target platform, and any target-specific properties.

Runnable examples live under this skill's `examples/` directory.

## Quick start

```ini
# cross.ini
[host_machine]
system = 'linux'
cpu_family = 'arm'
cpu = 'armv7l'
endian = 'little'

[binaries]
c = '/usr/bin/arm-linux-gnueabihf-gcc'
cpp = '/usr/bin/arm-linux-gnueabihf-g++'
ar = '/usr/bin/arm-linux-gnueabihf-gcc-ar'
strip = '/usr/bin/arm-linux-gnueabihf-strip'
exe_wrapper = '/usr/bin/qemu-arm'

[properties]
sys_root = '/opt/arm-sysroot'
pkg_config_libdir = '/opt/arm-sysroot/usr/lib/pkgconfig'
toolchain_prefix = 'arm-linux-gnueabihf'

[built-in options]
c_std = 'c17'
cpp_std = 'c++20'
warning_level = '3'
```

Then configure with:

```bash
meson setup builddir --cross-file cross.ini
```

## What a machine file controls

- `build_machine`: the machine doing the compiling
- `host_machine`: the machine that will run the outputs
- `target_machine`: only needed for toolchains that produce tools for a third machine
- `[binaries]`: compilers, linkers, archivers, wrappers, and helper executables
- `[properties]`: custom machine metadata, sysroot-related settings, and other values read with `meson.get_external_property()`
- `[built-in options]`: compiler, linker, and language-standard options such as `c_args`, `c_link_args`, `c_std`, `cpp_std`, and `warning_level`

## Practical rules

- Use `meson.get_external_property()` for custom values from native or cross files.
- Put compiler and linker flags in `[built-in options]`, not in `[properties]`.
- Set `pkg_config_libdir` when the target sysroot has its own pkg-config metadata.
- Use `exe_wrapper` when the target binaries cannot run on the build machine.
- Keep machine files under version control when the project depends on them.
- Put generated build-machine tools in a native file, not the cross file.
- Separate host tools from target libraries.

## Common cross-build shapes

- Linux to Windows with MinGW toolchains
- Linux to another Linux architecture
- macOS to iOS or another Apple target with the appropriate SDK setup
- host-specific toolchain builds in CI containers
- Windows native builds that still need a native file for host-side generators

## When to switch to native files

Use a native file for build tools that must run on the build machine, such as code generators, formatters, or helper executables.

## [properties] vs [built-in options]

- `[properties]` is for machine-specific metadata and custom values that Meson reads with `meson.get_external_property()`.
- `[built-in options]` is for compiler and linker configuration, including standards, warning levels, and argument lists.

If a setting changes the compiler invocation, it belongs in `[built-in options]`.

## When not to blame Meson

If the compiler, linker, or sysroot is wrong, Meson is usually exposing a toolchain problem rather than causing it. Check the machine file first.

## Common mistakes

- putting `c_args`, `cpp_args`, or standard selections in `[properties]`
- forgetting `pkg_config_libdir`
- using host libraries from the build machine instead of the sysroot
- assuming the build machine can run target binaries directly
- forgetting to use a native file for build-only helper programs

## Language Standard Selection

Language standards are set via `[built-in options]` in the machine file (see `meson-setup-configuration/SKILL.md` for the authoritative guidance).
