# SPDX-License-Identifier: GPL-3.0-only
"""Exercise an installed launcher sidecar with isolated data and local HTTP fixtures.

python tests/api_sidecar_smoke.py --exe install-api-test/lunalauncher-cli.exe
Add --online to smoke-test public resource providers (read-only network requests).
"""
import argparse
import base64
from collections import deque
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import hashlib
import gzip
import struct
import shutil
import os
from pathlib import Path
import queue
import subprocess
import sys
import tempfile
import threading
import time
import unittest


class Sidecar:
    def __init__(self, executable, profile):
        self.stderr = deque(maxlen=30)
        self.messages = queue.Queue()
        self.backlog = []
        self.counter = 0
        self.process = subprocess.Popen(
            [str(executable), "--dir", str(profile), "--mcp"], cwd=executable.parent,
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            env={**os.environ, "QT_QPA_PLATFORM": "offscreen"},
        )
        def read_stdout():
            for line in self.process.stdout:
                try:
                    self.messages.put(json.loads(line))
                except (ValueError, UnicodeError):
                    self.messages.put({"invalid_stdout": repr(line)})
        def read_stderr():
            for line in self.process.stderr:
                self.stderr.append(line.decode("utf-8", errors="replace"))
        for reader in (read_stdout, read_stderr):
            threading.Thread(target=reader, daemon=True).start()

    def send(self, method, params=None, *, notification=False):
        self.counter += 1
        message = {"jsonrpc": "2.0", "method": method, "params": params or {}}
        if not notification:
            message["id"] = self.counter
        self.write((json.dumps(message, ensure_ascii=False) + "\n").encode())
        return self.counter

    def write(self, data):
        self.process.stdin.write(data)
        self.process.stdin.flush()

    def receive(self, predicate, timeout=30):
        deadline = time.monotonic() + timeout
        while True:
            for i, message in enumerate(self.backlog):
                if predicate(message):
                    return self.backlog.pop(i)
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise AssertionError("Sidecar response timed out.\n" + "".join(self.stderr))
            try:
                message = self.messages.get(timeout=min(remaining, 1))
            except queue.Empty:
                if self.process.poll() is not None:
                    raise AssertionError(f"Sidecar exited: {self.process.returncode}\n" + "".join(self.stderr))
                continue
            if "invalid_stdout" in message:
                raise AssertionError(message)
            self.backlog.append(message)

    def response(self, request_id, timeout=30):
        return self.receive(lambda m: m.get("id") == request_id, timeout)

    def call(self, method, params=None):
        message = self.response(self.send(method, params))
        if "error" in message:
            raise AssertionError(message)
        return message["result"]

    def start(self, operation, **parameters):
        return self.send("launcher/execute", {"operation": operation, "parameters": parameters})

    def execute(self, operation, **parameters):
        request_id = self.start(operation, **parameters)
        try:
            message = self.response(request_id, timeout=120)
        except AssertionError:
            self.send("notifications/cancelled", {"requestId": request_id}, notification=True)
            self.response(request_id)
            raise
        if "error" in message:
            raise AssertionError(message)
        return message["result"]

    def ok(self, operation, **parameters):
        result = self.execute(operation, **parameters)
        if not result.get("ok"):
            raise AssertionError(result)
        return result["data"]

    def close(self):
        self.process.stdin.close()
        try:
            self.process.wait(timeout=15)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait()
            raise AssertionError("Sidecar failed to exit on stdin EOF")
        finally:
            self.process.stdout.close()
            self.process.stderr.close()
        if self.process.returncode != 0:
            raise AssertionError(f"Sidecar exit code: {self.process.returncode}\n" + "".join(self.stderr))


