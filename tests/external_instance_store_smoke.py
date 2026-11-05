# SPDX-License-Identifier: GPL-3.0-only
"""Acceptance test against an installed launcher copied outside the build tree."""
import argparse
import json
import tempfile
import unittest
from pathlib import Path

from api_sidecar_smoke import Sidecar


class ExternalInstanceStoreSmoke(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory(prefix="luna-external-store-")
        cls.root = Path(cls.tmp.name)
        cls.prism_a = cls.root / "Prism A"
        cls.prism_b = cls.root / "Prism B"
        cls.vanilla = cls.root / "HMCL-PCL"
        for path in (cls.prism_a, cls.prism_b, cls.vanilla):
            path.mkdir(parents=True)

        # PCL/HMCL-compatible shared-layout fixture.
        version = cls.vanilla / "versions" / "1.20.1"
        version.mkdir(parents=True)
        (version / "1.20.1.json").write_text(
            json.dumps({"id": "1.20.1", "type": "release"}), encoding="utf-8"
        )
        # A vanilla import must have either a client download URL or the
        # locally installed client artifact that HMCL/PCL would normally keep
        # beside the version metadata.
        (version / "1.20.1.jar").write_bytes(b"client")
        (cls.vanilla / "PCL.ini").write_text("fixture", encoding="utf-8")
        (cls.vanilla / "mods").mkdir()
        (cls.vanilla / "mods" / "common.jar").write_bytes(b"mod")
        (cls.vanilla / "config").mkdir()
        (cls.vanilla / "config" / "example.cfg").write_text("a=1\n", encoding="utf-8")

        cls.client = cls.root / "sync-client"
        cls.server = cls.root / "sync-server"
        (cls.client / "mods").mkdir(parents=True)
        (cls.server / "mods").mkdir(parents=True)
        (cls.client / "config").mkdir()
        (cls.server / "config").mkdir()
        (cls.client / "config" / "common.cfg").write_text("a=1\nb=1\n", encoding="utf-8")
        (cls.server / "config" / "common.cfg").write_text("a=1\nb=1\n", encoding="utf-8")

        cls.sidecar = Sidecar(ARGS.exe, cls.root / "launcher-data")
        for sid, name, path, fmt, default in (
            ("prism-a", "Prism A", cls.prism_a, "prism", True),
            ("prism-b", "Prism B", cls.prism_b, "prism", False),
            ("hmcl-pcl", "HMCL/PCL", cls.vanilla, "vanilla-compatible", False),
        ):
            cls.sidecar.ok("instance-store.add", id=sid, name=name, path=str(path), format=fmt,
                           enabled=True, writable=True, defaultForNewInstances=default)

    @classmethod
    def tearDownClass(cls):
        try:
            cls.sidecar.close()
        finally:
            cls.tmp.cleanup()

    def test_store_registration_and_vanilla_scan(self):
        s = self.sidecar
        s.ok("instance-store.add", id="prism-a", name="Prism A", path=str(self.prism_a),
             format="prism", enabled=True, writable=True, defaultForNewInstances=True)
        s.ok("instance-store.add", id="prism-b", name="Prism B", path=str(self.prism_b),
             format="prism", enabled=True, writable=True)
        s.ok("instance-store.add", id="hmcl-pcl", name="HMCL/PCL", path=str(self.vanilla),
             format="vanilla-compatible", enabled=True, writable=True)
        stores = s.ok("instance-store.list")["stores"]
        self.assertTrue({"prism-a", "prism-b", "hmcl-pcl"}.issubset({x["id"] for x in stores}))
        instances = s.ok("vanilla-instance.list", storeId="hmcl-pcl")["instances"]
        self.assertEqual(len(instances), 1)
        self.assertEqual(instances[0]["dialect"], "pcl")
        self.assertFalse(instances[0]["isolated"])

    def test_import_and_mmc_export_block(self):
        s = self.sidecar
        imported = s.ok("instance.convert.import", source=str(self.vanilla), targetStoreId="prism-a",
                        name="Imported HMCL", copyGameContent=True)
        destination = Path(imported["destination"] if "destination" in imported else imported.get("target", ""))
        if not destination.is_absolute():
            destination = self.prism_a / "Imported HMCL"
        self.assertTrue((destination / "instance.cfg").is_file())
        self.assertTrue((destination / "mmc-pack.json").is_file())
        self.assertTrue((destination / "minecraft" / "mods" / "common.jar").is_file()
                        or (destination / ".minecraft" / "mods" / "common.jar").is_file()
                        or (destination / "mods" / "common.jar").is_file())

        blocked = s.execute("instance.convert.export", source=str(destination),
                            target=str(self.root / "blocked-export"), versionId="1.20.1")
        self.assertFalse(blocked["ok"])

    def test_loader_install_and_sync(self):
        s = self.sidecar
        loader_target = self.root / "loader-vanilla"
        base = loader_target / "versions" / "1.20.1"
        base.mkdir(parents=True)
        (base / "1.20.1.json").write_text(json.dumps({"id": "1.20.1", "type": "release"}), encoding="utf-8")
        installed = s.ok("vanilla.loader.install", target=str(loader_target), versionId="1.20.1",
                         loaderId="net.fabricmc.fabric-loader", loaderVersion="0.16.5")
        self.assertTrue(installed["succeeded"])
        self.assertTrue(list((loader_target / "versions").glob("1.20.1-*/*.json")))

        profile = {
            "id": "external-sync", "name": "External sync",
            "client": {"instanceId": "client"}, "server": {"instanceId": "server"},
            "mods": {"common": [], "clientOnly": [], "serverOnly": []},
            "configs": {"common": ["config/common.cfg"], "clientOnly": [], "serverOnly": []},
            "sync": {"mode": "bidirectional", "conflictPolicy": "ask",
                      "baseHashes": {}, "baseContents": {}},
        }
        s.ok("sync.profile.save", profile=profile)
        plan = s.ok("sync.profile.plan", id="external-sync", clientRoot=str(self.client), serverRoot=str(self.server))
        self.assertTrue(plan["valid"])
        self.assertEqual(plan["changes"], [])
        (self.client / "config" / "common.cfg").write_text("a=2\nb=1\n", encoding="utf-8")
        conflict = s.ok("sync.profile.plan", profile=profile, clientRoot=str(self.client), serverRoot=str(self.server))
        self.assertFalse(conflict["valid"])
        self.assertIn("config/common.cfg", conflict["conflicts"])

    def test_modern_forge_profile_is_detected_and_materialized(self):
        forge_root = self.root / "HMCL Forge"
        profile = forge_root / "versions" / "1.18.2"
        profile.mkdir(parents=True)
        (profile / "mods").mkdir()
        (profile / "1.18.2.json").write_text(json.dumps({
            "id": "1.18.2",
            "jar": "1.18.2",
            "mainClass": "cpw.mods.bootstraplauncher.BootstrapLauncher",
            "arguments": {"game": ["--launchTarget", "forgeclient", "--fml.forgeVersion", "40.2.9", "--fml.mcVersion", "1.18.2"], "jvm": []},
            "libraries": [{"name": "net.minecraftforge:fmlloader:1.18.2-40.2.9"}],
        }), encoding="utf-8")
        s = self.sidecar
        s.ok("instance-store.add", id="forge", name="HMCL Forge", path=str(forge_root),
             format="vanilla-compatible", enabled=True, writable=True)
        found = s.ok("vanilla-instance.list", storeId="forge")["instances"]
        self.assertEqual(len(found), 1)
        self.assertEqual(found[0]["minecraftVersion"], "1.18.2")
        self.assertEqual(found[0]["loaderType"], "forge")
        self.assertEqual(found[0]["loaderVersion"], "40.2.9")

    def test_store_transfer_and_data_location(self):
        s = self.sidecar
        source = self.prism_a / "Transfer Me"
        source.mkdir(parents=True)
        (source / "instance.cfg").write_text(
            "[General]\nInstanceType=OneSix\nname=Transfer Me\n", encoding="utf-8"
        )
        (source / "mmc-pack.json").write_text('{"formatVersion":1,"components":[]}', encoding="utf-8")
        copied = s.ok("instance.store-transfer", instance="Transfer Me", targetStoreId="prism-b", mode="copy")
        copied_path = Path(copied["destination"])
        self.assertTrue((copied_path / "instance.cfg").is_file())
        move_source = self.prism_a / "Move Me"
        move_source.mkdir(parents=True)
        (move_source / "instance.cfg").write_text(
            "[General]\nInstanceType=OneSix\nname=Move Me\n", encoding="utf-8"
        )
        (move_source / "mmc-pack.json").write_text('{"formatVersion":1,"components":[]}', encoding="utf-8")
        moved = s.ok("instance.store-transfer", instance="Move Me", targetStoreId="prism-b", mode="move")
        self.assertTrue(Path(moved["destination"]).is_dir())
        self.assertFalse(move_source.exists())

        location = s.ok("startup.data-location")
        self.assertFalse(location["portable"])
        self.assertTrue(location["portablePath"].endswith("/UserData") or location["portablePath"].endswith("\\UserData"))
        portable = next(store for store in location["instanceStores"] if store["id"] == "portable-userdata")
        self.assertFalse(portable["enabled"])
        switched = s.ok("startup.set-data-location", portable=True)
        self.assertTrue(switched["restartRequired"])
        switched_back = s.ok("startup.set-data-location", portable=False)
        self.assertTrue(switched_back["restartRequired"])


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", type=Path, required=True)
    ARGS = parser.parse_args()
    ARGS.exe = ARGS.exe.resolve()
    unittest.main(argv=[__file__], verbosity=2)
