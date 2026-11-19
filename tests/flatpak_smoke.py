#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Install the actual bundle and exercise its sandbox with disposable data.

Requires the Flatpak runtime, a session bus and an X11/Wayland desktop (Xvfb is
supported). Installing the supplied bundle updates the user's Luna Flatpak;
tests never read or write the user's normal launcher profile.
"""
import argparse
import hashlib
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from functools import partial
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
import tarfile
import threading

from api_sidecar_smoke import Sidecar
import instance_export_smoke

APP_ID = "org.lunalauncher.LunaLauncher"
SCRIPTS = Path(__file__).resolve().parent


def run(command, **kwargs):
    result = subprocess.run(command, text=True, capture_output=True, timeout=120, **kwargs)
    if result.returncode:
        raise AssertionError(f"{command}: exit {result.returncode}\n{result.stdout}\n{result.stderr}")
    return result.stdout


def cf_probe(command, root):
    """Verify the actual helper loads and rejects an untrusted download URL.

    The helper intentionally accepts only public HTTPS CurseForge download
    pages. Neo's local rendering probe separately exercises WebKit processes.
    """
    request = root / "cf-request.json"
    request.write_text(json.dumps({"downloadDirectory": str(root / "downloads"),
        "items": [{"url": "http://127.0.0.1/invalid", "fileName": "fixture.jar"}]}))
    result = subprocess.run([*command, "--luna-entry", "cf-webview", "--request", str(request)],
                            capture_output=True, text=True, timeout=30)
    assert result.returncode == 2 and "unsupported CurseForge URL" in result.stdout, result


def managed_java_probe(client, root):
    """Download, verify and extract a real JRE into writable sandbox data."""
    location = Path(run(["flatpak", "--user", "info", "--show-location", APP_ID]).strip())
    archive = root / "jdk21.tar.gz"
    # OSTree deduplicates unrelated files as hard links. Materialize them for
    # this download fixture rather than encoding OSTree's storage layout into
    # the Java archive (whose top-level directory is stripped on installation).
    with tarfile.open(archive, "w:gz", compresslevel=1, dereference=True) as output:
        output.add(location / "files/jdk/21", arcname="jdk21")
    with archive.open("rb") as stream:
        checksum = hashlib.file_digest(stream, "sha256").hexdigest()

    class Files(SimpleHTTPRequestHandler):
        def log_message(self, *_):
            pass

    server = ThreadingHTTPServer(("127.0.0.1", 0), partial(Files, directory=str(root)))
    threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        client.ok("settings.set", scope="launcher", key="PclDownloadFallbackToQt", value=False)
        installed = client.ok("java.install", name="flatpak-managed-probe", downloadType="archive",
            url=f"http://127.0.0.1:{server.server_port}/{archive.name}", checksumType="sha256", checksumHash=checksum)
        runtimes = client.ok("java.refresh", managedOnly=True)
        assert any(j["path"].startswith(installed["path"] + "/") for j in runtimes), runtimes
    finally:
        server.shutdown()
        server.server_close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bundle", required=True, type=Path)
    args = parser.parse_args()
    bundle = args.bundle.resolve(strict=True)
    record = json.loads((bundle.parent / "linux-x64-flatpak.json").read_text())
    with bundle.open("rb") as stream:
        assert hashlib.file_digest(stream, "sha256").hexdigest() == record["sha256"]
    assert bundle.stat().st_size == record["size"] and bundle.name == record["asset"]
    installed = subprocess.run(["flatpak", "--user", "info", "--show-commit", APP_ID], capture_output=True, text=True)
    # Older Flatpak rejects reinstalling an identical bundle. Verify the exact
    # OSTree commit instead of treating an 'already installed' error as success.
    if installed.returncode or installed.stdout.strip() != record["ostreeCommit"]:
        run(["flatpak", "--user", "install", "--noninteractive", "-y", "--reinstall", "--bundle", str(bundle)])
    assert run(["flatpak", "--user", "info", "--show-commit", APP_ID]).strip() == record["ostreeCommit"]
    print("PASS: bundle checksum and installation", flush=True)
    with tempfile.TemporaryDirectory(prefix="luna-flatpak-smoke-") as temporary:
        root = Path(temporary)
        # Child probes create their fixtures beneath the same explicit grant.
        os.environ["TMPDIR"] = str(root)
        previous_tempdir = tempfile.tempdir
        tempfile.tempdir = str(root)
        try:
            # The Neo probe isolates XDG_DATA_HOME too. Keep the host Flatpak
            # installation discoverable; pass the disposable XDG path through
            # --env only to the application sandbox.
            base = ["env", "XDG_DATA_HOME=" + os.environ.get("XDG_DATA_HOME", str(Path.home() / ".local/share")),
                    "flatpak", "run", "--user", "--filesystem=" + str(root)]
            # Flatpak resets XDG directories after processing --env. Set the
            # test-only directories inside the sandbox before the real wrapper
            # runs, exercising its default path without touching a user profile.
            isolated = '''export XDG_DATA_HOME="$1/data" XDG_CONFIG_HOME="$1/config" XDG_CACHE_HOME="$1/cache"
shift
exec /app/bin/lunalauncher "$@"
'''
            application = ["--command=sh", APP_ID, "-c", isolated, "luna-smoke", str(root)]
            headless = [*base, "--env=QT_QPA_PLATFORM=offscreen", *application]
            context = json.loads(run([*headless, "--cli", "--print-startup-context"]))
            profile = root / "data/LunaLauncher"
            assert Path(context["dataRoot"]) == profile, context
            assert "LunaLauncher " + record["version"] in run([*headless, "--version"])
            assert Path(json.loads(run([*headless, "--cli", "--print-startup-context"]))["dataRoot"]) == profile
            # Audit every deployed ELF against the runtime, including dlopen
            # libraries, Qt plugins, WebKit processes, .NET AOT and JRE modules.
            audit = '''export LD_LIBRARY_PATH=/app/runtime/usr/lib:/app/jdk/21/lib:/app/jdk/21/lib/server
command -v ldd >/dev/null || exit 1
failed=0
while IFS= read -r file; do
  details="$(ldd "$file" 2>&1)"
  status=$?
  if [ "$status" -ne 0 ]; then
    case "$details" in *"not a dynamic executable"*|*"statically linked"*) continue;;
      *) printf '%s\\n%s\\n' "$file" "$details"; failed=1; continue;; esac
  fi
  case "$details" in *"not found"*|*"/home/"*|*"/tmp/luna-"*)
    printf '%s\\n%s\\n' "$file" "$details"; failed=1;; esac
done < /app/share/luna-flatpak-elfs.txt
exit "$failed"
'''
            run([*base, "--command=sh", APP_ID, "-c", audit])
            installed_record = json.loads(run([*base, "--command=cat", APP_ID, "/app/share/luna-flatpak.json"]))
            assert all(record[key] == value for key, value in installed_record.items()), installed_record
            run([*base, "--command=/app/jdk/21/bin/java", APP_ID, "-version"])
            print("PASS: startup paths, version, provenance, all ELF dependencies and bundled Java", flush=True)
            # The app cannot modify /app or read an ungranted host directory.
            denied = root.parent / (root.name + "-private")
            denied.mkdir()
            try:
                (denied / "sentinel").write_text("host-only")
                run([*base, "--command=sh", APP_ID, "-c",
                     'test -f /.flatpak-info && test ! -r "$1/sentinel" && test ! -w /app/bin/lunalauncher',
                     "probe", str(denied)])
            finally:
                (denied / "sentinel").unlink()
                denied.rmdir()
            print("PASS: denied ungranted host files and read-only application", flush=True)
            wrapper = root / "headless"
            wrapper.write_text("#!/bin/sh\nexec " + shlex.join(headless) + ' "$@"\n')
            wrapper.chmod(0o755)
            client = Sidecar(wrapper, profile)
            try:
                client.call("initialize")
                status = client.ok("launcher.update.status")
                assert status["supported"] is False and status["managedBy"] == "flatpak", status
                assert "flatpak update" in status["unsupportedReason"], status
                rejected = client.execute("launcher.update.apply", versionTag="v99.0.0")
                assert rejected["ok"] is False and "flatpak" in json.dumps(rejected).lower(), rejected
                assert client.execute("startup.set-data-location", portable=True)["ok"] is False
                client.ok("launcher.update.configure", automatic=False, intervalSeconds=3600)
                javas = client.ok("java.refresh")
                assert any(j.get("path", "").startswith("/app/jdk/21/") for j in javas), javas
                managed_java_probe(client, root)
                print("PASS: managed updates, data-location protection, Java detection and managed download", flush=True)
            finally:
                client.close()
            client = Sidecar(wrapper, profile)
            try:
                client.call("initialize")
                assert client.ok("launcher.update.status")["intervalSeconds"] == 3600
            finally:
                client.close()
            instance_export_smoke.main(wrapper)
            if record["neoUi"]:
                # Preserve the probe's own per-test data and external UI path
                # through Flatpak's environment filter, including its IPC check.
                gui_wrapper = root / "gui"
                gui_base = base
                if os.environ.get("GDK_BACKEND") == "x11":
                    # WSLg can expose Wayland while this test explicitly uses
                    # Xvfb. fallback-x11 alone then intentionally hides X11.
                    gui_base = [*base, "--nosocket=wayland", "--socket=x11"]
                gui_wrapper.write_text("#!/bin/sh\nexec " + shlex.join(gui_base) +
                    ' --env=LUNA_LAUNCHER_DATA="$LUNA_LAUNCHER_DATA" --env=LUNA_WEB_UI="$LUNA_WEB_UI" ' +
                    shlex.join(application) + ' "$@"\n')
                gui_wrapper.chmod(0o755)
                run([sys.executable, str(SCRIPTS / "appimage_neo_smoke.py"), "--flatpak", "--image", str(gui_wrapper)])
            cf_probe([*base, *application], root)
            print("PASS: Flatpak bundle, all ELF dependencies, bundled/managed Java, sandbox isolation, "
                  "persistent API settings, ZIP/MR/CF exports, Neo IPC/WebKit and CF helper")
        finally:
            tempfile.tempdir = previous_tempdir


if __name__ == "__main__":
    main()
