# Kvantum renderer in Luna

Upstream: https://github.com/tsujan/Kvantum

Pinned revision: `380d6c6940c02434f18e528b61ceeb166624b78f` (2026-10-21).
Only the Qt 6 style engine and its default resources are included. Copyright
notices are retained; GPL-3.0-or-later, see COPYING.

Luna builds a private static library, without the QStyle plugin, manager,
KDE WindowSystem or X11 integration. Downloaded theme archives are data only.

Downstream changes:

- The constructor accepts an explicit `.kvconfig` path and loads its sibling
  SVG. It never reads the global theme chooser or searches desktop theme
  directories, even when the explicit path is empty (built-in default).
- INI theme files use `QSettings::IniFormat` on all platforms.
- Desktop preference overrides are skipped for application-local themes.

Keep these changes when updating upstream. Native compositing effects require
the desktop compositor and are outside this private renderer.
