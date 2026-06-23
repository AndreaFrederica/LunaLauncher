---
name: meson-package-export-distribution
description: Install Meson projects correctly, export pkg-config metadata, version libraries, create release tarballs, and prepare packages for downstream consumers. Use when the user asks about install layout, pkg-config, DESTDIR, meson dist, releases, or packaging.
---

# Meson Package Export and Distribution

Use this skill when the goal is to ship a project cleanly to other users, package managers, or downstream build systems.

Runnable examples live under this skill's `examples/` directory.

## Install the right things

```meson
project(
  'example',
  'c',
  'cpp',
  version: '0.1.0',
  meson_version: '>=1.7.0',
  license: 'MIT',
  license_files: ['LICENSE'],
  default_options: [
    'c_std=c17',
    'cpp_std=c++20',
    'warning_level=3',
  ],
)
```

```meson
inc = include_directories('include')
lib = library(
  'mylib',
  'src/mylib.cpp',
  include_directories: inc,
  version: '1.2.0',
  soversion: '1',
  install: true,
)

install_headers('include/mylib/mylib.hpp', subdir: 'mylib')
```

This installs the public API and keeps private implementation details out of the package.

## Export a pkg-config file

```meson
pkg = import('pkgconfig')
pkg.generate(
  lib,
  description: 'My reusable library',
  filebase: 'mylib',
  subdirs: 'mylib',
)
```

That makes the library easy to consume from other Meson projects and from non-Meson consumers that use pkg-config.

## ABI and shared library versioning

If the library has a stable ABI, version it intentionally:

- `version` tracks the project release
- `soversion` tracks the ABI family

Use both when the library is intended for downstream reuse.

## Relocatable installs

Prefer install layouts that do not bake build-tree paths into installed files. Keep generated `.pc` files and config files portable when possible.

If a package needs a prefix-relative install, structure it deliberately and document the assumption.

## Support staging installs

Use `DESTDIR` during packaging and keep prefixes relocatable.

```bash
meson install -C builddir --destdir "$pkgdir"
```

## Release tarballs

Use `meson dist` for source distributions when the project is set up to build from a clean tree.

```bash
meson dist -C builddir
```

## Downstream packaging targets

A good packaging story should cover:

- Linux distro packaging
- local binary package staging
- reproducible release tarballs
- developer installs under a custom prefix
- SDK and sysroot exports when relevant

## Interoperability options

### pkg-config

Use for projects that expose libraries to many consumers.

### CMake package exports

Use when downstream users primarily consume via CMake and need package config files.

### vcpkg and Conan

Document how the project fits those ecosystems instead of assuming every consumer is a Meson user.

## Packaging checklist

- install headers intentionally, not accidentally
- version shared libraries with `version` and `soversion`
- export a dependency object for in-tree reuse
- generate pkg-config metadata for downstream use
- keep runtime data and developer-only files separate
- avoid hardcoded install paths
- document whether the install layout is relocatable
- ensure the installed package can be consumed from another build tree

## Common mistakes

- installing private headers as public API
- shipping unversioned shared libraries for a stable ABI
- forgetting pkg-config metadata
- embedding build-tree paths in installed files
- exporting a build-helper target instead of the actual public library interface

## What belongs elsewhere

- tests and install commands → `meson-testing-installation`
- build-tree structure and dependencies → `meson-advanced-project-design`
- complete templates → `meson-patterns-cookbook`


