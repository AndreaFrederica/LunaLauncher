# SPDX-License-Identifier: GPL-3.0-only
"""Run against a deployed CLI; all profiles and portable moves use temporary copies."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--install", type=Path, default=Path("install-webui-portable-release"))
    args = parser.parse_args()
    root = Path(tempfile.mkdtemp(prefix="luna-mmc-portable-test-"))
    env = os.environ.copy()
    for key in ("LUNA_LAUNCHER_DATA", "LUNA_LAUNCHER_CLI", "LUNALAUNCHER_DATA_DIR"):
        env.pop(key, None)
    env["QT_QPA_PLATFORM"] = "offscreen"
    sequence = 0

    def api(exe, operation, parameters=None, data=None):
        nonlocal sequence
        sequence += 1
        cmd = [str(exe), "--cli", "--json", "--non-interactive"]
        if data is not None:
            cmd += ["--dir", str(data)]
        cmd += ["api", operation, json.dumps(parameters or {})]
        result = subprocess.run(cmd, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=120)
        (root / f"{sequence}.stderr").write_bytes(result.stderr)
        lines = [line for line in result.stdout.decode("utf-8", errors="replace").splitlines() if line.startswith("{")]
        assert lines, (operation, result.returncode, result.stderr[-1000:])
        response = json.loads(lines[-1])
        assert response.get("ok"), response
        return response["data"]

    exe = args.install.resolve() / "lunalauncher-cli.exe"
    data = root / "data"
    fixtures = [("forge", "net.minecraftforge", "14.23.5.2864"), ("cleanroom", "com.cleanroommc.cleanroom", "0.6.13-alpha"), ("fabric", "net.fabricmc.fabric-loader", "0.16.0"), ("vanilla", None, ""), ("disabled", "net.minecraftforge", "14.23.5.2864")]
    for name, uid, version in fixtures:
        instance = data / "instances" / name
        instance.mkdir(parents=True)
        (instance / "instance.cfg").write_text(f"[General]\nInstanceType=OneSix\nname={name}\n")
        components = [{"uid": "net.minecraft", "version": "1.12.2"}]
        if uid:
            components.append({"uid": uid, "version": version, "disabled": name == "disabled"})
        (instance / "mmc-pack.json").write_text(json.dumps({"formatVersion": 1, "components": components}))
    broken = data / "instances" / "broken"
    broken.mkdir()
    (broken / "instance.cfg").write_text("[General]\nInstanceType=OneSix\nname=broken\n")
    (broken / "mmc-pack.json").write_text("invalid-json")
    listing = {item["id"]: item for item in api(exe, "instance.list", data=data)}
    for name, uid, version in fixtures:
        expected = "vanilla" if name == "disabled" else name
        item = listing[name]
        assert item["loaderType"] == expected, item
        assert item["minecraftVersion"] == "1.12.2", item
        for operation in ("instance.info", "instance.get"):
            item = api(exe, operation, {"instance": name}, data)
            assert item["loaderType"] == expected, item
            actual_version = item.get("minecraftVersion") or item.get("runtime", {}).get("minecraftVersion")
            assert actual_version == "1.12.2", item
    assert listing["broken"]["loaderType"] == "unknown", listing["broken"]
    print("PASS: cold list/info/get identify Forge, Cleanroom, Fabric, vanilla, disabled loader and broken metadata")

    # A real portable relocation must discard its old automatic store while
    # retaining an explicitly registered external folder.
    first = root / "portable-a"
    shutil.copytree(args.install, first, ignore=shutil.ignore_patterns("UserData", "logs", ".luna-use-system-data"))
    user_data = first / "UserData"
    user_data.mkdir()
    (first / "portable.txt").write_text("portable\n")
    manual = root / "explicit-external"
    manual.mkdir()
    registry = {"formatVersion": 1, "InstanceStores": [{"id": "manual", "name": "manual", "path": str(manual), "format": "prism", "origin": "user", "enabled": True, "writable": True}]}
    (user_data / "instance-stores.json").write_text(json.dumps(registry))
    api(first / "lunalauncher-cli.exe", "instance.list")
    old_portable = first / "UserData/instances"
    assert old_portable.is_dir()
    second = root / "portable-b"
    first.rename(second)
    api(second / "lunalauncher-cli.exe", "instance.list")
    saved = json.loads((second / "UserData/instance-stores.json").read_text())
    stores = saved["InstanceStores"]
    derived = [s for s in stores if s["origin"] == "portable-derived"]
    assert len(derived) == 1, stores
    assert Path(derived[0]["path"]).resolve() == (second / "UserData/instances").resolve(), derived
    assert any(s["id"] == "manual" for s in stores), stores
    assert manual.is_dir()
    api(second / "lunalauncher-cli.exe", "instance.list", data=second / "UserData")
    stores = json.loads((second / "UserData/instance-stores.json").read_text())["InstanceStores"]
    assert not any(s["origin"] == "portable-derived" for s in stores), stores
    assert any(s["id"] == "manual" for s in stores), stores
    print("PASS: portable relocation, system-mode pruning and explicit external store preservation")
    print(root)


if __name__ == "__main__":
    main()
