---
name: meson-build-system
description: Entry point for Meson questions. Use this skill for general Meson syntax, project structure, build directory basics, and the standard setup-compile-test-install flow. Route option setup, target building, testing, cross-compilation, subprojects, CI, debugging, version compatibility, packaging, and pattern examples to the specialized skills below.
---

# Meson Build System

Meson is a declarative build system. Start here when the user asks about `meson.build` syntax, target declarations, build directories, or the standard workflow of setup, compile, test, and install.

## Fast path

1. Write or edit `meson.build`
2. Configure a build directory with `meson setup`
3. Build with `meson compile -C <builddir>`
4. Run tests with `meson test -C <builddir>`
5. Install with `meson install -C <builddir>`

## Route to the right skill

- **New project or option setup** → `meson-setup-configuration`
- **Build a target or inspect target names** → `meson-compile-targets`
- **Run tests or install into a staging prefix** → `meson-testing-installation`
- **Different compiler, platform, or machine file** → `meson-cross-compilation`
- **Wraps, vendored dependencies, or fallback resolution** → `meson-subprojects-management`
- **CI pipelines, caches, and matrix builds** → `meson-ci-cd-integration`
- **Hard-to-diagnose setup/compile/test failures** → `meson-debugging-troubleshooting`
- **Minimum version requirements and deprecations** → `meson-version-compatibility`
- **Project architecture, dependency design, feature options, generated sources** → `meson-advanced-project-design`
- **Complete examples and project layouts** → `meson-patterns-cookbook`
- **Installation layout, pkg-config exports, releases, and packaging** → `meson-package-export-distribution`
- **Migrating from CMake, Autotools, or plain Makefiles** → `meson-setup-configuration` for language standard selection, `meson-version-compatibility` for baseline guidance

## Core rules

- Keep `meson.build` declarative and small.
- Prefer target-local configuration over global argument sprawl.
- Use feature options for optional capabilities.
- Export reusable interfaces with `declare_dependency()` instead of duplicating flags.
- Treat build directories as disposable; do not store source of truth there.
- Prefer built-in standards and options over manual compiler flags.
- Set a modern project baseline explicitly unless the example is intentionally specialized. Use the repository's actual license file name in `license_files` (`LICENSE`, `LICENSE.md`, `LICENSE.rst`, `COPYING`, or a path under a subdirectory) instead of forcing one filename everywhere:

```meson
project(
  'example',
  'c',
  'cpp',
  version: '0.1.0',
  meson_version: '>=1.7.0',
  license: 'MIT',
  license_files: ['LICENSE.md'],
  default_options: [
    'c_std=c17',
    'cpp_std=c++20',
    'warning_level=3',
  ],
)
```

## What good Meson looks like

- A small top-level `meson.build` that wires together subdirectories.
- Public headers installed separately from private implementation details.
- Dependencies represented as dependency objects, not copied compiler flags.
- Optional features controlled by `feature` options, not hardcoded booleans.
- Generated code created in the build tree, not committed to the source tree.
- Versioned install metadata and pkg-config exports for downstream users.

## Style guide

- Use explicit names for targets and dependency objects.
- Prefer `foo_dep` for dependency objects, `foo_lib` for libraries, and `foo_test` for test helpers.
- Keep `subdir()` boundaries shallow and predictable.
- Put reusable helper functions in `meson.build` only when they truly reduce duplication.
- Keep project metadata in `project()`, option defaults in `default_options`, and long-lived toggles in `meson_options.txt`.

## When a question is broader than one skill

Use this skill for the general framing, then route to the specialized skill that owns the concrete answer. If the user asks for a complete example, prefer `meson-patterns-cookbook`. If they ask how to ship or package the result, prefer `meson-package-export-distribution`.

Runnable examples live under each skill's `examples/` directory.
