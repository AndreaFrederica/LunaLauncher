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
import re
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
    parser.add_argument("--older-launcher", type=Path, help="An older real launcher compatible with the supplied runtime")
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

        def package(name: str, version: str | None = None) -> Path:
            archive = root / f"{name}.zip"
            with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=1) as writer:
                for file in payload.rglob("*"):
                    if file.is_file(): writer.write(file, file.relative_to(payload).as_posix())
                if version is not None:
                    writer.writestr(".luna-package-version", version + "\n")
            setup = root / f"LunaLauncher-Windows-x64-{name}-Setup.exe"
            run([str(args.assembler.resolve()), "--stub", str(args.installer.resolve()), "--payload", str(archive), "--output", str(setup)])
            return setup

        target = root / "便携安装 (2)" / "LunaLauncher"
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

        outer = target.parent / "双击启动.exe"
        unrelated = target.parent / "另一个安装包.exe"
        previous_setup = package("previous", manifest["version"])
        shutil.copy2(previous_setup, outer)
        (payload / "unrelated-marker.txt").write_text("unrelated download")
        shutil.copy2(package("unrelated"), unrelated)
        (payload / "unrelated-marker.txt").unlink()
        # Simulate an install that has already done an internal update. Its
        # external SFX will only match the backed-up installed shell.
        shutil.copy2(setup, target / "luna-installer-shell.exe")
        earlier = target.parent / ".luna-update-backup-legacy"
        earlier.mkdir()
        shutil.copy2(previous_setup, earlier / "luna-installer-shell.exe")
        (target / ".luna-update.json").write_text(json.dumps({"backupDirectory": str(earlier)}))
        unrelated_digest = digest(unrelated)
        assert digest(outer) != digest(setup)

        def update(executable: Path, success: bool, *options: str) -> subprocess.CompletedProcess:
            return run([str(executable), "--update", "--update-headless", "--install-dir", str(target),
                        "--data-root", str(data), "--restart-entry", "neo", "--no-launch", *options], success)

        update(setup, True)
        assert world.read_text() == "keep my world"
        assert (target / "custom.json5").read_text() == "{user: true}"
        assert not (target / "obsolete.dll").exists()
        record = json.loads((target / ".luna-install.json").read_text())
        assert record["entry_point"] == "both" and record["portable_mode"] and record["instance_layout"] == "custom"
        state = json.loads((target / ".luna-update.json").read_text())
        assert state["restartEntry"] == "neo" and Path(state["backupDirectory"]).is_dir()
        assert (data / ".lunalauncher_update.success").exists()
        assert digest(target / "luna-installer-shell.exe") == digest(outer) == digest(setup)
        assert record["adjacent_setup"] == outer.name
        assert digest(unrelated) == unrelated_digest
        print("PASS: discovered a custom-named legacy Setup from the previous backup and updated both EXEs")
        old_digest = digest(target / "lunalauncher.exe")
        old_record = (target / ".luna-install.json").read_bytes()

        if args.older_launcher:
            version = manifest["version"]
            numbered = package("local-upgrade", version)
            update(numbered, False, "--upgrade-to", version)
            assert digest(target / "lunalauncher.exe") == old_digest
            shutil.copy2(args.older_launcher, target / "lunalauncher.exe")
            older_digest = digest(target / "lunalauncher.exe")
            assert older_digest != old_digest
            wrong_version = re.sub(r"^\d+", "99", version)
            mismatched = package("wrong-version", wrong_version)
            update(mismatched, False, "--upgrade-to", wrong_version)
            assert "expected" in (data / ".prism_launcher_update.fail").read_text(encoding="utf-8")
            assert digest(target / "lunalauncher.exe") == older_digest
            assert world.read_text() == "keep my world"
            update(numbered, True, "--upgrade-to", version)
            assert digest(target / "lunalauncher.exe") == old_digest
            assert digest(outer) == digest(numbered)
            assert digest(unrelated) == unrelated_digest
            assert world.read_text() == "keep my world"
            old_record = (target / ".luna-install.json").read_bytes()
            print("PASS: local version comparison, equal version rejected, mismatched payload rolled back, real older launcher upgraded")

        damaged = root / "damaged.exe"
        shutil.copy2(setup, damaged)
        with damaged.open("r+b") as stream:
            stream.seek(-80, os.SEEK_END)
            stream.write(b"corrupt payload!")
        update(damaged, False)
        assert digest(target / "lunalauncher.exe") == old_digest and world.read_text() == "keep my world"

        # A valid SFX with an invalid new executable must restore the old files
        # and the exact installation record, not just report an extraction error.
        old_setup_digest = digest(outer)
        (payload / "lunalauncher.exe").write_bytes(b"invalid executable")
        update(package("bad-program"), False)
        assert digest(target / "lunalauncher.exe") == old_digest
        assert digest(target / "luna-installer-shell.exe") == digest(outer) == old_setup_digest
        assert (target / ".luna-install.json").read_bytes() == old_record
        assert world.read_text() == "keep my world"
        assert (data / ".prism_launcher_update.fail").exists()
        # Windows will refuse to replace an EXE opened without delete sharing.
        # The internal directory must roll back if publishing the outside EXE fails.
        import ctypes
        from ctypes import wintypes
        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel32.CreateFileW.argtypes = [
            wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, wintypes.LPVOID,
            wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE,
        ]
        kernel32.CreateFileW.restype = wintypes.HANDLE
        kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
        shutil.copy2(args.launcher, payload / "lunalauncher.exe")
        (payload / "marker.txt").write_text("new program files")
        pending = package("new")
        handle = kernel32.CreateFileW(str(outer), 0x80000000, 1, None, 3, 0, None)
        assert handle != ctypes.c_void_p(-1).value, ctypes.get_last_error()
        try:
            update(pending, False)
            assert "replace Setup" in (data / ".prism_launcher_update.fail").read_text(encoding="utf-8")
            assert digest(target / "luna-installer-shell.exe") == digest(outer) == old_setup_digest
            assert (target / ".luna-install.json").read_bytes() == old_record
            assert not (target / "marker.txt").exists()
            assert world.read_text() == "keep my world"
        finally:
            kernel32.CloseHandle(handle)
        update(pending, True)
        assert digest(target / "luna-installer-shell.exe") == digest(outer) == digest(pending)
        assert digest(unrelated) == unrelated_digest
        assert (target / "marker.txt").read_text() == "new program files"
        print("PASS: locked external Setup rolls back both entries and update succeeds after unlock")
        (payload / "luna-neo-ui.exe").unlink()
        update(package("missing-neo"), False)
        assert digest(target / "lunalauncher.exe") == old_digest
        print("PASS: real SFX update, Unicode portable path, manifest/hash, UI choice, data preservation, corrupt payload rejection, executable failure rollback")


if __name__ == "__main__":
    main()
