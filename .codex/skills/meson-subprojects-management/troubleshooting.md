# Meson Subprojects Management — Troubleshooting

## `Subproject not found`

Check that the wrap file exists and that fallback is enabled in the parent project.

## Wrong version is picked

A wrap or system dependency may be older than expected. Confirm the dependency choice with the Meson log and the wrap files in `subprojects/`.

## The subproject keeps rebuilding unexpectedly

Check whether the build directory was reused with changed source trees, wrap files, or options. If so, reconfigure or wipe the build directory.

## The subproject should be vendored, but packaging forbids it

Prefer a system dependency and keep fallback only as a development convenience when the downstream policy allows it.

## Stale wrap cache

Purge the wrap cache and fetch again:

```bash
meson subprojects purge --confirm --include-cache
meson subprojects download
```


## Subproject language standard policy

Subprojects inherit language defaults from the parent project's `default_options` only when they do not set their own. If a subproject explicitly sets `c_std` or `cpp_std` in its own `project()` defaults, those win for that subproject's targets.

When vendoring an existing library as a subproject, preserve its original language standard to avoid subtle ABI or behavioral differences. For new subprojects, prefer the same standards as the parent project.
