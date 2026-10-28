---
name: meson-patterns-cookbook
description: Canonical Meson project layouts and complete patterns for libraries, executables, generated sources, plugin systems, fallback dependencies, and anti-patterns. Use when the user wants a full example or a reusable project template.
---

# Meson Patterns Cookbook

This skill gives complete patterns, not just fragments.

The runnable examples in this repository are colocated under the owning skills' `examples/` directories; this skill acts as the index for the reusable patterns.

## Golden project layout

```text
project/
├── meson.build
├── meson_options.txt
├── include/
├── src/
├── tests/
├── subprojects/
└── tools/
```

A small project may omit directories it does not need, but the layout should still be obvious to a newcomer.

## Minimal vs production-oriented examples

A minimal example should show one concept cleanly.

A production-oriented example should add:

- install layout
- version metadata
- pkg-config export
- tests
- a clean dependency object
- a README that explains the workflow

Do not mix both goals in the same snippet unless the user asked for a complete project.

## Reusable library pattern

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
lib = library('mylib', 'src/mylib.cpp', include_directories: inc, install: true)

mylib_dep = declare_dependency(
  include_directories: inc,
  link_with: lib,
)

pkg = import('pkgconfig')
pkg.generate(
  lib,
  description: 'My reusable library',
  filebase: 'mylib',
  subdirs: 'mylib',
)
```

Use this when the project is meant to be consumed by another build.

## Executable with tests

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

inc = include_directories('include')
core = static_library('core', 'src/core.cpp', include_directories: inc)
app = executable('example-app', 'src/main.cpp', link_with: core, install: true)

test_exe = executable('core-test', 'tests/test_core.cpp', link_with: core)
test('core', test_exe)
```

This keeps production code and test code separate while still reusing the same core implementation.

## Mixed C and C++ pattern

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

inc = include_directories('include')
clib = static_library('clib', 'src/clib.c', include_directories: inc)
cppapp = executable('cppapp', 'src/main.cpp', link_with: clib, include_directories: inc, install: true)
```

Use this pattern when a project exposes a C ABI but uses C++ internally or vice versa.

## Generated source pattern

```meson
python = find_program('python3', required: true)

gen = custom_target(
  'generated-config',
  input: 'config.in.h',
  output: 'config.h',
  command: [python, 'scripts/gen-config.py', '@INPUT@', '@OUTPUT@'],
)

sources = [gen, 'src/main.cpp']
```

Use `custom_target()` for one-off generation and `generator()` when the same tool is applied to many inputs. Keep both forms building into the build tree; do not commit generated files into `source/`.

## Plugin pattern

```meson
plugin = shared_module(
  'sample_plugin',
  'src/plugin.cpp',
  install: true,
  install_dir: get_option('libdir') / 'myapp' / 'plugins',
)
```

The host application should define a stable plugin interface and a documented install location. On Windows, export symbols explicitly with `__declspec(dllexport)` or a `.def` file.

## Wrap fallback pattern

```meson
zlib_dep = dependency('zlib', fallback: ['zlib', 'zlib_dep'])
```

Prefer a single dependency object over manual include and linker flags.

## Complete fallback project pattern

```meson
foo_dep = dependency('foo', required: false)
if not foo_dep.found()
  foo_proj = subproject('foo')
  foo_dep = foo_proj.get_variable('foo_dep')
endif
```

This is useful when the project supports both system and vendored dependencies. The important part is that the parent consumes one exported dependency object, not a pile of manual include and linker flags.

## Common mistakes

- glob source files as a replacement for explicit file lists
- hardcode compiler or linker flags in multiple places
- use `run_command()` for work that Meson can model directly
- mutate global arguments from subprojects
- duplicate install logic in many directories
- commit generated files to the source tree
- hide the public API behind a build helper that nobody can reuse

## Good pattern checklist

- the public API is installed
- the dependency object is exported
- optional features are feature options
- generated files live in the build tree
- subprojects only exist when they genuinely add value
- examples explain both the minimal case and the production case


