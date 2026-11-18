"""Check an actual deployed portable tarball's MCP API and optional NeoUI."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--archive", required=True, type=Path)
parser.add_argument("--neo", action="store_true")
args = parser.parse_args()
manifest = json.loads((args.archive.parent / "linux-x64-portable-update.json").read_text(encoding="utf-8"))
item = next(record for record in manifest["artifacts"] if record["asset"] == args.archive.name)
with args.archive.open("rb") as stream:
    assert hashlib.file_digest(stream, "sha256").hexdigest() == item["sha256"]
assert args.archive.stat().st_size == item["size"]
with tempfile.TemporaryDirectory(prefix="luna-portable-runtime-") as temporary:
    with tarfile.open(args.archive) as bundle:
        bundle.extractall(temporary, filter="data")
    root = Path(temporary)
    scripts = Path(__file__).resolve().parent
    env = {key: value for key, value in os.environ.items() if key not in {
        "APPDIR", "APPIMAGE", "LD_LIBRARY_PATH", "LD_PRELOAD", "QT_PLUGIN_PATH",
        "QT_QPA_PLATFORM_PLUGIN_PATH", "LUNA_LAUNCHER_DATA", "LUNA_LAUNCHER_ENTRY"}}
    env.update(PATH="/usr/bin:/bin", QT_QPA_PLATFORM="offscreen")
    for entry, product in (("classic", "LunaLauncher"), ("updater", "LunaLauncherUpdater")):
        version = subprocess.run([str(root / "AppRun"), "--luna-entry", entry, "--version"],
                                 env=env, capture_output=True, text=True, check=True, timeout=30)
        assert f"{product} {manifest['version']}" in version.stdout, version.stdout
    context = subprocess.run([str(root / "AppRun"), "--cli", "--print-startup-context"],
                             env=env, capture_output=True, text=True, check=True, timeout=30)
    assert Path(json.loads(context.stdout)["dataRoot"]) == root / "usr/UserData", context.stdout
    subprocess.run([sys.executable, str(scripts / "appimage_api_smoke.py"), "--portable", "--image", str(root / "AppRun")], check=True)
    if args.neo:
        assert item["neoUi"], "This package does not contain NeoUI"
        subprocess.run([sys.executable, str(scripts / "appimage_neo_smoke.py"), "--portable", "--image", str(root / "AppRun")], check=True)
    print(f"PASS: actual portable tarball, glibc >= {item['minGlibc']}, SHA-256 and isolated runtime")
