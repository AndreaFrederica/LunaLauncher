# Neo UI

This is an optional Quasar/Vue frontend and a separate Tauri application. It is
independent of `tools/curseforge-webview`, which remains the Tauri helper used by
the CurseForge downloader.

The frontend talks to the installed `lunalauncher-cli --mcp` sidecar through a
Rust bridge. The default Meson build does not configure or compile this
directory. Enable it explicitly with `-Dneo_ui=enabled`.

For development, set `LUNA_LAUNCHER_CLI` to the installed CLI executable and
`LUNA_LAUNCHER_DATA` to a disposable launcher data directory before starting
Tauri. Installed builds discover `lunalauncher-cli` beside the Neo UI binary.
