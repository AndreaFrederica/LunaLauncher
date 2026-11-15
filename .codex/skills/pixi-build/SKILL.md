---
name: pixi-build
description: Build, test, install, deploy, or clean the LunaLauncher repository through its pinned Pixi tasks. Use whenever work in this repository needs a compiled binary, a test run, an install tree, a Web UI build, or a clean/reconfigure operation.
---

# Pixi Build

Use Pixi as the repository build entrypoint. Run commands from the repository root and keep the environment defined by `pixi.toml` and `pixi.lock`.

## Required commands

- Configure: `pixi run configure release` or `pixi run configure debug`
- Build: `pixi run build release` or `pixi run build debug`
- Install and deploy: `pixi run install release`
- Run the installed launcher check: `pixi run test-install release`
- Run the build-tree check: `pixi run test debug`
- Clean one profile: `pixi run clean release`
- Clean all generated build/install trees: `pixi run clean-all`

The repository also supports `linux-x64-gcc-release` for the Linux cross profile. Pass a profile explicitly when the requested build is not the default.

## Neo UI / Web UI

Use the dedicated tasks so `LUNA_NEO_UI=enabled` and the correct native/frontend deployment steps are applied:

- Complete build and install: `pixi run install-webui release`
- Compile without installing: `pixi run build-webui release`
- Frontend checks: `pixi run webui-check` and `pixi run webui-assets`
- Installed Web UI smoke test: `pixi run test-webui release`
- Run the installed UI: `pixi run run-webui release`

Use `pixi run --dry-run <task> [profile]` to inspect the dependency chain before a long operation.

## Toolchain rules

Do not perform the actual project build with a globally installed `meson`, `ninja`, `cmake`, MSBuild, MSYS2, MinGW, or an ad-hoc compiler command. Do not manually reuse a different `build-meson-*` directory after changing profile, compiler, or Web UI mode; use the Pixi `clean` task and configure again.

For target inspection, read-only Meson introspection through Pixi is allowed, for example `pixi run meson introspect build-meson-release --targets`. If a target-specific build is genuinely needed and no repository task covers it, keep the command inside Pixi and preserve the wrapper's environment; otherwise use the standard `build` task.

If `pixi` or the required compiler environment is unavailable, report the blocker and stop. Do not silently fall back to a system toolchain.

## Reporting

State the exact Pixi task and profile used, the resulting build/install path, and the first actionable failure if the task does not complete. A successful configure or dry run does not count as a successful compile; report the compile/test result separately.
