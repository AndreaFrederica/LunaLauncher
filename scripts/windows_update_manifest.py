#!/usr/bin/env python3
"""Generate the protocol-1 Windows update catalog beside a verified SFX."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess


def write_manifest(setup: Path, installation: Path, architecture: str = "x64") -> Path:
    # Read the shipped binary, not the Git tag or the packaging filename. These
    # can differ for manually published builds and same-day release revisions.
    output = subprocess.run([str((installation / "lunalauncher.exe").resolve()), "--version"],
                            check=True, capture_output=True, text=True, timeout=15).stdout
    match = re.search(r"\b(\d+\.\d+\.\d+(?:[-+][A-Za-z0-9.-]+)?)", output)
    if not match:
        raise RuntimeError(f"Cannot determine shipped launcher version: {output!r}")
    digest = hashlib.sha256()
    with setup.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    manifest = {
        "schemaVersion": 1, "protocol": 1, "version": match.group(1),
        "artifacts": [{"platform": "windows", "architecture": architecture,
                       "neoUi": (installation / "luna-neo-ui.exe").is_file(),
                       "asset": setup.name, "size": setup.stat().st_size,
                       "sha256": digest.hexdigest()}],
    }
    destination = setup.parent / f"windows-{architecture}-update.json"
    destination.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    return destination


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--setup", type=Path, required=True)
    parser.add_argument("--installation", type=Path, required=True)
    parser.add_argument("--architecture", choices=["x64", "arm64"], default="x64")
    args = parser.parse_args()
    print(write_manifest(args.setup, args.installation, args.architecture))
