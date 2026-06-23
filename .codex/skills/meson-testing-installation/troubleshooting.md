# Meson Testing & Installation — Troubleshooting

## Tests do not run

Check that the target is actually declared as a test:

```meson
test('name', test_exe)
```

If the test is not listed, Meson does not know about it.

## Failing test output is missing

Use:

```bash
meson test -C build --print-errorlogs
```

## Test depends on the build directory layout

Use absolute or Meson-provided paths instead of assuming the working directory.

## Install misses a file

Check:

- `install: true`
- `install_headers()`
- `install_data()`
- `install_dir`
- whether the file is intentionally private

## Installed files are in the wrong place

Inspect the prefix and directory options used during `meson setup`, then check the install tree with `--dry-run`.

## Staging install looks correct but runtime execution fails

This is usually a missing runtime dependency, not an installation bug. Check linked libraries and runtime search paths.

## Common mistakes

- forgetting to add the test target to `test()`
- confusing build-tree tests with install-tree tests
- not staging installs in CI before release
- letting tests rely on mutable state in the source tree

