#!/usr/bin/env python3
"""Build the standalone executable with Cargo's committed lockfile."""
import argparse
import os
import shutil
import subprocess
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("cargo")
    parser.add_argument("manifest")
    parser.add_argument("target_dir")
    parser.add_argument("profile", choices=["debug", "release"])
    parser.add_argument("output")
    args = parser.parse_args()
    command = [args.cargo, "build", "--locked", "--manifest-path", args.manifest,
               "--target-dir", args.target_dir, "-p", "yggdrasil-server"]
    if args.profile == "release":
        command.append("--release")
    # A desktop environment may have both Pixi Rust and rustup on PATH. Keep
    # rustc beside the explicitly selected Cargo instead of mixing toolchains.
    env = os.environ.copy()
    cargo_path = shutil.which(args.cargo) or args.cargo
    env["PATH"] = str(Path(cargo_path).absolute().parent) + os.pathsep + env.get("PATH", "")
    subprocess.check_call(command, env=env)
    destination = Path(args.output)
    executable = "yggdrasil-server.exe" if destination.suffix == ".exe" else "yggdrasil-server"
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(Path(args.target_dir) / args.profile / executable, destination)


if __name__ == "__main__":
    main()
