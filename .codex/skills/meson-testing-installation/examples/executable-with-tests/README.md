# Executable With Tests

A small application and a test binary share the same core code.

```bash
meson setup build
meson compile -C build
meson test -C build --print-errorlogs
```
