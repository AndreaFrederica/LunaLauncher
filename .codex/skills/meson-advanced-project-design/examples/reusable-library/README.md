# Reusable Library

Build a library, export a dependency object, install headers, and generate pkg-config metadata.

```bash
meson setup build
meson compile -C build
meson test -C build
meson install -C build --destdir "$PWD/stage"
```
