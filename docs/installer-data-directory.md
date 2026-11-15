# Installer data directory

Standard MMC/Prism and non-portable custom installations let the user edit the launcher data folder, browse for an existing folder, or restore the system default. The review page shows the resulting path. Existing data is used in place; selecting another path does not migrate old data.

A custom folder must be absolute, writable, and outside the program installation tree, so ordinary uninstall cannot delete it. Portable and HMCL/PCL migration modes continue to use `UserData` beside the installed program.

The installer saves the custom choice as `data_dir` in its configuration and writes `.luna-data-dir` beside the launcher. This file contains a UTF-8 absolute path. The instance-store registry is written inside the selected data folder. Native UI, the Neo UI backend startup-context query, and the updater share the marker reader. Resolution order is explicit `--dir`, the launcher data environment variable, the per-install marker, then the existing system/portable rules. An invalid marker reports an error rather than silently opening another profile.

Switching to portable mode or restoring the default from the installer clears the override. An explicit data-mode switch from launcher settings also clears it after preparing the new location. Package creation excludes the marker so a developer's data path cannot be shipped.

Non-portable Windows installation registers its uninstaller, `curseforge://` and `lunalauncher://` handlers, `.mrpack`, and a `.zip` Open With entry under the current user. Uninstall preserves external custom data and restores the previous `.mrpack` association. Windows registry tests use an isolated test subtree rather than changing live application registrations.
