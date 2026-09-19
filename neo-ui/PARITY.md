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

Still needed for equivalent workflows:

- Native file/directory pickers, drag-and-drop import and built-in icon picker.
- Full settings enum choices, override dependencies, unsaved draft preservation,
  localized labels/help, and dedicated global appearance/network/update pages.
- FTB local migration, ATLauncher share codes, legacy FTB private codes, and
  provider-specific sorting, categories and JS provider selection.
- Resource project details/changelogs, optional dependency selection, bulk
  operations, and update workflows matching the original mod pages.
- Full instance editor tabs for worlds/screenshots/exports and remaining tools.
- Independent concurrently executing API sessions or a backend scheduler for
  true parallel launch preparation. A separate frontend queue alone cannot
  provide this safely against a single locked launcher data directory.
- End-to-end desktop acceptance with multiple running game instances.
