# Neo UI

This is an optional Quasar/Vue frontend and a separate Tauri application. It is
independent of `tools/curseforge-webview`, which remains the Tauri helper used by
the CurseForge downloader.

The frontend talks to the installed `lunalauncher-cli --mcp` sidecar through a
Rust bridge. The default Meson build does not configure or compile this
directory. Enable it explicitly with `-Dneo_ui=enabled`.
