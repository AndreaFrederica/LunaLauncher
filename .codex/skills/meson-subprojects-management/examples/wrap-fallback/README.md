# Wrap Fallback

Shows how to prefer a system dependency and fall back to an in-tree source subproject (no `.wrap` file — the subproject lives directly under `subprojects/`).

```bash
meson setup builddir
meson compile -C builddir
meson test -C builddir
```
