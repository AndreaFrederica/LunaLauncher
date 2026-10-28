# Meson Debugging & Troubleshooting — Troubleshooting

## Dependency not found

Check whether the dependency should come from:

- the system
- a pkg-config file
- a CMake package
- a wrap fallback
- a subproject

## Wrong target selected

List targets with:

```bash
meson introspect builddir --targets
```

Then use the exact target name.

## Build stays stale

If the build tree looks stale, refresh the configuration:

```bash
meson setup builddir --reconfigure --clearcache
```

If that does not help, wipe the build tree.

## Cross build crashes at runtime

Confirm whether the target can actually run on the host machine. If not, use an `exe_wrapper` or restructure the test.

## Generated file is missing

Check that the generated file is part of the target source list or is produced through `custom_target()` / `generator()` and consumed correctly.

## Common mistakes

- solving the symptom before checking the actual dependency graph
- leaving the wrong build tree around after a toolchain switch
- assuming `meson test` and `meson compile` share the same failure cause
- forgetting to inspect the backend command line with `-v`

## Language Standard Selection

For language standard selection guidance consult `meson-setup-configuration/SKILL.md` — this skill focuses on debugging and troubleshooting.
