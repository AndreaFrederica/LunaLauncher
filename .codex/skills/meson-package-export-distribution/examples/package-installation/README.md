# Package Installation

Shows how install layout, staging, and pkg-config export fit together.

```bash
meson setup build --prefix=/usr
meson compile -C build
meson install -C build --destdir "$PWD/stage"
```
