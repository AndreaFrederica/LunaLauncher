"""Exercise the packaged NeoUI, WebKit helpers and native MCP bridge on a desktop.

Uses an isolated external UI and user profile; does not install an update or
touch the user's launcher data. Requires a working X11/Wayland display (WSLg
works). A --version check alone cannot cover these dependencies.
"""
import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import queue
import signal
import subprocess
import tempfile
import threading


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", required=True, type=Path)
    parser.add_argument("--portable", action="store_true", help="Probe a deployed portable AppRun instead of an AppImage")
    args = parser.parse_args()
    events = queue.Queue()

    class Receiver(BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass

        def do_POST(self):
            events.put(json.loads(self.rfile.read(int(self.headers["Content-Length"]))))
            self.send_response(200)
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(b"OK")

    server = ThreadingHTTPServer(("127.0.0.1", 0), Receiver)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        with tempfile.TemporaryDirectory(prefix="luna-neo-appimage-") as temporary:
            root = Path(temporary)
            profile = root / "profile"
            bundle = profile / "webui" / "appimage-probe"
            bundle.mkdir(parents=True)
            (bundle / "webui.json").write_text('{"name":"AppImage runtime test"}', encoding="utf-8")
            (bundle / "index.html").write_text(
                '<!doctype html><meta charset="utf-8"><body>AppImage runtime test<script src="./probe.js"></script>',
                encoding="utf-8")
            probe = """
const report = value => fetch(__REPORT__, {method:'POST',body:JSON.stringify(value)});
(async () => {
 const deadline = Date.now() + 15000;
 while (!window.__TAURI__?.core?.invoke && !window.__TAURI_INTERNALS__?.invoke) {
   if (Date.now() > deadline) throw new Error('Native IPC was not injected');
   await new Promise(resolve => setTimeout(resolve, 50));
 }
 const invoke = window.__TAURI__?.core?.invoke || window.__TAURI_INTERNALS__.invoke;
 const execute = (operation, parameters = {}) => invoke('launcher_request',
   {method:'launcher/execute',parameters:{operation,parameters}});
 const status = await execute('launcher.update.status');
 if (!status.ok || !status.data.supported) throw new Error(JSON.stringify(status));
 const configured = await execute('launcher.update.configure', {automatic:false, intervalSeconds:3600});
 if (!configured.ok || configured.data.automatic !== false) throw new Error(JSON.stringify(configured));
 await report({stage:'ready', status:status.data});
})().catch(error => report({stage:'error',error:String(error)}));
"""
            (bundle / "probe.js").write_text(probe.replace("__REPORT__", json.dumps(
                f"http://127.0.0.1:{server.server_port}/")), encoding="utf-8")
            env = {key: value for key, value in os.environ.items() if key not in {
                "LD_LIBRARY_PATH", "LD_PRELOAD", "QT_PLUGIN_PATH", "QT_QPA_PLATFORM_PLUGIN_PATH",
                "APPDIR", "APPIMAGE", "LUNA_LAUNCHER_CLI", "QT_QPA_PLATFORM"}}
            env.update(LUNA_LAUNCHER_DATA=str(profile), LUNA_WEB_UI="data/appimage-probe",
                       PATH="/usr/bin:/bin", XDG_DATA_DIRS="/usr/local/share:/usr/share",
                       XDG_DATA_HOME=str(root / "xdg-data"), XDG_CONFIG_HOME=str(root / "xdg-config"),
                       XDG_CACHE_HOME=str(root / "xdg-cache"))
            with (root / "runtime.log").open("w+") as log:
                arguments = [str(args.image.resolve())]
                if not args.portable:
                    arguments.append("--appimage-extract-and-run")
                host = subprocess.Popen([*arguments, "--luna-entry", "neo"],
                                        cwd=root, env=env, stdout=log, stderr=log, start_new_session=True)
                try:
                    event = events.get(timeout=90)
                    assert event["stage"] == "ready", event
                    assert Path(event["status"]["dataRoot"]) == profile, event
                    print("PASS: packaged NeoUI, WebKit helpers, native MCP bridge and isolated update settings")
                except Exception:
                    log.flush()
                    log.seek(0)
                    print(log.read()[-12000:])
                    raise
                finally:
                    # Kill only this disposable process group, including runtime
                    # extraction and any WebKit children, even if the host exited.
                    try:
                        os.killpg(host.pid, signal.SIGTERM)
                    except ProcessLookupError:
                        pass
                    try:
                        host.wait(timeout=10)
                    except subprocess.TimeoutExpired:
                        os.killpg(host.pid, signal.SIGKILL)
                        host.wait(timeout=10)
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)


if __name__ == "__main__":
    main()
