#!/usr/bin/env python3
"""Frontend-only workflows; the native build remains owned by Meson."""
import argparse
from pathlib import Path
import shutil
import sys

from build_neo_ui import prepare_frontend, run
from pixi_meson import build_dir, installed_binary_dir, repo_root, task_env
from pixi_meson import run as run_native


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=["deps", "assets", "run", "test"])
    parser.add_argument("profile", nargs="?", default="release", choices=["debug", "release"])
    args = parser.parse_args()
    root = repo_root()
    if args.command in {"run", "test"}:
        binary = installed_binary_dir(root, args.profile) / ("luna-neo-ui.exe" if sys.platform == "win32" else "luna-neo-ui")
        if not binary.is_file():
            raise SystemExit(f"Neo UI not installed at {binary}. Run: pixi run install-webui {args.profile}")
        command = [str(binary)] if args.command == "run" else [sys.executable, str(root / "tests/webui_smoke.py"), "--exe", str(binary)]
        run_native(command, cwd=binary.parent, env=task_env(root, args.profile))
        return
    pnpm = shutil.which("pnpm")
    if not pnpm:
        raise SystemExit("pnpm is unavailable; run this command through Pixi")
    source = root / "neo-ui"
    if args.command == "deps":
        prepare_frontend(Path(pnpm), source, build_dir(root, args.profile) / "neo-ui")
    else:
        run([pnpm, "build"], source)
        print(f"External Web UI bundle: {source / 'dist' / 'spa'}", flush=True)


if __name__ == "__main__":
    main()
