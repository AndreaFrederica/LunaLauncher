"""Run portable directory update tests in the pinned Meson environment."""
import os
from pathlib import Path
import sys
import pixi_meson

root = pixi_meson.repo_root()
env = pixi_meson.task_env(root, "release")
pixi_meson.build(root, "release", ["LinuxPortableUpdate_test"])
if len(sys.argv) > 1:
    bundle = Path(sys.argv[1]).resolve()
    env["LUNA_TEST_PORTABLE_ARCHIVE"] = str(bundle)
    env["LUNA_TEST_PORTABLE_MANIFEST"] = str(bundle.parent / "linux-x64-portable-update.json")
pixi_meson.run(["meson", "test", "-C", str(pixi_meson.build_dir(root, "release")),
                "--no-rebuild", "--print-errorlogs", "LinuxPortableUpdate_test"], cwd=root, env=env)
