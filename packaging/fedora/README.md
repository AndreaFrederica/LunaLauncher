# Fedora and COPR packaging

The RPM builds Luna Launcher 11.1.1 with Meson and installs under `/usr` using
`-Dinstall_layout=system`. The native Qt launcher retains Luna's launcher API,
CLI/MCP, JavaScript platform, PCL import, mirror selection, themes and native
multiplayer controls.

The offline RPM build disables the optional PCL.Download .NET backend,
YukariConnect service, CurseForge WebView helper, Tauri/Quasar Neo UI and Rust
yggdrasil service. These auxiliary programs are not bundled in this RPM. Native
integration controls do not imply that an external service is installed. The
Meson options for these features remain available for other distributions and
local builds; disabling them here does not remove their source implementations.

The RPM uses the system Qt WebP plugin from `qt6-qtimageformats`, and statically
links the bundled QuickJS sources. It does not ship a separate `libqjs.so`.
JARs and `qtlogging.ini` install under `/usr/share/LunaLauncher/`, with additional
resources in `/usr/share/LunaLauncher/resources/`. The executable, desktop file,
AppStream metadata, MIME definition, icons and manual use standard system paths.

## Build a source RPM

Install `rpm-build`, `git`, `python3`, `tar` and `gzip`. Initialize all pinned
submodules first, then run:

```fish
git submodule update --init --recursive
./packaging/fedora/make-srpm.sh
```

The helper writes the source archive and SRPM under `dist/fedora/rpmbuild/`.
Pass an output directory as its first argument to use a different location.
The source archive includes recursive source submodules, so the RPM build itself
does not need to download source dependencies. Submodules explicitly marked
`update=none` in their parent's snapshot `.gitmodules` are excluded, matching
Git's initialization policy; QuickJS's optional `test262` test data is one such
submodule. All other pinned submodules are required. `--wrap-mode=nodownload` enforces this
for Meson wraps. Distro build dependencies are declared in `lunalauncher.spec`;
COPR installs those separately from Fedora repositories.

The default `git` mode reads the spec, Meson version, source files and gitlink
pins from the same committed `HEAD`. Uncommitted edits are excluded. A missing
pinned source submodule object is an error, rather than a partially populated
archive. Submodule exclusion policy is read from each archived revision's
`.gitmodules`, not the current checkout's Git configuration.

To verify local packaging changes before committing, explicitly use a worktree
snapshot:

```fish
env LUNALAUNCHER_SRPM_SOURCE_MODE=worktree ./packaging/fedora/make-srpm.sh
```

That mode includes tracked changes, nonignored new source files and recursively
checked out submodule changes. It excludes Git metadata, local automation,
credential directories and generated build/install output. Review new files
before creating a distributable worktree snapshot. It also rejects unresolved
merge entries and requires every nonexcluded pinned submodule object to be
available. In this mode, exclusion policy follows each current `.gitmodules`
file, consistently with the rest of the worktree snapshot.
The spec version must match `project(version:)` in `meson.build` in either mode.

The spec uses Fedora `%meson`, `%meson_build` and `%meson_install` macros. Tests
are optional and can be enabled when building the binary RPM:

```fish
rpmbuild --rebuild --with tests dist/fedora/rpmbuild/SRPMS/lunalauncher-11.1.1-1.src.rpm
```

Enabled tests use `QT_QPA_PLATFORM=offscreen` for headless build workers.
Fedora's macros supply the system prefix and standard compiler/linker flags.
`build_platform` becomes `fedoraNN`, and the independent GitHub binary updater
is disabled for the RPM. Updates should come through the package manager.

## COPR SCM configuration

Use the repository's `.copr/Makefile` and **make srpm** build method. Selecting
only the spec file or a GitHub source archive does not assemble the required
submodule sources.

Recommended package settings:

- Source type: `SCM`
- SCM type: `git`
- Clone URL: `https://github.com/W-874/LunaLauncher.git`
- Committish: `meson` (or an explicitly selected release tag/commit)
- Spec file: `packaging/fedora/lunalauncher.spec`
- SRPM build method: `make srpm`

The Makefile synchronizes submodule URLs from the repository's `.gitmodules`,
initializes recursive submodules and copies the generated SRPM to COPR's
`outdir`. It makes no hard-coded QuickJS remote override. Submodule hosts must
be reachable during the SRPM preparation stage; binary RPM compilation is
offline with respect to source downloads.

Inspect the COPR package's actual committish after changing development branches:
Git pushes do not automatically change an SCM package configured to build a
different branch. An existing configuration using `update-stream-11.0.2` must
be changed to `meson` before it will build this branch. These repository files
do not change the remote COPR configuration.

The spec is intended for Fedora 42 through Rawhide. Successful local source
packaging or a host Meson build is not a successful build on every COPR chroot;
check the real Fedora build logs after submitting a build.
