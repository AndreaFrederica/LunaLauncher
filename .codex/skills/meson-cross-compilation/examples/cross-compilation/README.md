# Cross Compilation

Shows a native tool used by the build and a separate cross file for the host target.

```bash
meson setup build --native-file native.ini --cross-file cross.ini
meson compile -C build
```

The native file is for build-machine tools. The cross file is for the target.
