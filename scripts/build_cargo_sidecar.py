#!/usr/bin/env python3

import shutil
import subprocess
import sys
from pathlib import Path


def main() -> None:
    if len(sys.argv) != 6:
        raise SystemExit(
            "usage: build_cargo_sidecar.py <cargo> <manifest> <target-dir> <profile> <output>"
        )

    cargo, manifest, target_dir, profile, output = sys.argv[1:]
    command = [
        cargo,
        "build",
        "--locked",
        "--manifest-path",
        manifest,
        "--target-dir",
        target_dir,
    ]
    if profile == "release":
        command.append("--release")

    subprocess.check_call(command)

    executable_name = "luna-cf-webview.exe" if sys.platform == "win32" else "luna-cf-webview"
    built = Path(target_dir) / profile / executable_name
    destination = Path(output)
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(built, destination)


if __name__ == "__main__":
    main()
