---
name: meson-debugging-troubleshooting
description: Diagnose Meson setup, compile, test, dependency, and install failures. Use for mysterious build errors, stale build directories, dependency resolution problems, cross-file issues, backend mismatches, and configuration drift.
---

# Meson Debugging & Troubleshooting

Use this skill when the build is not behaving the way the user expects and the failure is not just a simple command typo.

## Start with the build shape

Before changing code, inspect what Meson believes the build looks like:

```bash
meson introspect builddir --buildoptions
meson introspect builddir --targets
meson introspect builddir --dependencies
meson introspect builddir --tests
```

## Debugging order

1. read the exact error message
2. identify whether the error happens at setup, compile, test, or install time
3. inspect Meson's view of the build with introspection
4. check the source tree and build directory for drift
5. check whether the issue is actually a compiler, linker, or dependency problem

## Setup problems

- wrong compiler selected
- stale build directory
- invalid machine file
- option mismatch
- dependency not found during configuration

## Compile problems

- missing include path
- missing target dependency
- source file omitted from target
- generated source not wired into the target
- compiler-specific warning or error

## Test problems

- test binary depends on runtime paths
- test assumes the source directory is writable
- test assumes a build-tree path that changed
- test needs an `exe_wrapper` in cross builds

## Install problems

- target not marked `install: true`
- install helper not used
- missing header or data file installation rule
- staging tree mismatch

## Common mistakes

- changing the toolchain without wiping or reconfiguring
- assuming Meson silently rescans every external dependency on every command
- treating stale build directories as “Meson bugs”
- hiding the real problem behind a too-small snippet

## What belongs elsewhere

- setup options and defaults → `meson-setup-configuration`
- target declarations and compile selection → `meson-compile-targets`
- cross files and machine configuration → `meson-cross-compilation`
- tests and install commands → `meson-testing-installation`
- fallback dependencies and wraps → `meson-subprojects-management`

## Related setup guidance

For language standard selection during migration, see `meson-setup-configuration/SKILL.md`. This skill focuses on debugging and troubleshooting.
