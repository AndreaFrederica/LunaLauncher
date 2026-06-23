# Meson Compile Targets — Reference

## Target declarations

Use target declarations to name build outputs clearly and make them easy to select later.

```meson
myapp = executable(
  'myapp',
  'src/main.cpp',
  'src/util.cpp',
  include_directories: include_directories('include'),
  install: true,
)
```

## Build the right target

```bash
meson compile -C builddir myapp
meson compile -C builddir path/to/myapp
```

## Useful inspection commands

```bash
meson introspect builddir --targets
meson compile -C builddir -v
```

## Notes

- target names come from `executable()`, `library()`, `shared_module()`, and other target declarations
- `meson compile` forwards to the configured backend
- flags configured during setup are part of the build tree, not the source tree


