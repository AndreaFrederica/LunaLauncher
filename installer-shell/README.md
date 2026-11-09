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

The SFX assembler places a fixed footer before the ZIP payload. This keeps the
archive readable by standard tools without a trailing private-data warning,
while the shell can still verify the payload hash before extraction.

The shell copies itself into the installed directory so the generated
uninstall shortcut remains usable after the downloaded SFX is removed.