class ApiSmoke(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory(prefix="luna-api-smoke-")
        cls.root = Path(cls.tmp.name)
        client = cls.root / "instances/neo-client"
        (client / ".minecraft").mkdir(parents=True)
        (client / "instance.cfg").write_text("[General]\nInstanceType=OneSix\nname=Neo client fixture\n", encoding="utf-8")
        (client / "mmc-pack.json").write_text('{"formatVersion":1,"components":[]}', encoding="utf-8")
        meta = cls.root / "meta"
        (meta / "net.minecraft").mkdir(parents=True)
        (meta / "index.json").write_text(json.dumps({"formatVersion": 1, "packages": [
            {"uid": "net.minecraft", "name": "Minecraft"}]}), encoding="utf-8")
        (meta / "net.minecraft/index.json").write_text(json.dumps({"formatVersion": 1,
            "uid": "net.minecraft", "name": "Minecraft", "versions": [
                {"version": "1.21.1", "type": "release", "recommended": True,
                 "releaseTime": "2024-08-08T00:00:00Z", "requires": []}]}), encoding="utf-8")
        cls.sidecar = Sidecar(ARGS.exe, cls.root)
        cls.slow_started = threading.Event()
        cls.release_http = threading.Event()
        class Handler(BaseHTTPRequestHandler):
            def log_message(self, *args):
                pass
            def do_GET(self):
                cls.slow_started.set()
                cls.release_http.wait(30)
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                try:
                    self.wfile.write(b'{}')
                except (ConnectionError, OSError):
                    pass
        cls.http = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        cls.http.daemon_threads = True
        threading.Thread(target=cls.http.serve_forever, daemon=True).start()

    @classmethod
    def tearDownClass(cls):
        cls.release_http.set()
        cls.http.shutdown()
        cls.http.server_close()
        try:
            cls.sidecar.close()
        finally:
            cls.tmp.cleanup()

    def test_01_catalog_and_mcp_compatibility(self):
        s = self.sidecar
        self.assertEqual(s.call("initialize")["serverInfo"]["name"], "lunalauncher")
        catalog = s.call("launcher/catalog")
        names = {op["name"] for op in catalog["operations"]}
        for required in ("resource.search", "resource.install-version", "component.versions", "server.yaml.write",
                         "integration.host", "aria2.downloads", "account.login", "task.cancel"):
            self.assertIn(required, names)
        tools = s.call("tools/list")["tools"]
        self.assertEqual(len(tools), len(names))
        result = s.call("tools/call", {"name": "lunalauncher_resource_providers", "arguments": {}})
        self.assertFalse(result["isError"])
        providers = result["structuredContent"]["data"]
        ids = {p["id"] for p in providers}
        self.assertTrue({"modrinth", "curseforge", "hangar"}.issubset(ids))
        self.assertEqual(len(ids), len(providers))
        self.assertTrue(all(p in {"modrinth", "curseforge", "hangar"} or p.startswith("js:") for p in ids))
        self.assertFalse(s.execute("operation.does-not-exist")["ok"])
        print(f"\nCatalog: {len(names)} operations", flush=True)

    def test_02_partial_utf8_multiple_frames_and_recovery(self):
        s = self.sidecar
        # Split a multi-byte Chinese UTF-8 character across writes.
        request = {"jsonrpc": "2.0", "id": "fragment", "method": "launcher/execute",
                   "params": {"operation": "account.login", "parameters": {"type": "offline", "username": "测试玩家"}}}
        payload = (json.dumps(request, ensure_ascii=False) + "\n").encode()
        split = payload.index("测".encode()) + 1
        s.write(payload[:split])
        # This incomplete request must not starve the Qt loop or produce a parse error.
        time.sleep(0.1)
        s.write(payload[split:] + b'{"jsonrpc":"2.0","id":"batched","method":"ping"}\n')
        self.assertTrue(s.response("fragment")["result"]["ok"])
        self.assertEqual(s.response("batched")["result"], {})
        s.write(b'not-json\n')
        self.assertEqual(s.receive(lambda m: m.get("error", {}).get("code") == -32700)["error"]["code"], -32700)
        self.assertEqual(s.call("ping"), {})
        s.write(b'x' * (4 * 1024 * 1024 + 1) + b'\n')
        self.assertEqual(s.receive(lambda m: m.get("error", {}).get("code") == -32600)["error"]["code"], -32600)
        self.assertEqual(s.call("ping"), {})

    def test_03_interaction_reply_busy_and_scoped_cancel(self):
        s = self.sidecar
        request_id = s.start("account.login", type="offline")
        event = s.receive(lambda m: m.get("params", {}).get("requestId") == request_id and m["params"].get("kind") == "input")["params"]
        self.assertFalse(event["secret"])
        self.assertEqual(s.ok("task.list", runningOnly=True), [])
        busy = s.response(s.start("settings.set", scope="launcher", key="Language", value="en_US"))
        self.assertEqual(busy["error"]["code"], -32000)
        s.send("notifications/cancelled", {"requestId": "unrelated"}, notification=True)
        wrong = s.response(s.send("launcher/respond", {"interactionId": "stale", "value": "Ignored"}))
        self.assertEqual(wrong["error"]["code"], -32602)
        wrong_type = s.response(s.send("launcher/respond", {"interactionId": event["interactionId"], "value": 17}))
        self.assertEqual(wrong_type["error"]["code"], -32602)
        self.assertTrue(s.call("launcher/respond", {"interactionId": event["interactionId"], "value": "NeoSmoke"})["accepted"])
        self.assertEqual(s.response(request_id)["result"]["data"]["profileName"], "NeoSmoke")
        request_id = s.start("account.login", type="offline")
        s.receive(lambda m: m.get("params", {}).get("requestId") == request_id and m["params"].get("kind") == "input")
        s.send("notifications/cancelled", {"requestId": request_id}, notification=True)
        self.assertFalse(s.response(request_id)["result"]["ok"])
        self.assertEqual(s.call("ping"), {})

    def test_04_server_configuration_and_file_boundaries(self):
        s = self.sidecar
        instance = s.ok("instance.create", type="server", name="Neo API 测试", executable="java", arguments=["-jar", "server.jar"])
        ref = instance["id"]
        original = s.ok("server.yaml.read", instance=ref, file="bukkit.yml")
        content = "settings:\n  motd: Neo 测试\n"
        updated = s.ok("server.yaml.write", instance=ref, file="bukkit.yml", content=content, ifRevision=original["revision"])
        self.assertEqual(s.ok("server.yaml.read", instance=ref, file="bukkit.yml")["content"], content)
        self.assertNotEqual(original["revision"], updated["revision"])
        self.assertFalse(s.execute("server.yaml.write", instance=ref, file="bukkit.yml", content="stale", ifRevision=original["revision"])["ok"])
        self.assertFalse(s.execute("server.yaml.read", instance=ref, file="../instance.cfg")["ok"])
        self.assertFalse(s.execute("server.yaml.write", instance=ref, file="bukkit.yml", content=5)["ok"])
        config = s.ok("server.loader.write", instance=ref, minecraftVersion="1.21.1", loaders=["fabric"], pluginLoaders=["paper"])
        self.assertEqual(config["loaders"], ["fabric"])
        self.assertEqual(s.ok("server.loader.read", instance=ref)["pluginLoaders"], ["paper"])
        self.assertFalse(s.execute("server.loader.write", instance=ref, loaders=["unknown"])["ok"])
        self.assertEqual(s.ok("server.loader.read", instance=ref)["loaders"], ["fabric"])
        root = Path(instance["root"])
        if not root.is_absolute():
            root = self.root / root
        (root / "logs").mkdir(exist_ok=True)
        (root / "logs/latest.log").write_text("Neo 日志\n", encoding="utf-8")
        self.assertIn("Neo 日志", s.ok("instance.log.read", instance=ref, file="latest.log")["content"])
        self.assertFalse(s.execute("instance.log.read", instance=ref, file=str(self.root / "meta/index.json"))["ok"])

    def test_05_metadata_offline_and_validation(self):
        s = self.sidecar
        self.assertTrue(any(c["uid"] == "net.minecraft" for c in s.ok("component.catalog", offline=True)))
        versions = s.ok("component.versions", uid="net.minecraft", offline=True)
        self.assertEqual(versions["versions"][0]["version"], "1.21.1")
        self.assertFalse(s.execute("component.versions", uid="missing.component", offline=True)["ok"])
        self.assertFalse(s.execute("component.versions", uid="../outside", offline=True)["ok"])
        self.assertFalse(s.execute("resource.search", provider="unknown", kind="mods")["ok"])
        self.assertFalse(s.execute("resource.search", provider="hangar", kind="mods")["ok"])
        self.assertFalse(s.execute("resource.search", provider="modrinth", kind="mods", loaders=["typo"])["ok"])
        self.assertFalse(s.execute("resource.search", provider="modrinth", kind="mods", offset=-1)["ok"])

    def test_06_long_task_controls_and_cancellation(self):
        s = self.sidecar
        logs = self.root / "instances/neo-client/.minecraft/logs"
        logs.mkdir(exist_ok=True)
        log = logs / "during-download.log"
        log.write_bytes(b"")
        subscription = s.ok("instance.log.subscribe", instance="neo-client", file=str(log))["subscriptionId"]
        url = f"http://127.0.0.1:{self.http.server_port}/"
        s.ok("settings.set", scope="launcher", key="MetaURLOverride", value=url)
        request_id = s.start("component.versions", uid="neo.slow")
        self.assertTrue(self.slow_started.wait(10), "The local metadata request did not start")
        # A progress event and both transports remain usable during the download.
        event = s.receive(lambda m: m.get("params", {}).get("requestId") == request_id and m["params"].get("kind") == "task")["params"]
        task_id = event["data"]["id"]
        status = s.ok("task.status", taskId=task_id)
        self.assertTrue(status["running"])
        self.assertTrue(status["canAbort"])
        log.write_bytes(b"during download\n")
        s.receive(lambda m: m.get("method") == "launcher/stream" and m["params"]["subscriptionId"] == subscription)
        self.assertTrue(s.ok("event.poll", subscriptionId=subscription)["events"])
        self.assertTrue(s.ok("event.unsubscribe", subscriptionId=subscription)["removed"])
        mcp = s.call("tools/call", {"name": "lunalauncher_task_list", "arguments": {"runningOnly": True}})
        self.assertFalse(mcp["isError"])
        self.assertTrue(mcp["structuredContent"]["data"])
        s.send("notifications/cancelled", {"requestId": "wrong-download"}, notification=True)
        self.assertTrue(s.ok("task.status", taskId=task_id)["running"])
        cancel = s.call("tools/call", {"name": "lunalauncher_task_cancel", "arguments": {"taskId": task_id}})
        self.assertFalse(cancel["isError"])
        self.assertFalse(s.response(request_id)["result"]["ok"])
        self.assertFalse(s.ok("task.status", taskId=task_id)["running"])
        self.assertEqual(s.call("ping"), {})
        s.ok("settings.reset", scope="launcher", key="MetaURLOverride")

    def test_07_optional_integration_status(self):
        s = self.sidecar
        statuses = s.ok("integration.status")
        self.assertEqual({entry["id"] for entry in statuses}, {"aria2", "terracotta", "yukari"})
        self.assertIsInstance(s.ok("aria2.downloads"), list)
        self.assertFalse(s.execute("integration.start", integration="unknown")["ok"])
        self.assertFalse(s.execute("aria2.remove", confirm=False)["ok"])

    def test_08_public_resource_catalog(self):
        if not ARGS.online:
            self.skipTest("enable with --online")
        s = self.sidecar
        search = s.ok("resource.search", provider="modrinth", kind="mods", query="sodium", minecraftVersion="1.21.1", loaders=["fabric"])
        self.assertTrue(search["projects"])
        project = search["projects"][0]["projectId"]
        info = s.ok("resource.project", provider="modrinth", kind="mods", projectId=project)
        self.assertTrue(info["name"])
        versions = s.ok("resource.versions", provider="modrinth", kind="mods", projectId=project, minecraftVersion="1.21.1", loaders=["fabric"])
        self.assertTrue(versions)
        releases = sorted((v for v in versions if v["type"] == "release"), key=lambda v: v["date"])
        self.assertGreater(len(releases), 1)
        installed = s.ok("resource.install-version", provider="modrinth", kind="mods", projectId=project,
            versionId=releases[0]["versionId"], minecraftVersion="1.21.1", loaders=["fabric"], instance="neo-client")
        self.assertTrue(Path(installed["path"]).is_file())
        self.assertGreater(Path(installed["path"]).stat().st_size, 0)
        self.assertEqual(hashlib.new(installed["hashType"], Path(installed["path"]).read_bytes()).hexdigest(), installed["hash"])
        s.ok("resource.disable", instance="neo-client", kind="mods", resource=installed["fileName"])
        plan = s.ok("resource.updates.check", instance="neo-client", kind="mods", minecraftVersion="1.21.1", loaders=["fabric"])
        self.assertEqual(len(plan["updates"]), 1, plan)
        item = plan["updates"][0]
        self.assertFalse(item["enabled"])
        self.assertEqual(item["target"]["versionId"], releases[-1]["versionId"])
        selection = dict(planId=plan["planId"], items=[item["itemId"]])
        self.assertFalse(s.execute("resource.updates.apply", planId=plan["planId"], items=["unknown"])["ok"])
        self.assertFalse(s.execute("resource.updates.apply", planId=plan["planId"], items=[item["itemId"]] * 2)["ok"])
        old_path = Path(installed["path"] + ".disabled")
        old_bytes = old_path.read_bytes()
        old_path.write_bytes(old_bytes + b"stale")
        self.assertFalse(s.execute("resource.updates.apply", **selection)["ok"])
        self.assertTrue(old_path.read_bytes().endswith(b"stale"))
        old_path.write_bytes(old_bytes)
        index = next((old_path.parent / ".index").glob("*.pw.toml"))
        old_index = index.read_bytes()
        index.write_bytes(old_index + b"\n# changed externally\n")
        self.assertFalse(s.execute("resource.updates.apply", **selection)["ok"])
        index.write_bytes(old_index)
        collision = old_path.parent / item["target"]["fileName"]
        collision.write_bytes(b"unrelated resource")
        self.assertFalse(s.execute("resource.updates.apply", **selection)["ok"])
        self.assertEqual(collision.read_bytes(), b"unrelated resource")
        collision.unlink()
        applied = s.ok("resource.updates.apply", **selection)
        self.assertTrue(applied["complete"], applied)
        self.assertEqual(applied["results"][0]["status"], "updated")
        self.assertFalse(old_path.exists())
        updated = Path(applied["results"][0]["path"])
        self.assertTrue(updated.name.endswith(".disabled"))
        self.assertEqual(hashlib.new(item["target"]["hashType"], updated.read_bytes()).hexdigest(), item["target"]["hash"])
        self.assertFalse(s.execute("resource.updates.apply", **selection)["ok"])
        fresh = s.ok("resource.updates.check", instance="neo-client", kind="mods", minecraftVersion="1.21.1", loaders=["fabric"])
        self.assertEqual(fresh["updates"], [], fresh)
        self.assertTrue(s.ok("resource.updates.discard", planId=fresh["planId"])["removed"])
        dependency = s.ok("resource.resolve-dependency", provider="modrinth", kind="mods", projectId="P7dR8mSH", minecraftVersion="1.21.1")
        self.assertTrue(dependency["versionId"])
        plugins = s.ok("resource.search", provider="hangar", kind="plugins", query="ViaVersion")["projects"]
        self.assertTrue(plugins)
        plugin_id = plugins[0]["projectId"]
        self.assertTrue(s.ok("resource.project", provider="hangar", kind="plugins", projectId=plugin_id)["name"])
        self.assertTrue(s.ok("resource.versions", provider="hangar", kind="plugins", projectId=plugin_id))

    def test_09_installed_cli_and_eof_during_interaction(self):
        with tempfile.TemporaryDirectory(prefix="luna-api-cli-") as profile:
            result = subprocess.run([str(ARGS.exe), "--dir", profile, "--cli", "--json", "api", "api.describe"],
                cwd=ARGS.exe.parent, capture_output=True, timeout=30,
                env={**os.environ, "QT_QPA_PLATFORM": "offscreen"})
            self.assertEqual(result.returncode, 0, result.stderr.decode("utf-8", errors="replace"))
            catalog = json.loads(result.stdout)
            self.assertTrue(catalog["ok"])
            self.assertTrue(any(op["name"] == "resource.search" for op in catalog["data"]))
        with tempfile.TemporaryDirectory(prefix="luna-api-eof-") as profile:
            sidecar = Sidecar(ARGS.exe, Path(profile))
            try:
                request = sidecar.start("account.login", type="offline")
                sidecar.receive(lambda m: m.get("params", {}).get("requestId") == request and m["params"].get("kind") == "input")
            finally:
                sidecar.close()

    def test_10_appearance_and_language(self):
        s = self.sidecar
        exported = s.ok("settings.export", scope="launcher")
        self.assertIn("values", exported)
        self.assertGreater(exported["count"], 0)
        self.assertEqual(s.ok("settings.import", scope="launcher", values={"Language": "en_US"})["imported"], 1)
        self.assertFalse(s.execute("settings.import", scope="launcher", values={"MetaURLOverride": {"bad": True}})["ok"])
        self.assertEqual(s.ok("settings.import", scope="launcher", values={"NoSuchSetting": "x"})["skippedUnknown"], 1)
        catalog = s.ok("appearance.catalog")
        self.assertTrue(catalog["themes"])
        self.assertTrue(catalog["icons"])
        selected = catalog["selected"]
        self.assertFalse(s.execute("appearance.select", theme=catalog["themes"][0]["id"], icons="nonexistent")["ok"])
        self.assertEqual(s.ok("appearance.catalog")["selected"], selected)
        theme = catalog["themes"][0]["id"]
        self.assertEqual(s.ok("appearance.select", theme=theme)["theme"], theme)
        self.assertEqual(s.ok("appearance.refresh")["selected"]["theme"], theme)
        languages = s.ok("language.list")
        self.assertTrue(any(entry["id"] == "en_US" for entry in languages["languages"]))
        self.assertFalse(s.execute("language.select", language="unknown", useSystemLocale=True)["ok"])
        self.assertEqual(s.ok("language.list")["useSystemLocale"], languages["useSystemLocale"])
        self.assertFalse(s.ok("language.select", language="en_US", useSystemLocale=False)["updateRequested"])
        self.assertTrue(s.ok("language.refresh", language="en_US")["builtin"])

    def test_11_server_resources_and_console(self):
        s = self.sidecar
        script = self.root / "terminal.py"
        script.write_text('import sys\nprint("NEO_READY", flush=True)\n'
            'for line in sys.stdin:\n print("NEO_REPLY:" + line.strip(), flush=True)\n', encoding="utf-8")
        server = s.ok("instance.create", type="server", name="PTY fixture", executable=sys.executable, arguments=["-u", str(script)])
        ref = server["id"]
        resource = self.root / "fixture.jar"
        resource.write_bytes(b"fixture resource")
        for kind in ("mods", "plugins"):
            self.assertEqual(s.ok("resource.list", instance=ref, kind=kind), [])
            s.ok("resource.install", instance=ref, kind=kind, source=str(resource))
            self.assertEqual(s.ok("resource.list", instance=ref, kind=kind)[0]["fileName"], "fixture.jar")
            s.ok("resource.disable", instance=ref, kind=kind, resource="fixture.jar")
            self.assertFalse(s.ok("resource.inspect", instance=ref, kind=kind, resource="fixture.jar.disabled")["enabled"])
            s.ok("resource.enable", instance=ref, kind=kind, resource="fixture.jar.disabled")
            s.ok("resource.remove", instance=ref, kind=kind, resource="fixture.jar", confirm=True)
            self.assertEqual(s.ok("resource.list", instance=ref, kind=kind), [])
        sub = s.ok("instance.console.subscribe", instance=ref)["subscriptionId"]
        cursor, output = 0, b""
        def await_output(marker):
            nonlocal cursor, output
            deadline = time.monotonic() + 15
            while time.monotonic() < deadline:
                batch = s.ok("event.poll", subscriptionId=sub, after=cursor)
                cursor = batch["nextCursor"]
                for event in batch["events"]:
                    if event["kind"] == "console.data":
                        output += base64.b64decode(event["data"])
                if marker in output:
                    return
                time.sleep(0.1)
            self.fail(f"Console output missing {marker!r}: {output!r}")
        try:
            self.assertTrue(s.ok("server.start", instance=ref)["running"])
            await_output(b"NEO_READY")
            s.ok("server.console.resize", instance=ref, columns=90, rows=24)
            self.assertFalse(s.execute("server.console.resize", instance=ref, columns=0, rows=24)["ok"])
            self.assertEqual(s.ok("server.console.write", instance=ref, text="roundtrip\r\n")["bytesWritten"], 11)
            await_output(b"NEO_REPLY:roundtrip")
            self.assertFalse(s.execute("resource.install", instance=ref, kind="plugins", source=str(resource))["ok"])
            s.ok("instance.kill", instance=ref)
            output = b""
            self.assertTrue(s.ok("server.start", instance=ref)["running"])
            await_output(b"NEO_READY")
        finally:
            s.ok("instance.kill", instance=ref)
            self.assertTrue(s.ok("event.unsubscribe", subscriptionId=sub)["removed"])
        self.assertFalse(s.execute("server.console.write", instance=ref, text="test")["ok"])

    def test_12_log_streams_polling_and_overflow(self):
        s = self.sidecar
        logs = self.root / "instances/neo-client/.minecraft/logs"
        logs.mkdir(exist_ok=True)
        log = logs / "stream.log"
        payload = ("Neo 日志🙂\n" * 5000).encode()
        log.write_bytes(payload)
        self.assertFalse(s.execute("instance.log.subscribe", instance="neo-client", file=str(self.root / "terminal.py"))["ok"])
        sub = s.ok("instance.log.subscribe", instance="neo-client", file=str(log), tailBytes=len(payload))["subscriptionId"]
        cursor, data = 0, b""
        try:
            while len(data) < len(payload):
                batch = s.receive(lambda m: m.get("method") == "launcher/stream" and m["params"]["subscriptionId"] == sub)["params"]
                cursor = batch["nextCursor"]
                for event in batch["events"]:
                    if event["kind"] == "log.data":
                        data += base64.b64decode(event["data"])
            self.assertEqual(data, payload)
            # Poll retains history independently of notification delivery.
            history = s.ok("event.poll", subscriptionId=sub, after=0, limit=1)
            self.assertEqual(len(history["events"]), 1)
            self.assertTrue(history["hasMore"])
            self.assertFalse(s.execute("event.poll", subscriptionId=sub, after=cursor + 100)["ok"])
            log.write_bytes(b"rotated\n")
            batch = s.receive(lambda m: m.get("method") == "launcher/stream" and m["params"]["subscriptionId"] == sub and
                any(e["kind"] == "log.reset" for e in m["params"]["events"]))["params"]
            self.assertTrue(any(e.get("data") == base64.b64encode(b"rotated\n").decode() for e in batch["events"]))
            # More than one MiB of event data evicts history with an explicit gap.
            with log.open("ab") as handle:
                handle.write(b"x" * (2 * 1024 * 1024))
            deadline = time.monotonic() + 15
            while time.monotonic() < deadline:
                history = s.ok("event.poll", subscriptionId=sub, after=0)
                if history["dropped"] > 0:
                    break
                time.sleep(0.2)
            self.assertGreater(history["dropped"], 0)
        finally:
            self.assertTrue(s.ok("event.unsubscribe", subscriptionId=sub)["removed"])
        self.assertFalse(s.execute("event.poll", subscriptionId=sub)["ok"])
        self.assertFalse(s.ok("event.unsubscribe", subscriptionId=sub)["removed"])

    def test_13_update_plan_validation(self):
        s = self.sidecar
        self.assertFalse(s.execute("resource.updates.check", instance="missing", kind="mods")["ok"])
        self.assertFalse(s.execute("resource.updates.check", instance="neo-client", kind="mods", minecraftVersion="1.21.1", releaseTypes=["unknown"])["ok"])
        self.assertFalse(s.execute("resource.updates.apply", planId="missing", items=[])["ok"])
        self.assertFalse(s.ok("resource.updates.discard", planId="missing")["removed"])


    def test_14_accounts_and_batch(self):
        s = self.sidecar
        sub = s.ok("account.subscribe")["subscriptionId"]
        try:
            initial = s.ok("event.poll", subscriptionId=sub)
            self.assertEqual(initial["events"][0]["kind"], "account.snapshot")
            self.assertIn("defaultAccount", initial["events"][0]["data"])
            s.ok("account.login", type="offline", username="NeoEvents")
            current = s.ok("account.snapshot")
            account = next(a for a in current["accounts"] if a["name"] == "NeoEvents")
            self.assertIn("skin", account)
            self.assertIn("lastError", account)
            changes = s.ok("event.poll", subscriptionId=sub, after=initial["nextCursor"])
            self.assertTrue(any(any(a["name"] == "NeoEvents" for a in e["data"]["accounts"]) for e in changes["events"]))
        finally:
            s.ok("event.unsubscribe", subscriptionId=sub)
        operations = [{"operation": "runtime.info"}, {"operation": "instance.info", "parameters": {"instance": "missing"}}, {"operation": "account.snapshot"}]
        stopped = s.ok("api.batch", operations=operations)
        self.assertEqual(stopped["executed"], 2)
        self.assertFalse(stopped["complete"])
        continued = s.ok("api.batch", operations=operations, stopOnError=False)
        self.assertEqual(continued["executed"], 3)
        self.assertFalse(s.execute("api.batch", operations=[{"operation": "api.batch"}])["ok"])

    def test_15_world_copy_preserves_source(self):
        s = self.sidecar
        world = self.root / "instances/neo-client/.minecraft/saves/Original"
        world.mkdir(parents=True, exist_ok=True)
        def string(value):
            data = value.encode()
            return struct.pack(">H", len(data)) + data
        nbt = b"\x0a\x00\x00\x0a" + string("Data") + b"\x08" + string("LevelName") + string("Original") + b"\x00\x00"
        (world / "level.dat").write_bytes(gzip.compress(nbt))
        (world / "sentinel.txt").write_text("preserve me")
        self.assertFalse(s.execute("instance.world.copy", instance="neo-client", world="Original", name="Original", replace=True)["ok"])
        self.assertEqual((world / "sentinel.txt").read_text(), "preserve me")
        copy = s.ok("instance.world.copy", instance="neo-client", world="Original", name="Copied")
        self.assertEqual((Path(copy["path"]) / "sentinel.txt").read_text(), "preserve me")
        listed = s.ok("instance.world.list", instance="neo-client")
        self.assertTrue(any(w["name"] == "Copied" for w in listed))
        self.assertFalse(s.execute("instance.world.copy", instance="neo-client", world="Original", name="CON", replace=True)["ok"])
        s.ok("instance.world.copy", instance="neo-client", world="Original", name="Copied", replace=True)
        self.assertTrue((world / "level.dat").exists())

    def test_16_java_probe(self):
        s = self.sidecar
        invalid = s.ok("java.diagnose", path=str(self.root / "missing-java.exe"))
        self.assertFalse(invalid["usable"])
        self.assertTrue(invalid["recommendations"])
        java = shutil.which("java")
        if java:
            result = s.ok("java.diagnose", path=java)
            self.assertTrue(result["usable"], result)
            self.assertGreater(result["major"], 0)
            self.assertTrue(result["vendor"])
            self.assertTrue(result["architecture"])

    def test_17_instance_files_and_state(self):
        s = self.sidecar
        created = s.ok("instance.file.write", instance="neo-client", path="lunaui/api-test.txt", content="first")
        self.assertFalse(s.execute("instance.file.write", instance="neo-client", path="lunaui/api-test.txt", content="lost update")["ok"])
        read = s.ok("instance.file.read", instance="neo-client", path="lunaui/api-test.txt")
        self.assertEqual(read["content"], "first")
        s.ok("instance.file.write", instance="neo-client", path="lunaui/api-test.txt", content="second", revision=created["revision"])
        for path in ("../outside", "lunaui/../../outside", "C:/outside", "lunaui:stream"):
            self.assertFalse(s.execute("instance.file.write", instance="neo-client", path=path, content="blocked")["ok"])
        self.assertFalse(s.execute("instance.file.remove", instance="neo-client", path=".", confirm=True)["ok"])
        state = s.ok("instance.custom-ui.state", instance="neo-client")
        s.ok("instance.custom-ui.save-state", instance="neo-client", state={"theme": "neo"}, revision=state["revision"])
        self.assertEqual(s.ok("instance.custom-ui.state", instance="neo-client")["state"], {"theme": "neo"})
        self.assertFalse(s.execute("instance.custom-ui.save-state", instance="neo-client", state={}, revision=state["revision"])["ok"])

    def test_20_exports_and_helpers(self):
        import zipfile
        s = self.sidecar
        for fmt, index in (("zip", "instance.cfg"), ("modrinth", "modrinth.index.json"), ("curseforge", "manifest.json")):
            output = self.root / ("export-" + fmt + ".zip")
            s.ok("instance.export", instance="neo-client", output=str(output), format=fmt, version="1.0", exclude=[".minecraft/mods"])
            with zipfile.ZipFile(output) as archive:
                self.assertIn(index, archive.namelist())
                if fmt != "zip":
                    manifest = json.loads(archive.read(index))
                    self.assertEqual(manifest["versionId" if fmt == "modrinth" else "version"], "1.0")
            before = output.read_bytes()
            self.assertFalse(s.execute("instance.export", instance="neo-client", output=str(output), format=fmt, version="2")["ok"])
            self.assertEqual(output.read_bytes(), before)
        inside = self.root / "instances/neo-client/recursive.zip"
        self.assertFalse(s.execute("instance.export", instance="neo-client", output=str(inside))["ok"])
        self.assertFalse(inside.exists())
        for fmt in ("text", "html", "markdown", "json", "csv", "custom"):
            result = s.ok("resource.export-list", instance="neo-client", format=fmt, template="{name}")
            self.assertIn("content", result)
        self.assertFalse(s.ok("instance.managed-pack.info", instance="neo-client")["managed"])
        self.assertFalse(s.execute("instance.managed-pack.update", instance="neo-client", source="missing", confirm=True)["ok"])
        self.assertEqual(len(s.ok("java.catalog")), 4)
        self.assertFalse(s.execute("java.versions", uid="../escape", version="21", offline=True)["ok"])
        for helper in ("authlib-injector", "nide8auth"):
            status = s.ok("authentication.helper.status", helper=helper)
            self.assertIn("installed", status)
            self.assertFalse(s.execute("authentication.helper.remove", helper=helper)["ok"])
        self.assertFalse(s.ok("external-tool.check", tool="mcedit", path="Z:/missing/tool")["valid"])
        s.ok("account.preset.add", name="API fixture", authUrl="https://example.invalid/auth", sessionUrl="https://example.invalid/session", tokenType="OAuth")
        self.assertTrue(any(p["name"] == "API fixture" and p["tokenType"] == "OAuth" for p in s.ok("account.preset.list")))
        s.ok("account.preset.remove", name="API fixture", confirm=True)
        self.assertFalse(s.execute("settings.import-prism", confirm=False)["ok"])

    def test_21_world_datapacks_themes_and_compressed_logs(self):
        import zipfile
        s = self.sidecar
        game = self.root / "instances/neo-client/.minecraft"
        world = game / "saves/API world"
        world.mkdir(parents=True, exist_ok=True)
        pack = self.root / "fixture-datapack.zip"
        with zipfile.ZipFile(pack, "w") as archive:
            archive.writestr("pack.mcmeta", '{"pack":{"pack_format":48,"description":"API fixture"}}')
        args = dict(instance="neo-client", kind="datapacks", world="API world")
        s.ok("resource.install", **args, source=str(pack))
        self.assertTrue((world / "datapacks/fixture-datapack.zip").is_file())
        self.assertEqual(len(s.ok("resource.list", **args)), 1)
        s.ok("resource.disable", **args, resource=pack.name)
        self.assertFalse(s.ok("resource.list", **args)[0]["enabled"])
        s.ok("resource.enable", **args, resource=pack.name)
        s.ok("resource.remove", **args, resource=pack.name, confirm=True)
        self.assertEqual(s.ok("resource.list", **args), [])
        self.assertFalse(s.execute("resource.list", **{**args, "world": "../escape"})["ok"])
        self.assertFalse(s.execute("resource.list", **{**args, "kind": "mods"})["ok"])
        theme = self.root / "fixture-icons.zip"
        with zipfile.ZipFile(theme, "w") as archive:
            archive.writestr("icons/api-fixture/index.theme", "[Icon Theme]\nName=API fixture\nInherits=hicolor\nDirectories=\n")
        digest = "sha256:" + hashlib.sha256(theme.read_bytes()).hexdigest()
        self.assertEqual(s.ok("appearance.install", kind="icons", path=str(theme), digest=digest)["installed"], ["api-fixture"])
        self.assertFalse(s.execute("appearance.install", kind="icons", path=str(theme))["ok"])
        s.ok("appearance.install", kind="icons", path=str(theme), overwrite=True)
        self.assertFalse(s.execute("appearance.install", kind="icons", path=str(theme), digest="sha256:" + "0" * 64, overwrite=True)["ok"])
        with zipfile.ZipFile(theme, "w") as archive:
            archive.writestr("icons/../../escaped.txt", "bad")
        self.assertFalse(s.execute("appearance.install", kind="icons", path=str(theme), overwrite=True)["ok"])
        s.ok("appearance.remove", kind="icons", id="api-fixture", confirm=True)
        log = game / "logs/fixture.log.gz"
        log.parent.mkdir(exist_ok=True)
        log.write_bytes(gzip.compress("压缩日志\n".encode() * 100))
        self.assertEqual(s.ok("instance.log.read", instance="neo-client", file=str(log))["content"], "压缩日志\n" * 100)
        self.assertTrue(s.ok("instance.log.read", instance="neo-client", file=str(log), maxBytes=10)["truncated"])
        arbitrary = game / "options.txt"
        arbitrary.write_text("must survive")
        self.assertFalse(s.execute("instance.log.delete", instance="neo-client", file=str(arbitrary), confirm=True)["ok"])
        self.assertEqual(arbitrary.read_text(), "must survive")
        s.ok("account.preset.add", name="API edit", authUrl="https://example.invalid/a", sessionUrl="https://example.invalid/s")
        s.ok("account.preset.edit", originalName="API edit", name="API edited", authUrl="https://example.invalid/b", sessionUrl="https://example.invalid/s")
        self.assertTrue(any(p["name"] == "API edited" and p["authUrl"].endswith("/b") for p in s.ok("account.preset.list")))
        s.ok("account.preset.remove", name="API edited", confirm=True)

    def test_22_pack_catalogs_and_ftb_migration(self):
        s = self.sidecar
        cache = self.root / "cache/api-packs"
        cache.mkdir(parents=True, exist_ok=True)
        def response(url, value):
            (cache / hashlib.sha256(url.encode()).hexdigest()).write_bytes(value if isinstance(value, bytes) else json.dumps(value).encode())
        response("https://download.nodecdn.net/containers/atl/launcher/json/packsnew.json", [
            {"id": 42, "position": 1, "name": "API Pack", "type": "public", "versions": [{"version": "1.0", "minecraft": "1.21.1"}]}])
        packs = s.ok("modpack.search", provider="atlauncher", query="API", offline=True)
        self.assertEqual(len(packs), 1)
        self.assertEqual(s.ok("modpack.versions", provider="atlauncher", projectId=packs[0]["projectId"], offline=True)[0]["versionId"], "1.0")
        base = "https://api.feed-the-beast.com/v1/modpacks/public"
        response(base + "/modpack/all", {"packs": [42]})
        response(base + "/modpack/42", {"id": 42, "name": "FTB Fixture", "synopsis": "fixture", "description": "fixture", "type": "release",
            "featured": False, "installs": 0, "plays": 0, "updated": 0, "art": [], "authors": [], "tags": [],
            "versions": [{"id": 99, "name": "1.0", "type": "release", "updated": 0, "specs": {"id": 1, "minimum": 1024, "recommended": 2048}}]})
        self.assertEqual(len(s.ok("modpack.search", provider="ftb", offline=True)), 1)
        self.assertEqual(s.ok("modpack.versions", provider="ftb", projectId="42", offline=True)[0]["versionId"], "99")
        response("https://api.technicpack.net/modpack/fixture?build=multimc", {"name": "fixture", "version": "1.0", "minecraft": "1.21.1", "url": "https://example.invalid/pack.zip"})
        self.assertEqual(s.ok("modpack.versions", provider="technic", projectId="fixture", offline=True)[0]["versionId"], "1.0")
        for name in ("modpacks", "thirdparty"):
            response("https://dist.creeper.host/FTB2/static/" + name + ".xml", b'<modpacks><modpack name="Legacy Fixture" dir="fixture" version="1.0" mcVersion="1.7.10" oldVersions="1.0;0.9" url="fixture.zip"/></modpacks>' if name == "modpacks" else b'<modpacks/>')
        self.assertEqual(len(s.ok("modpack.search", provider="legacy-ftb", offline=True)), 1)
        self.assertEqual(len(s.ok("modpack.versions", provider="legacy-ftb", projectId="fixture", offline=True)), 2)
        self.assertFalse(s.execute("modpack.install", provider="ftb", projectId="42", versionId="99", offline=True)["ok"])
        s.ok("modpack.legacy-ftb.private-codes", codes=["fixture"])
        self.assertEqual(s.ok("modpack.legacy-ftb.private-codes"), ["fixture"])
        source = self.root / "ftb-app/fixture"
        source.mkdir(parents=True)
        (source / "instance.json").write_text(json.dumps({"uuid": "fixture", "id": 42, "versionId": 99, "name": "FTB local fixture", "version": "1.0",
            "mcVersion": "1.21.1", "totalPlayTime": 120000, "modLoader": "", "jvmArgs": "-Dfixture=true"}))
        (source / "marker.txt").write_text("preserve me")
        self.assertEqual(len(s.ok("modpack.ftb-local.list", path=str(source.parent))), 1)
        result = s.ok("modpack.ftb-local.import", path=str(source), name="FTB migrated", group="API fixtures")
        self.assertTrue(result["installed"])
        self.assertEqual(len(result["instances"]), 1)
        migrated = self.root / "instances" / result["instances"][0]
        self.assertTrue(any(p.read_text() == "preserve me" for p in migrated.rglob("marker.txt")))
        self.assertEqual((source / "marker.txt").read_text(), "preserve me")

    def test_23_binary_screenshots_and_panel_state(self):
        import zlib
        s = self.sidecar
        payload = bytes(range(256))
        s.ok("instance.file.write", instance="neo-client", path="binary.bin", encoding="base64", content=base64.b64encode(payload).decode())
        self.assertEqual(base64.b64decode(s.ok("instance.file.read", instance="neo-client", path="binary.bin", encoding="base64")["content"]), payload)
        s.ok("instance.file.rename", instance="neo-client", path="binary.bin", destination="renamed.bin")
        self.assertFalse(s.execute("instance.file.rename", instance="neo-client", path="renamed.bin", destination="../escape")["ok"])
        def chunk(kind, data):
            return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff)
        png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack(">IIBBBBB", 1, 1, 8, 2, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(b'\x00\xff\x00\x00')) + chunk(b'IEND', b'')
        source = self.root / "red.png"
        source.write_bytes(png)
        s.ok("instance.screenshot.import", instance="neo-client", source=str(source), name="red.png")
        self.assertEqual(base64.b64decode(s.ok("instance.screenshot.read", instance="neo-client", file="red.png")["content"]), png)
        s.ok("instance.screenshot.rename", instance="neo-client", file="red.png", name="renamed.png")
        self.assertFalse(s.execute("instance.screenshot.rename", instance="neo-client", file="renamed.png", name="../outside.png")["ok"])
        s.ok("instance.screenshot.delete", instance="neo-client", file="renamed.png", confirm=True)
        s.ok("instance.file.remove", instance="neo-client", path="lunaui/panel.js", confirm=True)
        s.ok("instance.file.write", instance="neo-client", path="lunaui/controls.json", content=json.dumps({"title": "Controls", "controls": [
            {"id": "enabled", "type": "toggle", "default": True},
            {"id": "mode", "type": "select", "options": ["first", "second"]},
            {"type": "account-select"}]}))
        opened = s.ok("instance.custom-ui.open", instance="neo-client")
        sid = opened["sessionId"]
        self.assertEqual(s.ok("instance.custom-ui.open", instance="neo-client")["sessionId"], sid)
        self.assertTrue(opened["tabs"][0]["controls"][0]["value"])
        self.assertEqual(opened["tabs"][0]["controls"][1]["value"], "first")
        self.assertFalse(s.ok("instance.custom-ui.trigger", sessionId=sid, tab=0, control=1, value="missing")["complete"])
        self.assertTrue(s.ok("instance.custom-ui.trigger", sessionId=sid, tab=0, control=2, value="")["complete"])
        self.assertTrue(s.ok("instance.custom-ui.save", sessionId=sid)["complete"])
        state = s.ok("instance.custom-ui.state", instance="neo-client")
        s.ok("instance.custom-ui.save-state", instance="neo-client", revision=state["revision"], state={"external": True})
        self.assertFalse(s.ok("instance.custom-ui.save", sessionId=sid)["complete"])
        self.assertTrue(s.ok("instance.custom-ui.state", instance="neo-client")["state"]["external"])
        s.ok("instance.custom-ui.close", sessionId=sid)

    def test_19_custom_ui_runtime(self):
        s = self.sidecar
        script = '''
function clicked(e) { launcher.setState('fromHandler', e.value); launcher.saveState(); }
var tabs = [{title:'Test', controls:[{id:'toggle',type:'toggle',onChange:'clicked'}]}];
'''
        s.ok("instance.file.write", instance="neo-client", path="lunaui/panel.js", content=script)
        panel = s.ok("instance.custom-ui.open", instance="neo-client")
        sid = panel["sessionId"]
        try:
            self.assertEqual(panel["errors"], [])
            result = s.ok("instance.custom-ui.trigger", sessionId=sid, tab=0, control=0, value=True)
            self.assertTrue(result["complete"])
            self.assertTrue(result["state"]["fromHandler"])
            self.assertTrue(s.ok("instance.custom-ui.state", instance="neo-client")["state"]["fromHandler"])
            result = s.ok("instance.custom-ui.call", sessionId=sid, method="fs.writeFile", arguments=["lunaui/bridge.txt", "bridge"])
            self.assertTrue(result["value"])
            self.assertEqual(s.ok("instance.file.read", instance="neo-client", path="lunaui/bridge.txt")["content"], "bridge")
            result = s.ok("instance.custom-ui.call", sessionId=sid, method="fs.rm", arguments=[".", True])
            self.assertFalse(result["value"])
            result = s.ok("instance.custom-ui.call", sessionId=sid, method="fs.writeFile", arguments=["../escape", "bad"])
            self.assertFalse(result["value"])
            self.assertFalse(s.execute("instance.custom-ui.trigger", sessionId=sid, tab=99, control=0, value=True)["ok"])
        finally:
            s.ok("instance.custom-ui.close", sessionId=sid)
        self.assertFalse(s.execute("instance.custom-ui.snapshot", sessionId=sid)["ok"])
        s.ok("instance.file.write", instance="neo-client", path="lunaui/loop.js", content="while(true){}")
        panel = s.ok("instance.custom-ui.open", instance="neo-client")
        self.assertTrue(panel["errors"])
        s.ok("instance.custom-ui.close", sessionId=panel["sessionId"])
        s.ok("instance.file.remove", instance="neo-client", path="lunaui/loop.js", confirm=True)

    def test_18_update_marker_stream(self):
        s = self.sidecar
        sub = s.ok("launcher.update.subscribe")["subscriptionId"]
        try:
            initial = s.ok("event.poll", subscriptionId=sub)
            (self.root / ".prism_launcher_update.fail").write_text("fixture")
            batch = s.receive(lambda m: m.get("method") == "launcher/stream" and m["params"]["subscriptionId"] == sub and
                any(e["data"]["updateFailureMarker"] for e in m["params"]["events"]))["params"]
            self.assertGreater(batch["nextCursor"], initial["nextCursor"])
            self.assertTrue(s.ok("launcher.update.status")["updateFailureMarker"])
            before = s.ok("launcher.update.status")["automatic"]
            self.assertFalse(s.execute("launcher.update.configure", automatic=not before, intervalSeconds=-1)["ok"])
            self.assertEqual(s.ok("launcher.update.status")["automatic"], before)
        finally:
            s.ok("event.unsubscribe", subscriptionId=sub)
            (self.root / ".prism_launcher_update.fail").unlink(missing_ok=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", type=lambda path: Path(path).resolve(), required=True)
    parser.add_argument("--online", action="store_true")
    parser.add_argument("--tests", nargs="*", default=[], help="Optional unittest test names")
    ARGS = parser.parse_args()
    if not ARGS.exe.is_file():
        parser.error("--exe must point to an installed launcher CLI executable")
    unittest.main(argv=[__file__, *ARGS.tests], verbosity=2)
