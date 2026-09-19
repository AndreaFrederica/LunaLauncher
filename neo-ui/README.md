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

Without `LUNA_LAUNCHER_DATA`, Neo UI uses the CLI's normal data-directory
selection, including portable installations and the launcher's environment
override. It does not create a separate empty `data` profile. To open a profile
created by an older Neo UI build, explicitly point `LUNA_LAUNCHER_DATA` to that
installation's `data` directory; no profiles are moved or deleted automatically.

The top navigation exposes the instance library, creation/import, accounts,
and settings at all window sizes. Instance management includes resource
installation, enable/disable, and removal. Minecraft and loader version lists
are fetched from the backend using the load-version buttons. The interface
revision in the header can be used to identify an outdated installed binary.
