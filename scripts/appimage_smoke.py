#!/usr/bin/env python3
"""Build and run AppImage update tests through the pinned Meson environment."""
from pathlib import Path
import sys

import pixi_meson

root = pixi_meson.repo_root()
env = pixi_meson.task_env(root, "release")
directory = pixi_meson.build_dir(root, "release")
pixi_meson.run([sys.executable, str(root / "tests/appimage_packaging_test.py")], cwd=root, env=env)
pixi_meson.build(root, "release", ["AppImageUpdate_test", "WindowsUpdateManifest_test"])
if len(sys.argv) > 1:
    env["LUNA_TEST_APPIMAGE"] = str(Path(sys.argv[1]).resolve())
pixi_meson.run(["meson", "test", "-C", str(directory), "--no-rebuild", "--print-errorlogs",
                "AppImageUpdate_test", "WindowsUpdateManifest_test"], cwd=root, env=env)
