#!/usr/bin/env python3
"""Build a separate portable Web UI tree without borrowing system user data."""
import os
from pathlib import Path
from pixi_meson import configure, build, install


def main():
    root = Path(__file__).resolve().parent.parent
    destination = root / "install-webui-portable-release"
    if (destination / ".luna-use-system-data").exists():
        raise SystemExit("The portable output has explicitly selected system data; choose a fresh output before packaging.")
    os.environ["LUNA_INSTALL_DIR"] = str(destination)
    os.environ["LUNA_NEO_UI"] = "enabled"
    configure(root, "release")
    build(root, "release")
    install(root, "release")
    (destination / "UserData").mkdir(exist_ok=True)
    (destination / "portable.txt").write_text("LunaLauncher portable mode\n", encoding="utf-8")
    print(f"Portable launcher: {destination / 'luna-neo-ui.exe'}", flush=True)


if __name__ == "__main__":
    main()
