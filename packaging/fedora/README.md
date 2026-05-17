# Fedora Packaging

This directory provides a Fedora 44 baseline for building Luna Launcher as a
COPR-friendly RPM without relying on the in-tree `CPack RPM` path.

For distributable Fedora packages, use `CMAKE_INSTALL_PREFIX=/usr`.
Do not use `/usr/local` for RPM/COPR builds: `/usr/local` is intended for
administrator-managed local software, while packaged files belong under `/usr`.

## What is included

- `lunalauncher.spec`: RPM spec intended for Fedora 44 first.
- `make-srpm.sh`: helper to create a source tarball from `HEAD` and build an
  SRPM locally.

## Why use this instead of `CPack`

`CPack` is still useful for local experiments, but COPR works better with a
normal RPM spec because:

- Fedora build dependencies are explicit.
- Scriptlets for desktop, icon, and MIME cache updates are under RPM control.
- Future Fedora-version-specific dependency changes can be made in one place.

## Fedora 44 baseline

The spec currently assumes Fedora 44 and sets:

- `CMAKE_INSTALL_PREFIX=/usr`
- `Launcher_BUILD_PLATFORM=fedora44`
- system Qt 6 packages
- standard RPM scriptlets for desktop, icon, and MIME caches

## Build an SRPM locally

Prerequisites:

- `rpm-build`
- `git`

Run:

```bash
./packaging/fedora/make-srpm.sh
```

If you want a local packaging-oriented CMake configure outside `rpmbuild`, the
repository now includes the preset:

```bash
cmake --preset linux_fedora44_packaging
```

Use that preset only for packaging workflows. For normal local development,
keep using the regular `linux` preset, which installs into the workspace-local
`install/` directory.

The script writes the source tarball and SRPM under `dist/fedora/rpmbuild/`.

## COPR usage

Two straightforward options:

1. Upload the generated SRPM to COPR.
2. Point COPR SCM builds at this repository and use `packaging/fedora/lunalauncher.spec`.

For the current baseline, target Fedora 44 only.

## Extending to other Fedora versions later

When you need Fedora 41-43 or Rawhide later, keep the spec as the single point
of change:

- add `%if 0%{?fedora}` conditionals around dependency differences
- adjust `Launcher_BUILD_PLATFORM` if you want release-specific branding in the
  About dialog
- keep the installed runtime resources under `share/LunaLauncher/resources` so
  Linux packaging stays consistent across targets
