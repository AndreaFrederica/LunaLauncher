# Troubleshooting: meson setup Issues

## `meson setup: command not found`

Install Meson first and make sure it is on `PATH`.

## Source directory already configured

The source tree already has a build directory attached. Choose the approach that matches your intent:

**A — Reconfigure in place (preserves build artifacts)**
```bash
meson setup builddir --reconfigure
```

**B — Start fresh (removes all build artifacts)**
```bash
rm -rf builddir
meson setup builddir
```

## Wrong compiler is used

The compiler was probably chosen during the first setup. Change the environment before setup, or wipe the build directory and configure again.

## Option changes are ignored

Run `meson setup builddir --reconfigure` or `meson configure builddir` after changing options.

## Stale cache after a toolchain change

Use the modern combined refresh command:

```bash
meson setup builddir --reconfigure --clearcache
```

## Cross/native file confusion

If the wrong machine file is used, Meson may configure the build successfully but produce the wrong target shape. Recheck `--cross-file` and `--native-file` separately.

## Build type does not match expectations

Confirm the build type explicitly:

```bash
meson introspect builddir --buildoptions
```

Then reconfigure with the desired build type.

## Common mistakes

- changing environment variables after `meson setup` and expecting Meson to rediscover the compiler
- editing `meson.build` and forgetting to reconfigure when options changed
- assuming `--reconfigure` fixes a bad machine file
- treating `--clearcache` as a substitute for a bad toolchain selection

## Related setup guidance

See `meson-setup-configuration/SKILL.md` for authoritative language standard selection details, including the recommended `default_options` block.
