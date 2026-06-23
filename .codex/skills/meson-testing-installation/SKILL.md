---
name: meson-testing-installation
description: Run tests and install build targets using Meson. Use this skill whenever the user wants to run tests, debug test failures, filter tests by suite or name, set up CI test pipelines, or install to a custom prefix or staging directory — even if they just say "how do I run only unit tests" or "how do I install to /opt".
---

# Meson Testing & Installation

`meson test` runs project tests, and `meson install` puts built artifacts into their final location. For pkg-config exports, release tarballs, and packaging layout, use `meson-package-export-distribution`.

Runnable examples live under this skill's `examples/` directory.

## Quick Start

**Run all tests:**
```bash
meson test -C build
```

**List available tests:**
```bash
meson test -C build --list
```

**Run specific test:**
```bash
meson test -C build test_name
```

**Install project:**
```bash
meson install -C build
```

**Dry-run (see what would be installed):**
```bash
meson install -C build --dry-run
```

## Why tests and installs are separate

Testing answers “does it work here?”
Installation answers “does the project stage correctly?”

A healthy project should prove both.

## meson test: Reference

### Basic syntax

```bash
meson test [options] [test_names...]
```

### Common options

| Option | Purpose |
|--------|---------|
| `-C BUILDDIR` | Build directory |
| `--list` | List available tests (don't run) |
| `-j JOBS` | Parallel test jobs |
| `-v, --verbose` | Verbose output |
| `--no-rebuild` | Don't rebuild before testing |
| `--gdb` | Run failing tests under gdb |
| `--benchmark` | Run benchmark tests |
| `--suite SUITE` | Run only one suite |
| `--print-errorlogs` | Print failing test logs |

## Test suites

Use suites to keep test categories readable:

- `unit`
- `integration`
- `slow`
- `compile`
- `install`

Example:

```meson
test('core', core_test, suite: 'unit')
```

## Install patterns

### Install with custom prefix

```bash
meson setup build --prefix=/opt/myapp
meson install -C build
```

### Install into a package root

```bash
meson install -C build --destdir "$pkgdir"
```

### Use a staging tree in CI

```bash
meson install -C build --destdir "$PWD/stage"
```

## Common install helpers

- `install_headers()`
- `install_man()`
- `install_data()`
- `install_subdir()`
- `install_symlink()`
- `install_emptydir()`

## Install checklist

- public headers installed intentionally
- shared library versioning set if the ABI is stable
- pkg-config metadata exported if downstream users need it
- private files kept out of the public prefix
- staging installs tested in CI

## Common mistakes

- confusing “built successfully” with “installed correctly”
- forgetting `install: true` on the target that should ship
- letting tests depend on the source directory layout
- writing tests that require a full install when a build-tree test would be simpler
- failing to check the install tree in CI


