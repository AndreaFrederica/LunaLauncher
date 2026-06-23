# Meson Debugging & Troubleshooting — Reference

## Useful commands

```bash
meson introspect builddir --buildoptions
meson introspect builddir --targets
meson introspect builddir --dependencies
meson introspect builddir --tests
meson setup builddir --reconfigure --clearcache
meson compile -C builddir -v
meson test -C builddir --print-errorlogs
meson install -C builddir --dry-run
```

## What each command helps with

- `--buildoptions` shows what Meson configured
- `--targets` shows target names and types
- `--dependencies` shows resolved dependency information
- `--tests` shows how Meson sees the test suite
- `-v` shows the backend command line
- `--print-errorlogs` shows test stderr/stdout
- `--dry-run` reveals the install plan

## Common failure shapes

- setup failure: toolchain or machine-file issue
- compile failure: source, include, or dependency issue
- test failure: runtime, fixture, or environment issue
- install failure: missing install metadata or wrong install path

## Good investigation habits

- reproduce with one command at a time
- keep the build tree and source tree separate
- write down the exact compiler and linker command if needed
- compare the failing machine file or option set to a known-good one

## Common mistakes

- guessing before checking `meson introspect`
- changing multiple build variables at once
- assuming a build-tree error is a Meson parser bug
- skipping the backend log and only reading the high-level summary

## Language Standard Selection

For language standard selection guidance consult `meson-setup-configuration/SKILL.md` — this skill focuses on debugging and troubleshooting.
