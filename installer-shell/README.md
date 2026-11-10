# LunaLauncher Installer Shell

This is a standalone Rust/egui installer and SFX reader for LunaLauncher.
It intentionally does not link to the Qt launcher or depend on the main
repository's C++ build. The shell can be built, tested, and used with an
external ZIP payload before it is assembled into a signed SFX executable.

The project is licensed under the Mozilla Public License 2.0. The launcher
payload remains under its own licenses, including GPLv3 for LunaLauncher.

## Development commands

```text
cargo run -- --payload path/to/app.zip --list
cargo run -- --payload path/to/app.zip --extract path/to/output
cargo run -- --payload path/to/app.zip --install
cargo run -- --payload path/to/app.zip --install --install-dir path/to/target --no-launch
cargo run -- --uninstall path/to/installation
cargo run -- --payload path/to/app.zip
```

When launched without an explicit command, the egui window first offers an
install wizard, an extract-only action, and a read-only archive viewer. The
viewer lists UTF-8 ZIP entries with a filter and can extract the complete
archive to a directory. Viewing or extracting does not mark installation as
complete; until the install wizard succeeds, the next launch returns to this
choice screen.

The wizard detects the process locale, defaults to Simplified Chinese for
unknown Windows locales, and provides an English/简体中文 switch in the window.
The install page is organized as four beginner-friendly steps: location, UI,
data/instance mode, and shortcuts.

The install wizard also configures the launcher's existing data conventions:
MMC mode selects non-portable data and leaves the current directory
unregistered; 原版/HMCL/PCL mode keeps the shell in the current directory,
installs the launcher payload into a `LunaLauncher/` child directory, enables
portable data, and registers that child as a vanilla-compatible store; 自定义
mode exposes the portable checkbox, store path, format, and registration
toggle. Reopening the package shell from either location starts the saved UI
selection directly after installation.

Release ZIP creation omits the portable/system marker files because they are
machine-local installation state rather than launcher payload files.

The SFX assembler places a fixed footer before the ZIP payload. This keeps the
archive readable by standard tools without a trailing private-data warning,
while the shell can still verify the payload hash before extraction.

The shell copies itself into the installed directory so the generated
uninstall shortcut remains usable after the downloaded SFX is removed.
