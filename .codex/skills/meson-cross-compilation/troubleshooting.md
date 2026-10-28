# Meson Cross-Compilation — Troubleshooting

## `Machine file not found`

Use an absolute path or confirm the relative path is correct from the source directory.

## Wrong compiler or linker is selected

Check the `[binaries]` section and make sure the toolchain prefix matches the cross compiler you actually installed.

## `pkg-config` finds host libraries

The sysroot is not isolated well enough.

**Fix:** set `pkg_config_libdir` and make sure the cross environment does not inherit host pkg-config paths.

## Target binaries cannot run during tests

Set an `exe_wrapper` if the target can be executed through a wrapper such as Wine or QEMU. If it cannot run at all, mark tests so they are skipped in that configuration.

## `meson.get_cross_property()` does not work

Use `meson.get_external_property()` instead.

## Compiler flags seem ignored

Check whether the flags were placed in the right section:

- compiler and linker flags belong in `[built-in options]`
- metadata belongs in `[properties]`

## Build machine and host machine are mixed up

Confirm whether a tool should live in a native file instead of the cross file.

## Common mistakes

- using the wrong machine file for the wrong role
- putting language standards into `[properties]`
- using host pkg-config data for target libraries
- assuming the cross file should contain build-machine helper tools

## Language Standard Selection

Language standards are set via `[built-in options]`. See `meson-setup-configuration/SKILL.md` for the authoritative guidance.
