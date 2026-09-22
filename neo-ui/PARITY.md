# Neo UI coverage

This is an implementation checklist, not a claim of Qt GUI parity. Both
`instance.icon` and the UI changes are additive; the Qt pages remain intact.

Implemented in the Neo UI:

- Shared launcher shell, responsive navigation, theme tokens and instance cards.
- Backend-rendered PNG instance icons, built-in fallback and import by local path.
- Vanilla/loader version selection and local/URL pack import.
- Pack search, version selection and installation for Modrinth, CurseForge,
  ATLauncher, FTB, Technic and legacy FTB.
- Resource search, Minecraft/loader filtering, version selection and dependency
  installation for Modrinth and CurseForge.
- Reusable settings panel exposing all returned launcher/instance settings,
  typed controls, individual saves/resets, and labels for mapped Qt controls.
- Java detection, selection, diagnosis and provider package download/install.
- Instance tabs, separate console subscriptions/history, and a launch queue.
  Launch preparation remains serialized by the shared API session. Existing
  games continue running; during another launch, cached instance tabs remain
  accessible. This is not parallel preparation/download support.
- Launcher appearance, language, proxy, updater, news, cache, and Prism import
  controls are available in the launcher settings workspace.
- Account profile, skin library, skin upload/reset, cape selection, and account
  ordering are available in the account workspace.
- Component ordering, enable/disable, customization, custom component install,
  instance export, and resource update plans are available in instance tools.
- Server start, console commands, properties, EULA, player lists, and YAML
  editing are available for server instances.
- XMCL UI shell port now supplies the sidebar, instance icons and status
  overlays, sticky home header, add-instance dialog, home card workspace,
  settings navigation, account workspace, and market-style resource workspace.
  The source snapshot and MIT notice are kept under `src/xmcl-port`.

Still needed for equivalent workflows:

- Native file/directory pickers, drag-and-drop import and built-in icon picker.
- Full settings enum choices, override dependencies, unsaved draft preservation,
  and localized labels/help remain incremental improvements.
- FTB local migration, ATLauncher share codes, legacy FTB private codes, and
  provider-specific sorting, categories and JS provider selection.
- Resource project details/changelogs, optional dependency selection, and
  provider-specific bulk operations remain incremental improvements.
- Native path resolution for drag-and-drop and richer project detail dialogs
  remain for the next XMCL component port; browser file selection currently
  forwards the Tauri file path when available.
- Independent concurrently executing API sessions or a backend scheduler for
  true parallel launch preparation. A separate frontend queue alone cannot
  provide this safely against a single locked launcher data directory.
- End-to-end desktop acceptance with multiple running game instances.
