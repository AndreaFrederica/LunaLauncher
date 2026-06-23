# Basic Build

Shows named target selection, parallel jobs, verbose output, and `meson introspect --targets`.

```bash
# Full build
meson setup build
meson compile -C build

# Build only the library
meson compile -C build demo

# Build only the executable
meson compile -C build demo-app

# Build with all CPU cores
meson compile -C build -j 0

# Show every compiler and linker command
meson compile -C build -v

# List all targets
meson introspect build/ --targets

# Run tests
meson test -C build --print-errorlogs
```
