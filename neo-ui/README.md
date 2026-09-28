# Neo UI

完整中文开发文档：[Web UI 开发与构建](../docs/WEBUI-DEVELOPMENT.md)。

```sh
pixi run install-webui  # Configure, build, install and deploy everything
pixi run run-webui      # Run the installed application
pixi run webui-assets   # Export frontend only to neo-ui/dist/spa
```

The default complete installation is `install-webui-release`; optional `debug`
selects `install-webui-debug`. Set `LUNA_BUILD_DIR` / `LUNA_INSTALL_DIR` to reuse
an existing build/installation. `webui-dev` provides browser preview and
`webui-check` runs type checking. The browser preview has no native API bridge.

This is an optional Quasar/Vue frontend and a separate Tauri application. It is
independent of `tools/curseforge-webview`, which remains the Tauri helper used by
the CurseForge downloader.

The frontend talks to the installed `lunalauncher-cli --mcp` sidecar through a
Rust bridge. The default Meson build does not configure or compile this
directory. Enable it explicitly with `-Dneo_ui=enabled`.

For development, set `LUNA_LAUNCHER_CLI` to the installed CLI executable and
`LUNA_LAUNCHER_DATA` to a disposable launcher data directory before starting
Tauri. Installed builds discover `lunalauncher-cli.exe` on Windows and
`lunalauncher` on other platforms beside the Neo UI binary.

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

Instance management displays the actual instance/game directories and the
ordered version components separately from installed mod/resource files.
Resource read failures are shown separately from an empty resource directory.

Launching opens instance management and subscribes to the console before
issuing the launch request. The Tauri bridge forwards `launcher/stream` batches
for live output and running-state changes. The console retains up to 3,000
entries, supports auto-scroll and clearing the display, and keeps startup
errors visible. Historical log files can also be listed and read (up to 1 MiB).
A successful launch response means that the request returned; the console is
the source for subsequent client output and exit state.

## External Web UIs

Copy any compiled static frontend into one of these locations:

```text
<directory containing luna-neo-ui.exe>/webui/<folder>/index.html
<launcher data directory>/webui/<folder>/index.html
```

Keep the build's assets next to its `index.html` (for example `assets/`). The
data directory is read from `runtime.info` on the actual launcher sidecar, so
portable mode and `LUNA_LAUNCHER_DATA` work without a second path configuration.
Use the title bar's **Web UI** button, **Settings → Neo UI appearance**, or
**Ctrl/Command + Shift + U** to open the independent native management window.
Refresh the list after adding/replacing files. No launcher rebuild is needed.
Folders from both locations are listed separately, even with identical names.

An optional `webui.json` in the same directory may contain:

```json
{
  "name": "My Web UI",
  "version": "1.0.0",
  "description": "My independent launcher frontend",
  "apiVersion": 1,
  "nativeFrame": true
}
```

`apiVersion` is the bridge version (currently 1). `nativeFrame` defaults to true;
set it to false only if your UI supplies working window controls. A missing
manifest uses the folder name. Invalid manifests/missing entry pages are shown
with an error. The selected ID (`install/<folder>`, `data/<folder>`, or `builtin`)
is saved in `<data directory>/webui/selection.json`. A removed or invalid selection
falls back to the embedded UI. Set `LUNA_WEB_UI=builtin` before starting the
launcher to recover from a UI whose JavaScript fails at runtime.

Files are served by Tauri's local `webui` protocol, with no HTTP server or port.
Absolute `/assets/...` and relative asset paths both work; use hash routing for
SPAs. Missing asset paths return 404, and paths/symlinks outside the selected
directory are rejected. Only install trusted UI code: it has the same launcher
API access as the embedded UI. Other sites do not receive these permissions.

The host injects `window.luna`, so plain JavaScript and frameworks can use the
same backend without bundling a Tauri SDK:

```js
const result = await luna.execute('instance.list'); // { ok, data, error }
const catalog = await luna.catalog();
const unlisten = await luna.onEvent(event => { /* progress / login prompts */ });
const unstream = await luna.onStream(batch => { /* console output */ });
await luna.respond({ interactionId, value }); // or { interactionId, cancel: true }
await luna.openPath(instanceDirectory);
await luna.openWebUiManager();
// await luna.switchWebUi('builtin');
unlisten(); unstream();
```

`luna.onExit(handler)` observes backend exit; `luna.webUis()` lists available UIs.
Ordinary `execute` calls are serialized; task controls remain available during
downloads. The existing `@tauri-apps/api` bridge is also supported. UI switching
reloads the document and discards unsaved local input, but keeps the native host,
sidecar and running game processes alive. Pending backend requests (including
downloads and interactive login) prevent switching until completed/cancelled.
Each UI must rebuild its view state and subscriptions after loading. Use a
distinct storage key prefix for each external UI; they share a local origin.

`examples/webui/instance-list` is a dependency-free working example. Copy that
whole folder into a `webui` directory to try discovery and switching. It is an
API integration example, not a replacement for the embedded management screens.
External bundles remain separate files and do not increase the embedded UI size.
The entire Neo UI host still stays optional via `-Dneo_ui=disabled`.

To export this repository's complete Neo UI as an external bundle, run
`pixi run webui-assets` and copy the **contents** of `neo-ui/dist/spa`
into `webui/neo-ui/`. Select that folder in Web UI management. Subsequent frontend
changes only need another frontend build and file copy; Rust/C++ recompilation is
unnecessary. Frontends from other projects need an adapter for the Luna API.
Reinstalling with Meson or the Pixi install task preserves external UI files.

### Verification

`pixi run test-webui` (after `install-webui`), or
`pixi run python tests/webui_smoke.py --exe install-neo-ui-api/luna-neo-ui.exe`,
runs the installed Tauri application with disposable launcher and WebView data.
It checks real asset loading, the injected API, login events and cancellation,
switch protection during a request, native manager creation, navigation between
two bundles while retaining backend subscriptions, persisted selection, and
recovery to the embedded UI, plus closing the launcher with its manager open.
It requires a desktop/WebView runtime.
