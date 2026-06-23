# Meson Subprojects Management — Reference

## Wrap file types

- `wrap-file` for tarball-based downloads
- `wrap-git` for git-based sources
- patch files for local fixes

## Dependency resolution policy

A well-structured project usually tries the system dependency first, then falls back to a wrap only when necessary.

```meson
foo_dep = dependency('foo', required: false)
if not foo_dep.found()
  foo_proj = subproject('foo')
  foo_dep = foo_proj.get_variable('foo_dep')
endif
```

## Subproject boundaries

- subprojects live under `subprojects/`
- the parent project owns top-level options and global build policy
- the subproject owns its own local implementation details
- export a `declare_dependency()` object for consumers

## Wrap cache cleanup

Use `meson subprojects purge --confirm --include-cache` when wrap data is stale or inconsistent.

## Anti-patterns

- calling `subproject()` from deep inside nested build logic
- setting compiler globals inside a subproject
- hardcoding vendored dependency use when a system package is acceptable


## Subproject language standard policy

Subprojects inherit language defaults from the parent project's `default_options` only when they do not set their own. If a subproject explicitly sets `c_std` or `cpp_std` in its own `project()` defaults, those win for that subproject's targets.

When vendoring an existing library as a subproject, preserve its original language standard to avoid subtle ABI or behavioral differences. For new subprojects, prefer the same standards as the parent project unless the subproject targets an older embedded environment.
