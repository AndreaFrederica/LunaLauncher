# Plugin System

Build a host executable and a shared module plugin.

```bash
meson setup build
meson compile -C build
meson test -C build
```

On Unix-like systems the host can load the plugin with `dlopen()`. On Windows it uses `LoadLibrary()`.
