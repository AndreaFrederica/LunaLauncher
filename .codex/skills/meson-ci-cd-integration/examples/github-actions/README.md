# GitHub Actions

Shows a Meson CI job that caches compiler output and package downloads rather than the whole build tree.

```bash
meson setup builddir
meson compile -C builddir
meson test -C builddir --print-errorlogs
```
