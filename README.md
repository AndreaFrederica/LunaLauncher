<p align="center">
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="/program_info/org.interferencelauncher.InterferenceLauncher.logo-darkmode.svg">
  <source media="(prefers-color-scheme: light)" srcset="/program_info/org.interferencelauncher.InterferenceLauncher.logo.svg">
  <img alt="Luna Launcher" src="/program_info/org.interferencelauncher.InterferenceLauncher.logo.svg" width="40%">
</picture>
</p>

<p align="center">
  Luna Launcher is a custom launcher for Minecraft that allows you to manage multiple Minecraft installations with ease.<br />
  <br />
  This project is an independent <b>fork</b> of Prism Launcher and is <b>not</b> endorsed by or affiliated with the Prism Launcher project.
  <br />
  It improves accessibility by supporting community-maintained mirror APIs such as <b>BMCLAPI</b>.
</p>

---

## Installation

Installation instructions and downloads will be provided once stable releases are available.

At the moment, this project is primarily intended for developers and early testers.

---

## Building

If you want to build Luna Launcher yourself, please refer to the build instructions from the upstream project:

- <https://prismlauncher.org/wiki/development/>

The build process and requirements are largely identical, aside from project naming and backend configuration.

---

## Acknowledgements

Luna Launcher would not be possible without the work of earlier projects and their contributors.

- **Prism Launcher** — for maintaining a modern, open, and community-driven Minecraft launcher.
- **MultiMC** — the original project that laid the foundation for many third-party Minecraft launchers.

We sincerely thank all contributors to these projects for their long-standing efforts in the Minecraft community.

---

## Forking / Redistribution Policy

You are free to fork, redistribute, and provide custom builds as long as you follow the terms of the [license](LICENSE).

If you make code changes (rather than only packaging):

- Make it clear that your fork is **not** Prism Launcher and is **not** endorsed by or affiliated with the Prism Launcher project.
- Go through [CMakeLists.txt](CMakeLists.txt) and change any upstream API keys to your own, or set them to empty strings (`""`) to disable the related functionality.

If you are building this software for a distribution, please set `Launcher_BUILD_PLATFORM` to an appropriate identifier (for example: `archlinux`, `fedora`, `nixpkgs`).

---

## License

All launcher code is available under the **GPL-3.0-only** license.

The logo and related assets are licensed under **CC BY-SA 4.0**.
