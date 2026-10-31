# Runtime resource paths

Runtime data that can be updated, overridden, or redistributed separately from
the executable is resolved through `launcher/LauncherResources.{h,cpp}`.

`LauncherResources::searchPaths(relativeDirectory)` returns directories in this
order:

1. the active launcher data directory (`Application::dataRoot()`), so users can
   override packaged data;
2. the platform installation resource directories;
3. a source-tree `launcher/exresources` directory when running a development
   build.

The installation candidates are derived from the executable location and the
platform layout:

- Windows: `<launcher>/resource`;
- macOS: `<Launcher>.app/Contents/Resources`;
- Unix: `share/<launcher>/resources` next to the executable, followed by the
  standard `/usr` and `/usr/local` prefixes.

Use `locateFile("directory/file")` for a single data file and
`installPath("directory")` when a component needs to watch or write the
packaged directory. New mutable data should be installed under
`launcher/exresources/<directory>` and added to both the Meson and CMake
`exresources` install lists. Do not add mutable or separately licensed data to
Qt resources; immutable UI assets may continue to use qrc.

The current migration covers fonts, icons, translations, themes, catpacks,
MCMod data, and the JS API files. JS APIs are loaded from this filesystem tree;
they are no longer compiled into a qrc resource.
