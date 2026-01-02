#!/usr/bin/env python3
import subprocess
import platform
import sys
from pathlib import Path

VCPKG_REPO = "https://github.com/microsoft/vcpkg.git"
VCPKG_COMMIT = None  # e.g. "2024.12.16" 或具体 commit hash


def run(cmd, cwd=None):
    print(f"running {' '.join(cmd)} in {cwd}")
    subprocess.check_call(cmd, cwd=cwd)


def clone_vcpkg(vcpkg_dir: Path):
    if vcpkg_dir.exists():
        print(f"vcpkg already exists: {vcpkg_dir}")
        return

    run([
        "git", "clone",
        "--depth", "1",
        VCPKG_REPO,
        str(vcpkg_dir)
    ])

    if VCPKG_COMMIT:
        run(["git", "fetch", "--depth", "1", "origin", VCPKG_COMMIT], cwd=vcpkg_dir)
        run(["git", "checkout", VCPKG_COMMIT], cwd=vcpkg_dir)


def bootstrap(vcpkg_dir: Path):
    system = platform.system()
    if system == "Windows":
        run(["bootstrap-vcpkg.bat"], vcpkg_dir)
    else:
        sh = vcpkg_dir / "bootstrap-vcpkg.sh"
        sh.chmod(0o755)
        run([str(sh)], cwd=vcpkg_dir)


def main():

    vcpkg_dir : Path = Path("third_party") / "vcpkg"

    print(f"Using private vcpkg at: {vcpkg_dir}")

    clone_vcpkg(vcpkg_dir)
    bootstrap(vcpkg_dir)

    exe = "vcpkg.exe" if platform.system() == "Windows" else "vcpkg"
    vcpkg = vcpkg_dir / exe

    if not vcpkg.exists():
        print("ERROR: vcpkg bootstrap failed", file=sys.stderr)
        sys.exit(1)

    print()
    print("Private vcpkg ready.")
    print("Use it via CMake toolchain only (manifest mode).")


if __name__ == "__main__":
    main()
