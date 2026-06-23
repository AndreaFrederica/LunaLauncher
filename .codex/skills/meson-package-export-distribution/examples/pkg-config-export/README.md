# Pkg-config Export

Shows library installation, header installation, and `pkg.generate()` for downstream consumers.

```bash
meson setup build
meson compile -C build
meson install -C build --destdir "$PWD/stage"
meson test -C build
```
