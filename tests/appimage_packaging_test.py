"""Check AppRun's shell entry routing, quoting and data-root preservation."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


class AppRunTest(unittest.TestCase):
    def test_portable_runtime_requirements(self):
        with tempfile.TemporaryDirectory(prefix="luna-glibc-") as temporary:
            root = Path(temporary)
            requirements = root / "usr/share/luna-portable.requirements"
            requirements.parent.mkdir(parents=True)
            app_run = Path(__file__).resolve().parents[1] / "scripts/appimage/AppRun"
            requirements.write_text("x64\n999.0\n", encoding="utf-8")
            env = {**os.environ, "APPDIR": str(root)}
            result = subprocess.run(["sh", str(app_run), "--version"], env=env, capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("requires glibc >= 999.0", result.stderr)
            self.assertFalse((root / "usr/UserData").exists())

    def test_entries_and_portable_data(self):
        with tempfile.TemporaryDirectory(prefix="luna 空格 ") as temporary:
            root = Path(temporary)
            binary = root / "usr/bin"
            binary.mkdir(parents=True)
            app_run = root / "AppRun"
            shutil.copyfile(Path(__file__).resolve().parents[1] / "scripts/appimage/AppRun", app_run)
            for name in ("lunalauncher", "lunalauncher_updater", "luna-neo-ui", "luna-cf-webview.bin"):
                path = binary / name
                path.write_text('#!/bin/sh\nprintf "%s\\n" "${0##*/}" "$@"\n', encoding="utf-8")
                path.chmod(0o755)
            env = {**os.environ, "APPDIR": str(root), "LUNA_LAUNCHER_DATA": str(root / "数据 目录")}
            env.pop("LUNA_LAUNCHER_ENTRY", None)
            env.pop("XDG_CURRENT_DESKTOP", None)
            hooks = root / "apprun-hooks"
            hooks.mkdir()
            (hooks / "qt.sh").write_text('case "${XDG_CURRENT_DESKTOP}" in GNOME) :;; esac\n', encoding="utf-8")
            def run(*arguments):
                return subprocess.run(["sh", str(app_run), *arguments], env=env, capture_output=True, text=True, check=True).stdout.splitlines()
            self.assertEqual(run(), ["luna-neo-ui"])
            self.assertEqual(run("--version"), ["lunalauncher", "--dir", str(root / "数据 目录"), "--version"])
            self.assertEqual(run("--luna-entry", "classic", "a b"), ["lunalauncher", "--dir", str(root / "数据 目录"), "a b"])
            self.assertEqual(run("--luna-entry", "updater", "--check-only"), ["lunalauncher_updater", "--check-only"])
            self.assertEqual(run("--luna-entry", "cf-webview", "a b"), ["luna-cf-webview.bin", "a b"])
            (binary / "luna-neo-ui").unlink()
            self.assertEqual(run(), ["lunalauncher", "--dir", str(root / "数据 目录")])
            failure = subprocess.run(["sh", str(app_run), "--luna-entry", "invalid"], env=env, capture_output=True)
            self.assertEqual(failure.returncode, 2)


if __name__ == "__main__":
    unittest.main()
