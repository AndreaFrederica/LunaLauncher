#!/usr/bin/env python3
import shutil
import os
import subprocess
import sys
from pathlib import Path


def run(command, cwd):
    print("+", " ".join(str(part) for part in command), flush=True)
    env = os.environ.copy()
    env["CI"] = "true"  # Meson custom targets cannot answer pnpm reinstall prompts.
    subprocess.check_call([str(part) for part in command], cwd=cwd, env=env)


pnpm, cargo, source, build, output = map(Path, sys.argv[1:])
source = source.resolve()
build = build.resolve()
install = build / "pnpm-store"
run([pnpm, "install", "--frozen-lockfile", "--store-dir", install], source)
run([pnpm, "build"], source)
cargo_target = build / "cargo-target"
run([cargo, "build", "--locked", "--release", "--features", "custom-protocol", "--target-dir", cargo_target], source / "src-tauri")
binary = cargo_target / "release" / ("luna-neo-ui.exe" if sys.platform == "win32" else "luna-neo-ui")
if not binary.is_file():
    raise SystemExit(f"Neo UI build did not produce {binary}")
Path(output).parent.mkdir(parents=True, exist_ok=True)
shutil.copy2(binary, output)
