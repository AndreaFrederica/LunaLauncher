#!/usr/bin/env python3
import shutil
import os
import subprocess
import sys
import tempfile
from pathlib import Path


def run(command, cwd):
    print("+", " ".join(str(part) for part in command), flush=True)
    env = os.environ.copy()
    env["CI"] = "true"  # Meson custom targets cannot answer pnpm reinstall prompts.
    subprocess.check_call([str(part) for part in command], cwd=cwd, env=env)


def prepare_frontend(pnpm, source, build):
    run([pnpm, "install", "--frozen-lockfile", "--store-dir", build / "pnpm-store"], source)


def main():
    pnpm, cargo, source, build, output = map(Path, sys.argv[1:])
    source = source.resolve()
    build = build.resolve()
    # The source checkout is often hosted below the Codex workspace ACL.  That
    # ACL permits normal writes but denies delete-child, while Quasar/esbuild
    # creates an adjacent temporary .mjs file with delete sharing enabled. On
    # Windows this is reported as ERROR_ACCESS_DENIED even though a shell can
    # create ordinary files in the same directory. Build the Web UI from a
    # disposable copy in the user's temp directory so the native MSVC build
    # remains reproducible under both protected and normal checkouts.
    with tempfile.TemporaryDirectory(prefix="luna-neo-ui-") as temporary:
        frontend = Path(temporary) / "neo-ui"
        shutil.copytree(
            source,
            frontend,
            ignore=shutil.ignore_patterns("node_modules", "dist", ".quasar", "*.temporary.*"),
        )
        # tauri.conf.json keeps the shared launcher icon outside neo-ui. Keep
        # the same relative layout in the staging copy for Windows resource
        # generation.
        program_info = source.parent / "program_info"
        if program_info.is_dir():
            shutil.copytree(program_info, Path(temporary) / "program_info")
        prepare_frontend(pnpm, frontend, build)
        run([pnpm, "build"], frontend)
        cargo_target = build / "cargo-target"
        run(
            [cargo, "build", "--locked", "--release", "--features", "custom-protocol", "--target-dir", cargo_target],
            frontend / "src-tauri",
        )
        binary = cargo_target / "release" / ("luna-neo-ui.exe" if sys.platform == "win32" else "luna-neo-ui")
        if not binary.is_file():
            raise SystemExit(f"Neo UI build did not produce {binary}")
        output.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(binary, output)


if __name__ == "__main__":
    main()
