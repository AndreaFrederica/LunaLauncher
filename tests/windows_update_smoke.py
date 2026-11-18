"""Exercise the real Windows SFX worker in a disposable portable installation.

Run inside Pixi after building the installer bins and launcher targets. No user
installation or system registration is changed; all fixtures are portable.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from windows_update_manifest import write_manifest


def run(command: list[str], success: bool = True) -> subprocess.CompletedProcess:
    result = subprocess.run(command, capture_output=True, timeout=45)
    if (result.returncode == 0) != success:
        raise AssertionError((command, result.returncode, result.stdout, result.stderr))
    return result


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--installer", type=Path, required=True)
    parser.add_argument("--assembler", type=Path, required=True)
    parser.add_argument("--launcher", type=Path, required=True)
    parser.add_argument("--runtime", type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="luna-windows-update-") as temporary:
        root = Path(temporary)
        payload = root / "payload"
        payload.mkdir()
        shutil.copy2(args.launcher, payload / "lunalauncher.exe")
        shutil.copy2(args.runtime / "luna-neo-ui.exe", payload / "luna-neo-ui.exe")
        for file in args.runtime.glob("*.dll"):
            try: os.link(file.resolve(), payload / file.name)
            except OSError: shutil.copy2(file, payload / file.name)
        shutil.copytree(args.runtime / "platforms", payload / "platforms")

        def package(name: str) -> Path:
            archive = root / f"{name}.zip"
            with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=1) as writer:
                for file in payload.rglob("*"):
                    if file.is_file(): writer.write(file, file.relative_to(payload).as_posix())
            setup = root / f"LunaLauncher-Windows-x64-{name}-Setup.exe"
            run([str(args.assembler.resolve()), "--stub", str(args.installer.resolve()), "--payload", str(archive), "--output", str(setup)])
            return setup

        target = root / "便携安装 (2)"
        data = target / "UserData"
        (data / "instances").mkdir(parents=True)
        (data / "logs").mkdir()
        world = data / "instances" / "world.txt"
        world.write_text("keep my world", encoding="utf-8")
        (target / "lunalauncher.exe").write_bytes(b"old launcher")
        (target / "obsolete.dll").write_bytes(b"old library")
        (target / "custom.json5").write_text("{user: true}")
        original_record = {"install_dir": str(target), "entry_point": "both", "portable_mode": True,
                           "instance_layout": "custom", "desktop_shortcuts": False,
                           "installed_entries": ["lunalauncher.exe", "obsolete.dll"]}
        (target / ".luna-install.json").write_text(json.dumps(original_record), encoding="utf-8")
        setup = package("test")
        manifest = json.loads(write_manifest(setup, payload).read_text())
        assert manifest["protocol"] == 1 and manifest["artifacts"][0]["neoUi"]
        assert manifest["artifacts"][0]["sha256"] == digest(setup)

        def update(executable: Path, success: bool) -> subprocess.CompletedProcess:
            return run([str(executable), "--update", "--update-headless", "--install-dir", str(target),
                        "--data-root", str(data), "--restart-entry", "neo", "--no-launch"], success)

        update(setup, True)
        assert world.read_text() == "keep my world"
        assert (target / "custom.json5").read_text() == "{user: true}"
        assert not (target / "obsolete.dll").exists()
        record = json.loads((target / ".luna-install.json").read_text())
        assert record["entry_point"] == "both" and record["portable_mode"] and record["instance_layout"] == "custom"
        state = json.loads((target / ".luna-update.json").read_text())
        assert state["restartEntry"] == "neo" and Path(state["backupDirectory"]).is_dir()
        assert (data / ".lunalauncher_update.success").exists()
        assert (target / "luna-installer-shell.exe").stat().st_size == setup.stat().st_size
        old_digest = digest(target / "lunalauncher.exe")
        old_record = (target / ".luna-install.json").read_bytes()

        damaged = root / "damaged.exe"
        shutil.copy2(setup, damaged)
        with damaged.open("r+b") as stream:
            stream.seek(-80, os.SEEK_END)
            stream.write(b"corrupt payload!")
        update(damaged, False)
        assert digest(target / "lunalauncher.exe") == old_digest and world.read_text() == "keep my world"

        # A valid SFX with an invalid new executable must restore the old files
        # and the exact installation record, not just report an extraction error.
        (payload / "lunalauncher.exe").write_bytes(b"invalid executable")
        update(package("bad-program"), False)
        assert digest(target / "lunalauncher.exe") == old_digest
        assert (target / ".luna-install.json").read_bytes() == old_record
        assert world.read_text() == "keep my world"
        assert (data / ".prism_launcher_update.fail").exists()
        (payload / "luna-neo-ui.exe").unlink()
        update(package("missing-neo"), False)
        assert digest(target / "lunalauncher.exe") == old_digest
        print("PASS: real SFX update, Unicode portable path, manifest/hash, UI choice, data preservation, corrupt payload rejection, executable failure rollback")


if __name__ == "__main__":
    main()
