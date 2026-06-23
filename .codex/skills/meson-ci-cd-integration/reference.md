# Meson CI/CD Integration — Reference

## Recommended caches

- compiler cache: `~/.cache/ccache` or `~/.cache/sccache`
- wrap download cache: `subprojects/packagecache`
- package artifacts: CI artifacts, not source-control caches

## Recommended job shape

1. checkout
2. install Meson and Ninja
3. restore compiler and wrap caches
4. `meson setup`
5. `meson compile`
6. `meson test`
7. optional `meson install --destdir`
8. optional packaging or `meson dist`

## When to avoid caching `build/`

Avoid caching the build directory when any of the following changed:

- Meson version
- compiler version or identity
- source tree contents
- machine file contents
- native file contents
- `project()` options or `-D` flags

A restored build tree can be useful only when the environment is effectively the same build.

## Helpful commands

```bash
meson setup builddir --reconfigure --clearcache
meson compile -C builddir -v
meson test -C builddir --print-errorlogs
meson install -C builddir --destdir stage
```

## Release jobs

Keep release jobs separate from normal test jobs and prefer a clean configure there.


