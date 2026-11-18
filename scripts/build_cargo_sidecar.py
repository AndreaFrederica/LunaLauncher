#!/usr/bin/env python3

import shutil
import subprocess
import sys
import platform
from pathlib import Path


def desktop_cargo_command(cargo, arguments):
    """Keep Pixi's compiler, using the host CRT with its GTK/WebKit SDK.

    The conda GNU sysroot has an older glibc. Mixing its Scrt1.o with the
    distro GTK library search paths fails with missing __libc_csu_* symbols.
    Apply this only to the final desktop binary, preserving dependency caches.
    """
    if sys.platform != "linux":
        return [cargo, "build", *arguments]
    multiarch = "aarch64-linux-gnu" if platform.machine() == "aarch64" else "x86_64-linux-gnu"
    candidates = (Path("/usr/lib") / multiarch, Path("/usr/lib64"), Path("/usr/lib"))
    crt = next((path for path in candidates if (path / "Scrt1.o").is_file()), None)
    if crt is None:
        raise SystemExit("Native GTK/WebKit builds require the host glibc development startup files (libc6-dev/glibc-devel)")
    return [cargo, "rustc", *arguments, "--", "-C", "link-arg=--sysroot=/", "-C", f"link-arg=-B{crt}/"]


def main() -> None:
    if len(sys.argv) != 6:
        raise SystemExit(
            "usage: build_cargo_sidecar.py <cargo> <manifest> <target-dir> <profile> <output>"
        )

    cargo, manifest, target_dir, profile, output = sys.argv[1:]
    arguments = [
        "--locked",
        "--manifest-path",
        manifest,
        "--target-dir",
        target_dir,
        "--bin", "luna-cf-webview",
    ]
    if profile == "release":
        arguments.append("--release")

    subprocess.check_call(desktop_cargo_command(cargo, arguments))

    executable_name = "luna-cf-webview.exe" if sys.platform == "win32" else "luna-cf-webview"
    built = Path(target_dir) / profile / executable_name
    destination = Path(output)
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(built, destination)


if __name__ == "__main__":
    main()
