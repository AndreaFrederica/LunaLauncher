# Linux Flatpak

Luna's Flatpak contains Neo UI, Classic UI, the CurseForge WebKit browser,
native download/authentication services and Java 21. The application ID is
`org.lunalauncher.LunaLauncher`; the supported architecture is x86_64.

## Install and run

Download the `.flatpak` and its `.sha256` file from the same GitHub build/release.
From that download directory:

```sh
sha256sum -c LunaLauncher-Linux-x64-*-webui.flatpak.sha256
flatpak remote-add --user --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
flatpak install --user ./LunaLauncher-Linux-x64-<version>-webui.flatpak
flatpak run org.lunalauncher.LunaLauncher
flatpak run org.lunalauncher.LunaLauncher --luna-entry classic
```

The desktop entry starts Neo UI and has a **Classic UI** action. CLI operations
and `--version` select the native backend automatically. For example:

```sh
flatpak run org.lunalauncher.LunaLauncher --cli --print-startup-context
```

Data is stored at
`~/.var/app/org.lunalauncher.LunaLauncher/data/LunaLauncher`. Java 21 is available
at `/app/jdk/21/bin/java` inside the sandbox. Enable managed Java downloads for
Minecraft versions needing a different Java version; host `/usr/lib/jvm` paths
do not refer to the host's Java inside Flatpak.

Network, graphics, audio, controllers and Downloads access are granted. The
package uses desktop portals for file selection and does not grant home/host
filesystem access or permission to execute host commands. Install your desktop's
`xdg-desktop-portal` backend. For a persistent external instance store that your
desktop's folder portal cannot expose, grant just that directory:

```sh
flatpak override --user --filesystem=/absolute/path/to/Minecraft org.lunalauncher.LunaLauncher
```

## Updates

Standalone bundles are updated by installing the newer `.flatpak` file; launcher
data is preserved. The in-app updater does not replace files in `/app`. When a
signed Luna Flatpak repository is configured, the software center or
`flatpak update org.lunalauncher.LunaLauncher` manages updates. Flathub supplies
the platform runtime, not the Luna application; this does not claim a Flathub
listing.

## Build and test

Use a clean Linux checkout, including recursive Git submodules, with Pixi. The
Ubuntu 22.04 desktop build dependencies used in CI are:

```sh
sudo apt-get install clang zlib1g-dev libgtk-3-dev libwebkit2gtk-4.1-dev \
  libayatana-appindicator3-dev librsvg2-dev desktop-file-utils libxkbcommon-x11-0 \
  libxcb-cursor0 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-render-util0 \
  libopengl0 xvfb flatpak dbus-x11 patchelf
pixi install --frozen
pixi run flatpak-setup
pixi run package-flatpak-webui
GDK_BACKEND=x11 dbus-run-session -- xvfb-run -a pixi run test-flatpak /absolute/path/to/package.flatpak
```

`package-flatpak` produces the Classic variant. The Web UI variant includes
both interfaces. To package a completed matching Pixi build without recompiling:

```sh
LUNA_NEO_UI=enabled pixi run python scripts/pixi_flatpak.py package
```

Artifacts are written to `release-assets/`: the bundle, checksum and
`linux-x64-flatpak.json`. The default OSTree repository is
`.cache/flatpak-repo/`, outside release assets. The packager repeats Meson's
install into a fresh DESTDIR, so unmanaged files from `install-*` are excluded.
It reuses the pinned Qt/GTK deployment and creates Java 21 with Pixi's `jlink`.
Runtime metadata and permissions live in `scripts/flatpak/package.json`.

This is binary packaging against `org.freedesktop.Platform//25.08`, with all
application libraries deployed by the pinned packaging tools. It is not an
offline Flathub source-build manifest. The native build and Java generation run
through Pixi; Flatpak stages and exports their output without a second compiler.

The smoke task installs/reinstalls the supplied bundle in the user Flatpak
installation and uses disposable profile directories. It verifies every ELF
dependency in the actual runtime, bundled Java detection, a real managed Java
download/extraction, persistence, denied host
filesystem access, managed update status, ZIP/MR/CF exports and the Neo
WebKit/native IPC bridge. It also exercises the real CF executable's URL
validation. Interactive Microsoft login, public CF downloads and game/GPU/audio
behavior require a desktop acceptance check.

CI builds and tests Flatpak in `.github/workflows/flatpak.yml` and in Linux
release jobs. The separate workflow can be dispatched for a Flatpak artifact
without rebuilding Windows or replacing its stable release.

## Hosting a signed repository

Use a persistent signing key already imported into the build user's GPG keyring:

```sh
LUNA_NEO_UI=enabled LUNA_FLATPAK_GPG_KEY=<key-id> \
  LUNA_FLATPAK_REPO_URL=https://downloads.example.org/luna/flatpak \
  pixi run python scripts/pixi_flatpak.py package --repo /srv/luna-flatpak
flatpak build-update-repo --generate-static-deltas --gpg-sign=<key-id> /srv/luna-flatpak
```

Serve that OSTree directory at the configured HTTPS URL and publish the signing
public key through your trusted distribution channel. Bundles built with the
origin URL carry it for subsequent Flatpak updates. Keep the signing key stable
across releases. The default GitHub bundle pipeline does not create a production
signing identity or publish an unsigned network repository.
