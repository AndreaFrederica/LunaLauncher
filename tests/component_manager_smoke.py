"""Exercise NeoUI's component editor against an isolated, offline native sidecar.

Usage: pixi run python tests/component_manager_smoke.py --exe <staged CLI>
"""
import argparse
import json
from pathlib import Path
import shutil
import tempfile
import unittest
import zipfile

from api_sidecar_smoke import Sidecar


class Components(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory(prefix="luna-components-")
        cls.root = Path(cls.tmp.name)
        cls.client = cls.root / "instances/custom"
        (cls.client / "patches").mkdir(parents=True)
        (cls.client / "minecraft").mkdir()
        (cls.client / "instance.cfg").write_text("[General]\nInstanceType=OneSix\nname=Custom MITE fixture\n", encoding="utf-8")
        components = [
            {"uid": "net.minecraft", "version": "1.6.4", "important": True},
            {"uid": "com.example.pack", "version": "2"},
            {"uid": "com.example.optional", "version": "1"},
        ]
        cls.write_json(cls.client / "mmc-pack.json", {"formatVersion": 1, "components": components})
        cls.write_json(cls.client / "patches/net.minecraft.json", {
            "formatVersion": 1, "uid": "net.minecraft", "name": "Patched Minecraft", "version": "1.6.4",
            "mainClass": "net.minecraft.client.main.Main", "compatibleJavaMajors": [17],
            "mainJar": {"name": "com.example:patched-client:1", "MMC-hint": "local"},
            "libraries": [{"name": "net.xiaoyu233.fishmodloader:fishmodloader:v3.4.4"}],
        })
        for uid in ["com.example.pack", "com.example.optional"]:
            cls.write_json(cls.client / f"patches/{uid}.json", {"formatVersion": 1, "uid": uid, "name": uid, "version": "2" if uid.endswith("pack") else "1"})
        standard = cls.root / "instances/standard"
        standard.mkdir()
        (standard / "instance.cfg").write_text("[General]\nInstanceType=OneSix\nname=Standard fixture\n", encoding="utf-8")
        cls.write_json(standard / "mmc-pack.json", {"formatVersion": 1, "components": [{"uid": "net.minecraft", "version": "1.21.1", "important": True}]})
        meta = cls.root / "meta/net.minecraft"
        meta.mkdir(parents=True)
        cls.write_json(meta.parent / "index.json", {"formatVersion": 1, "packages": [{"uid": "net.minecraft", "name": "Minecraft"}]})
        cls.write_json(meta / "index.json", {"formatVersion": 1, "uid": "net.minecraft", "name": "Minecraft", "versions": [{"version": "1.21.1", "type": "release", "releaseTime": "2024-08-08T00:00:00Z", "requires": []}]})
        cls.write_json(meta / "1.21.1.json", {"formatVersion": 1, "uid": "net.minecraft", "name": "Minecraft", "version": "1.21.1", "mainClass": "net.minecraft.client.main.Main", "type": "release", "releaseTime": "2024-08-08T00:00:00Z"})
        standalone = cls.root / "instances/standalone-loader"
        shutil.copytree(cls.client, standalone)
        cls.write_json(standalone / "mmc-pack.json", {"formatVersion": 1, "components": [
            components[0], {"uid": "net.xiaoyu233.fishmodloader", "version": "v3.4.4"},
        ]})
        cls.write_json(standalone / "patches/net.xiaoyu233.fishmodloader.json", {
            "formatVersion": 1, "uid": "net.xiaoyu233.fishmodloader", "name": "FishModLoader", "version": "v3.4.4",
            "libraries": [{"name": "net.xiaoyu233.fishmodloader:fishmodloader:v3.4.4"}],
        })
        cls.sidecar = Sidecar(ARGS.exe, cls.root)
        cls.sidecar.call("initialize")

    @staticmethod
    def write_json(path, value):
        path.write_text(json.dumps(value), encoding="utf-8")

    @classmethod
    def tearDownClass(cls):
        try:
            cls.sidecar.close()
        finally:
            cls.tmp.cleanup()

    def ok(self, op, **params):
        return self.sidecar.ok(op, instance="custom", **params)

    def expect_failure(self, op, **params):
        result = self.sidecar.execute(op, instance="custom", **params)
        self.assertFalse(result.get("ok"), result)
        return result

    def test_00_instance_info_before_components_resolves_local_metadata(self):
        s = self.sidecar
        s.ok("instance.list")
        s.ok("instance.info", instance="standard")
        for _ in range(2):
            rows = s.ok("instance.components.list", instance="standard")["components"]
            self.assertEqual(len(rows), 1)
            self.assertEqual(rows[0]["problems"], [])
            self.assertEqual(rows[0]["contents"]["mainClass"], "net.minecraft.client.main.Main")
            self.assertTrue(rows[0]["canCustomize"])

    def test_01_actual_components_and_embedded_loader(self):
        result = self.ok("instance.components.list")
        self.assertFalse(result["readOnly"])
        self.assertTrue(result["canInstallCustom"])
        self.assertEqual([c["id"] for c in result["components"]], ["net.minecraft", "com.example.pack", "com.example.optional"])
        mc = result["components"][0]
        self.assertTrue(mc["custom"])
        self.assertTrue(mc["canEdit"])
        self.assertFalse(mc["canRemove"])
        self.assertEqual(mc["contents"]["embeddedLoaders"], [{"name": "FishModLoader", "version": "v3.4.4"}])
        self.assertEqual(mc["contents"]["javaVersions"], [17])
        self.assertEqual(mc["contents"]["mainJar"], "com.example:patched-client:1")

    def test_01_standalone_loader_does_not_describe_itself_as_embedded(self):
        rows = self.sidecar.ok("instance.components.list", instance="standalone-loader")["components"]
        loader = next(c for c in rows if c["id"] == "net.xiaoyu233.fishmodloader")
        self.assertEqual(loader["contents"]["embeddedLoaders"], [])
        self.assertIn("net.xiaoyu233.fishmodloader:fishmodloader:v3.4.4", loader["contents"]["libraries"])
        self.assertEqual(loader["version"], "v3.4.4")

    def test_02_edit_reload_and_protect_patch(self):
        data = self.ok("instance.component.read-json", component="com.example.pack")
        original = (self.client / "patches/com.example.pack.json").read_bytes()
        self.expect_failure("instance.component.write-json", component="com.example.pack", revision=data["revision"], content="{")
        changed = json.loads(data["content"])
        changed["uid"] = "com.example.other"
        self.expect_failure("instance.component.write-json", component="com.example.pack", revision=data["revision"], content=json.dumps(changed))
        self.assertEqual((self.client / "patches/com.example.pack.json").read_bytes(), original)
        changed.update(uid="com.example.pack", name="Edited custom component", version="3")
        self.ok("instance.component.write-json", component="com.example.pack", revision=data["revision"], content=json.dumps(changed))
        rows = self.ok("instance.components.list")["components"]
        row = next(c for c in rows if c["id"] == "com.example.pack")
        self.assertEqual(row["name"], "Edited custom component")
        self.assertEqual(row["version"], "3")
        self.expect_failure("instance.component.write-json", component="com.example.pack", revision=data["revision"], content=data["content"])
        self.assertEqual(json.loads((self.client / "patches/com.example.pack.json").read_text())["version"], "3")

    def test_03_enable_move_and_confirm_remove(self):
        self.ok("instance.component.set-enabled", component="com.example.optional", enabled=False)
        self.ok("instance.component.move", component="com.example.optional", direction="up")
        rows = self.ok("instance.components.list")["components"]
        self.assertEqual(rows[1]["id"], "com.example.optional")
        self.assertFalse(rows[1]["enabled"])
        self.expect_failure("instance.component.remove", component="com.example.optional", confirm=False)
        self.expect_failure("instance.component.remove", component="net.minecraft", confirm=True)
        self.ok("instance.component.remove", component="com.example.optional", confirm=True)
        self.assertFalse((self.client / "patches/com.example.optional.json").exists())

    def test_04_add_import_and_duplicate_validation(self):
        self.ok("instance.component.add-empty", component="com.example.empty", name="Empty patch")
        self.expect_failure("instance.component.add-empty", component="com.example.empty", name="Overwrite")
        self.expect_failure("instance.component.add-empty", component="../escape", name="Escape")
        source = self.root / "import.json"
        source.write_text("{", encoding="utf-8")
        self.expect_failure("instance.component.install-custom", source=str(source), type="component")
        self.write_json(source, {"formatVersion": 1, "uid": "../outside", "name": "Invalid", "version": "1"})
        self.expect_failure("instance.component.install-custom", source=str(source), type="component")
        self.write_json(source, {"formatVersion": 1, "uid": "com.example.import", "name": "Imported patch", "version": "4"})
        self.ok("instance.component.install-custom", source=str(source), type="component")
        self.expect_failure("instance.component.install-custom", source=str(source), type="component")
        self.assertIn("com.example.import", [c["id"] for c in self.ok("instance.components.list")["components"]])
        rows = self.ok("instance.components.reload")["components"]
        self.assertIn("com.example.import", [c["id"] for c in rows], (self.client / "mmc-pack.json").read_text())
        self.assertEqual(next(c for c in rows if c["id"] == "com.example.import")["version"], "4")

    def test_05_import_jar_mod_client_and_agent(self):
        source = self.root / "test.jar"
        with zipfile.ZipFile(source, "w") as jar:
            jar.writestr("fixture.txt", "test")
        for kind in ["jarmod", "jar", "agent"]:
            with self.subTest(kind=kind):
                self.ok("instance.component.install-custom", type=kind, source=str(source))
        rows = self.ok("instance.components.reload")["components"]
        self.assertTrue(any(c["contents"].get("jarModCount", 0) for c in rows), json.dumps(rows))
        self.assertTrue(any(c["contents"].get("agentCount", 0) for c in rows))
        self.assertTrue(any(c["id"] == "customjar" for c in rows))

    def test_06_customize_and_revert_standard_metadata(self):
        s = self.sidecar
        rows = s.ok("instance.components.list", instance="standard")["components"]
        self.assertTrue(rows[0]["canCustomize"], rows)
        s.ok("instance.component.customize", instance="standard", component="net.minecraft")
        patch = self.root / "instances/standard/patches/net.minecraft.json"
        self.assertTrue(patch.exists())
        data = s.ok("instance.component.read-json", instance="standard", component="net.minecraft")
        self.assertEqual(json.loads(data["content"])["version"], "1.21.1")
        result = s.execute("instance.component.revert", instance="standard", component="net.minecraft", confirm=False)
        self.assertFalse(result["ok"])
        s.ok("instance.component.revert", instance="standard", component="net.minecraft", confirm=True)
        self.assertFalse(patch.exists())

    def test_07_foreign_profile_is_read_only(self):
        vanilla = self.root / "vanilla/.minecraft/versions/external"
        vanilla.mkdir(parents=True)
        self.write_json(vanilla / "external.json", {"id": "external", "type": "release", "mainClass": "net.minecraft.client.main.Main", "minecraftVersion": "1.6.4", "libraries": [], "minecraftArguments": "--username ${auth_player_name}"})
        s = self.sidecar
        s.ok("instance-store.add", id="vanilla", name="Vanilla", path=str(self.root / "vanilla"), format="vanilla-compatible", enabled=True, writable=True)
        entries = s.ok("instance.list")
        instance = next(c["id"] for c in entries if c.get("format") == "vanilla-compatible")
        result = s.ok("instance.components.list", instance=instance)
        self.assertTrue(result["readOnly"])
        self.assertFalse(result["canInstallCustom"])
        self.assertTrue(result["components"])
        for c in result["components"]:
            self.assertFalse(any(c[k] for k in ["canEdit", "canRemove", "canMove", "canCustomize", "canChangeVersion", "canDisable"]))
        self.assertFalse(s.execute("instance.component.add-empty", instance=instance, component="com.example.denied", name="Denied")["ok"])

    def test_08_changes_survive_backend_restart(self):
        self.sidecar.close()
        type(self).sidecar = Sidecar(ARGS.exe, self.root)
        self.sidecar.call("initialize")
        rows = self.ok("instance.components.list")["components"]
        ids = [c["id"] for c in rows]
        self.assertIn("com.example.empty", ids)
        self.assertIn("com.example.import", ids)
        self.assertIn("customjar", ids)
        self.assertNotIn("com.example.optional", ids)
        self.assertEqual(next(c for c in rows if c["id"] == "com.example.pack")["name"], "Edited custom component")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", required=True, type=Path)
    ARGS, remaining = parser.parse_known_args()
    ARGS.exe = ARGS.exe.resolve()
    unittest.main(argv=[__file__, *remaining], verbosity=2)
