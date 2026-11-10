#!/usr/bin/env python3
"""Package an installed Windows launcher tree as a ZIP and self-extracting SFX."""

from __future__ import annotations

import argparse
import hashlib
import os
import re
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent


def run(command: list[str]) -> None:
    print("+", " ".join(command), flush=True)
    subprocess.run(command, cwd=ROOT, check=True)


def launcher_version() -> str:
    meson = (ROOT / "meson.build").read_text(encoding="utf-8")
    match = re.search(r"version:\s*'([^']+)'", meson)
    if not match:
        raise SystemExit("could not determine launcher version from meson.build")
    return match.group(1)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("profile", choices=["release"])
    args = parser.parse_args()

    if os.name != "nt":
        raise SystemExit("Windows SFX packaging is only available on Windows")

    webui = os.environ.get("LUNA_NEO_UI", "disabled").lower() == "enabled"
    install_name = f"install-webui-{args.profile}" if webui else f"install-{args.profile}"
    source = ROOT / install_name
    if not source.is_dir():
        raise SystemExit(f"installed launcher tree not found: {source}")

    stub = ROOT / "installer-shell" / "target" / "release" / "luna-installer-shell.exe"
    create_zip = ROOT / "installer-shell" / "target" / "release" / "create-zip.exe"
    assemble_sfx = ROOT / "installer-shell" / "target" / "release" / "assemble-sfx.exe"
    for tool in (stub, create_zip, assemble_sfx):
        if not tool.is_file():
            raise SystemExit(f"installer packaging tool not found: {tool}")

    output_dir = ROOT / "release-assets"
    output_dir.mkdir(exist_ok=True)
    suffix = "-webui" if webui else ""
    stem = f"LunaLauncher-Windows-x64-{launcher_version()}{suffix}"
    payload = output_dir / f"{stem}.zip"
    sfx = output_dir / f"{stem}-Setup.exe"

    run([str(create_zip), "--source", str(source), "--output", str(payload)])
    run([str(assemble_sfx), "--stub", str(stub), "--payload", str(payload), "--output", str(sfx)])

    for artifact in (payload, sfx):
        checksum = output_dir / f"{artifact.name}.sha256"
        digest = sha256(artifact)
        checksum.write_text(f"{digest}  {artifact.name}\n", encoding="ascii")
        print(f"SHA256 {digest}  {artifact.name}")
    print(f"Packaged {payload}")
    print(f"Packaged {sfx}")


if __name__ == "__main__":
    main()
