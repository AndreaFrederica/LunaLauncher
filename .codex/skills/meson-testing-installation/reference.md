# Meson Testing & Installation — Reference

## Testing commands

```bash
meson test -C builddir
meson test -C builddir --list
meson test -C builddir --suite unit
meson test -C builddir test_name
meson test -C builddir --print-errorlogs
```

## Test metadata

Use `test()` to define tests and suites. Use `suite:` for filtering and grouping. Use `timeout:` for long-running tests and `should_fail: true` for negative tests that are expected to fail.

## Installation commands

```bash
meson install -C builddir
meson install -C builddir --dry-run
meson install -C builddir --destdir "$PWD/stage"
```

## Install helpers

- `install_headers()`
- `install_man()`
- `install_data()`
- `install_subdir()`
- `install_symlink()`
- `install_emptydir()`

## When tests fail

- `--print-errorlogs` shows the captured logs
- `--no-rebuild` is useful when you already know the build tree is fresh
- `--gdb` helps when the test binary crashes
- `--suite` helps isolate large test matrices

## When installs fail

- check `install: true`
- check `install_dir`
- check staging prefix permissions
- check whether a target is intentionally non-installable

## Good split of responsibilities

- `meson.build` decides what is testable and installable
- `meson test` runs the tests
- `meson install` copies outputs into a prefix or package root

## Common mistakes

- expecting `meson test` to run installed binaries automatically
- using a source-tree path inside a test that should be relocatable
- installing private helper tools as if they were public API

