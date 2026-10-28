# Meson Advanced Project Design — Troubleshooting

## Generated file appears in the source tree

**Symptom:** A generated header or source file is committed to `source/` or shows up in `git status` after a build.

**Cause:** `configure_file()`, `custom_target()`, or a script writes output to a source-relative path.

**Fix:** Always direct output to the build tree. Use `@OUTPUT@` (which resolves inside the build tree) and never construct source-tree paths manually.

```meson
# Wrong — writes into the source tree
configure_file(input: 'config.h.in', output: '../src/config.h', ...)

# Correct — output stays in the build tree
configure_file(input: 'config.h.in', output: 'config.h', configuration: conf)
```

Add generated files to `.gitignore` if they were previously committed by mistake.

---

## Compiler flags leak from one target to another

**Symptom:** A flag set for an internal target (e.g. `-DINTERNAL_BUILD`) appears in the compile commands of an unrelated target.

**Cause:** `add_project_arguments()` is called from a subproject, or per-target arguments are placed in the wrong scope.

**Fix:** Use `add_project_arguments()` only in the top-level `meson.build`. Use per-target `c_args` / `cpp_args` for flags that should not propagate:

```meson
# Wrong — called from inside a subproject, bleeds into parent
add_project_arguments('-DINTERNAL_BUILD', language: 'cpp')

# Correct — scoped to one target
mylib = library('mylib', 'src/mylib.cpp',
  cpp_args: ['-DINTERNAL_BUILD'],
)
```

---

## Feature option does not gate the dependency correctly

**Symptom:** A dependency is always required even when the feature option is set to `disabled`.

**Cause:** The option is fetched but not passed as `required:` to `dependency()`.

**Fix:** Pass the feature option object directly to `required:`:

```meson
# Wrong — option value ignored
wayland_dep = dependency('wayland-client', required: true)

# Correct — required follows the option
wayland_opt = get_option('wayland')
wayland_dep = dependency('wayland-client', required: wayland_opt)
```

When `wayland` is `disabled`, the dependency call returns a not-found object instead of aborting.

---

## `declare_dependency()` not visible to the parent project

**Symptom:** `subproject.get_variable('mylib_dep')` raises "variable not found" or returns the wrong type.

**Cause:** The subproject's `meson.build` builds the library but never assigns a `declare_dependency()` result to a named variable.

**Fix:** Assign the result to a stable variable name at the top level of the subproject's `meson.build`:

```meson
# Inside the subproject
mylib_dep = declare_dependency(
  include_directories: inc,
  link_with: mylib,
)
# mylib_dep is now accessible to the parent via get_variable('mylib_dep')
```

---

## Library type causes unexpected link errors in consumers

**Symptom:** A consumer that links against a `static_library()` fails with undefined symbols at link time, or a `shared_library()` consumer fails at runtime with "symbol not found".

**Cause:** Library type chosen without considering how the consumer links.

**Fix:** Use `library()` to let the user choose via `default_library`, or pick explicitly:

- Static → consumer links everything in; no runtime dependency, but every consumer gets its own copy.
- Shared → symbols must be exported; the `.so`/`.dll` must be on the runtime path.
- Module (`shared_module()`) → only loaded via `dlopen`/`LoadLibrary`; never link directly.

```meson
# Let the user decide at configure time
lib = library('mylib', 'src/mylib.cpp', install: true)
```

---

## `custom_target()` output not picked up by a dependent target

**Symptom:** The generated header or source is not found when the depending target compiles.

**Cause:** The `custom_target()` result is not passed to `sources:` or used as an include directory.

**Fix:** Pass the target directly in `sources:`:

```meson
gen_header = custom_target('gen-header', output: 'gen.hpp', ...)

myapp = executable('myapp', 'src/main.cpp',
  sources: [gen_header],          # pulls in the generated file
  include_directories: [inc, gen_header[0].parent()],
)
```

Or use `include_directories(meson.current_build_dir())` when the generator writes to the current build subdirectory.

---

## Common mistakes

- Calling `subproject()` from inside a conditional deep in build logic — call it near the top level so dependency resolution is visible.
- Using `run_command()` for work Meson can model with `custom_target()` or `generator()` — `run_command()` runs at configure time, not build time.
- Storing both system and vendored copies of the same dependency in the same build without a clear policy — pick one resolution path per dependency.
- Letting `add_project_arguments()` calls from a subproject reach the parent's targets.

## What belongs elsewhere

- Wrap files and fallback resolution → `meson-subprojects-management`
- Install layout and pkg-config → `meson-package-export-distribution`
- CI job shapes → `meson-ci-cd-integration`
- Version requirements → `meson-version-compatibility`
