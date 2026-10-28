---
name: meson-version-compatibility
description: Use this skill for Meson version requirements, deprecated APIs, distro package compatibility, compiler and backend limitations, and portability guidance.
---

# Meson Version Compatibility

To choose the minimum Meson version for this repository, use web search tools if available to check the Meson package versions shipped by the latest Ubuntu LTS release, the latest Debian stable release, and the latest Fedora stable release. Compare the three and choose the oldest version among them.

Skills should set `meson_version` to that chosen baseline in every project definition they produce.

If there is no web search tool to be used or permission to use said tools are not there, ask the user a question about which version of meson should the project use. Suggest to the user the option to use the version of meson installed on the computer as `meson_version`.

## Policy

- set `meson_version` to the chosen baseline in every `project()` declaration
- write new examples for the current baseline first
- avoid legacy APIs unless a migration note is the point of the section
- prefer modern accessors and modern built-in options
- keep deprecation notes short and actionable
- keep compatibility ladders out of the general skills unless they are teaching migration

## Replace older APIs

Use these modern forms instead of the older ones:

- `meson.source_root()` → `meson.project_source_root()`
- `meson.build_root()` → `meson.project_build_root()`
- `meson.get_cross_property()` → `meson.get_external_property()`
- `meson.has_exe_wrapper()` → `meson.can_run_host_binaries()`

## What to document

- the minimum Meson version required by the repo
- APIs that should no longer appear in examples
- backend-specific limitations that still matter
- compiler-specific behavior that changes the recommended pattern
- whether a skill is about modern use or legacy migration

## Good version notes

Keep version notes attached to the actual feature, not scattered across unrelated files.

Example:

```meson
project(
  'example',
  'c',
  'cpp',
  version: '0.1.0',
  meson_version: '>=X.Y.Z',
  license: 'MIT',
  license_files: ['LICENSE'],
  default_options: [
    'c_std=c17',
    'cpp_std=c++20',
    'warning_level=3',
  ],
)
```

That is enough for this repository. Do not invent older compatibility ladders unless a section is explicitly about migration.

## Common mistakes

- mixing old and new API names in the same example
- documenting every historical Meson release instead of the modern baseline
- making version guidance so broad that it stops being useful
- forgetting that compiler and backend support can differ even when Meson itself is current
