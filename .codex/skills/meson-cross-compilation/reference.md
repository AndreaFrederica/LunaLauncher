# Meson Cross-Compilation — Reference

## Machine file sections

### `[host_machine]`

Describes the platform that will run the resulting binaries.

```ini
[host_machine]
system = 'linux'
cpu_family = 'x86_64'
cpu = 'x86_64'
endian = 'little'
```

### `[binaries]`

Use absolute paths or stable tool names for the compiler toolchain.

```ini
[binaries]
c = '/usr/bin/x86_64-w64-mingw32-gcc'
cpp = '/usr/bin/x86_64-w64-mingw32-g++'
ar = '/usr/bin/x86_64-w64-mingw32-gcc-ar'
strip = '/usr/bin/x86_64-w64-mingw32-strip'
exe_wrapper = '/usr/bin/wine'
```

### `[properties]`

Use properties for custom metadata and sysroot-related settings.

```ini
[properties]
sys_root = '/opt/sysroot'
pkg_config_libdir = '/opt/sysroot/usr/lib/pkgconfig'
toolchain_prefix = 'x86_64-w64-mingw32'
```

### `[built-in options]`

Use built-in options for compiler, linker, and language settings.

```ini
[built-in options]
c_std = 'c17'
cpp_std = 'c++20'
warning_level = '3'
c_args = ['-O2']
c_link_args = ['-Wl,--as-needed']
```

## Accessing custom values

Use the modern property accessor:

```meson
prefix = meson.get_external_property('toolchain_prefix', '')
```

Avoid `meson.get_cross_property()`. The modern accessor works for both cross and native files.

## Running target binaries during tests

If the target binary can run on the build machine through a wrapper, set `exe_wrapper`. If it cannot run, structure tests so they are skipped or use host-side test helpers instead.

## Native files

Use a native file for build tools that run on the build machine:

```ini
[binaries]
python = '/usr/bin/python3'
```

Then use those tools for generators, formatters, and packaging helpers.

## Common mistakes

- putting `c_args` in `[properties]`
- forgetting `pkg_config_libdir`
- using host libraries from the build machine instead of the sysroot
- assuming the build machine can run target binaries directly
- putting generators for build tools into the cross file

## Language Standard Selection

Language standards are set via `[built-in options]` in the machine file. See `meson-setup-configuration/SKILL.md` for the authoritative language standard selection guidance.
