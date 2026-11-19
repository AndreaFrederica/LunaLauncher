# Flatpak support assessment

The initial 2026-11-19 assessment below is historical. Implementation now lives
in the dedicated Pixi Flatpak packaging pipeline; see [build, installation and
test instructions](flatpak.md). It ships binary bundles on the Freedesktop 25.08
runtime. An offline Flathub source-build manifest and Flathub review remain
separate distribution work.

## Existing foundations

- The application ID is already `org.lunalauncher.LunaLauncher`, with desktop,
  icon, and AppStream metadata under `program_info/`.
- `DesktopServices::isFlatpak()` detects `/.flatpak-info`.
- `ShortcutUtils.cpp` constructs `flatpak run <application-id>` shortcuts.
- `JavaUtils.cpp` scans `/app/jdk`; launcher-managed Java downloads can live in
  the application's writable data directory.
- Linux self-updates are currently enabled for supported AppImage/portable
  installations. A Flatpak installation should receive updates from its Flatpak
  repository, with an appropriate explanation in the update UI.
- The project already builds Linux through pinned Pixi tasks and Meson.

Prism Launcher provides a working packaging reference on Flathub. Its manifest
currently uses `org.kde.Platform` and `org.kde.Sdk` version `6.10`, the OpenJDK 17
SDK extension for compilation, and launcher-managed Java for game instances:

<https://github.com/flathub/org.prismlauncher.PrismLauncher/blob/master/org.prismlauncher.PrismLauncher.yml>

That manifest builds upstream Prism through CMake. Luna's additional components
and active Meson/Pixi build require a dedicated manifest and build integration.

## Work needed for the full Luna package

### Build inside the selected SDK

Add a Flatpak-specific Pixi packaging task/profile and manifest. The application
must install into `/app` and use the selected SDK's libraries and compiler.
The existing wrapper assumes an aqt-downloaded Qt tree and a normal deployment
prefix, so those paths need explicit SDK/prefix configuration.

Keep sources and dependencies available offline during the sandbox build:
Meson wraps/subprojects, Cargo dependencies, pnpm packages, and NuGet packages.
The current build downloads/restores several of these on demand.

### Qt and WebKitGTK

KDE 6.10 is a practical starting point for the Qt launcher. The complete
application also uses Tauri and WebKitGTK 4.1 for Neo UI and the CurseForge
browser helper. Verify or package their full GTK/WebKit dependency chain in
the chosen runtime; Qt's browser components do not replace Tauri's dependencies.

An internal Classic prototype can validate Qt, Java, and game launching first.
Disabling Neo UI or the CF browser helper would be a reduced feature prototype,
not proof that the complete application is supported.

### Optional native components

`PCL.Download` builds a .NET 10 NativeAOT shared library. The packaged application
does not need a separately installed .NET runtime, but the build needs the SDK,
native toolchain, and pinned offline NuGet inputs. Rust authentication/networking
services and their process/data paths also need sandbox validation.

### Sandbox integration

Provide network access, graphical display access, audio, and GPU acceleration.
Validate game controllers and GameMode/MangoHud separately. Prism's finish-args
are a useful reference for Minecraft-specific requirements.

Use the application's XDG data directory for writable state. Test Qt and Tauri
file dialogs through desktop portals, including persistent access to user-chosen
instance stores, imports, exports, and Java locations. Host `/usr/lib/jvm` paths
cannot be assumed to name usable Java installations inside the sandbox; prefer
launcher-managed Java or an explicitly packaged runtime.

Keep the launcher, its CLI backend, and services inside the same sandbox. Normal
Minecraft launching does not require executing the host launcher or host Java.

## Suggested delivery sequence

1. Add a dedicated manifest/task and obtain a reproducible Linux x86_64 prototype
   that uses `/app`, the SDK, and offline dependency inputs.
2. Validate Classic and launcher-managed Java, then include Neo UI and the CF
   browser helper with all required libraries.
3. Test on X11 and Wayland: account login, game launch with the relevant Java
   versions, GPU/audio, mod downloads, external stores, imports/exports, and
   persistence after restart. Test the Flatpak-managed update path.
4. Publish a versioned `.flatpak` bundle and a signed Flatpak repository/ref for
   updates. A Flathub submission is a subsequent distribution step with its own
   application-ID verification and review.

No Flatpak binaries were built during the original assessment. The implementation
now provides a full bundle build and installed-sandbox smoke tests through Pixi,
plus CI packaging. Existing Windows release assets remain separate.
