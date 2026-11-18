"""Verify the installed NeoUI's real bridge exits and reaps MCP on update.

The updater in the disposable installation is a harmless Windows where.exe.
The real SFX installation transaction is tested separately; no network or user
installation is involved in this frontend handoff probe.
"""
import argparse
import json
import os
from pathlib import Path
import queue
import shutil
import subprocess
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

from webui_smoke import temporary_profile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--installation', required=True, type=Path)
    args = parser.parse_args()
    events = queue.Queue()

    class Receiver(BaseHTTPRequestHandler):
        def log_message(self, *_): pass
        def do_POST(self):
            events.put(json.loads(self.rfile.read(int(self.headers['Content-Length']))))
            self.send_response(200)
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()
            self.wfile.write(b'OK')

    server = ThreadingHTTPServer(('127.0.0.1', 0), Receiver)
    thread = threading.Thread(target=server.serve_forever)
    thread.start()
    try:
        with temporary_profile() as temporary:
            root = Path(temporary)
            install = root / 'installation'
            install.mkdir()
            for name in ('luna-neo-ui.exe', 'lunalauncher-cli.exe'):
                shutil.copy2(args.installation / name, install / name)
            for file in args.installation.glob('*.dll'):
                shutil.copy2(file, install / file.name)
            shutil.copytree(args.installation / 'platforms', install / 'platforms')
            shutil.copy2(Path(os.environ['SystemRoot']) / 'System32' / 'where.exe', install / 'lunalauncher_updater.exe')
            profile = root / 'profile'
            bundle = profile / 'webui' / 'handoff-probe'
            bundle.mkdir(parents=True)
            (bundle / 'webui.json').write_text('{"name":"Update handoff test"}', encoding='utf-8')
            (bundle / 'index.html').write_text('<!doctype html><meta charset="utf-8"><body>Update handoff test<script src="./probe.js"></script>', encoding='utf-8')
            probe = '''
const report = value => fetch(__REPORT__, {method:'POST',body:JSON.stringify(value)});
(async () => {
 const deadline = Date.now() + 10000;
 while (!window.__TAURI__?.core?.invoke && !window.__TAURI_INTERNALS__?.invoke) {
   if (Date.now() > deadline) throw new Error('Native IPC was not injected');
   await new Promise(resolve => setTimeout(resolve, 50));
 }
 const invoke = window.__TAURI__?.core?.invoke || window.__TAURI_INTERNALS__.invoke;
 const execute = (operation, parameters = {}) => invoke('launcher_request', {method:'launcher/execute',parameters:{operation,parameters}});
 const status = await execute('launcher.update.status');
 if (!status.ok || !status.data.supported) throw new Error(JSON.stringify(status));
 const result = await execute('launcher.update.apply', {versionTag:'v11.0.2-20990101'});
 if (!result.ok || !result.data.shutdownRequired || !result.data.externalUpdaterUi || result.data.restartByFrontend)
   throw new Error(JSON.stringify(result));
 await report({stage:'handoff', updater:result.data.updater});
})().catch(error => report({stage:'error',error:String(error)}));
'''
            (bundle / 'probe.js').write_text(probe.replace('__REPORT__', json.dumps(f'http://127.0.0.1:{server.server_port}/')), encoding='utf-8')
            env = dict(os.environ, LUNA_LAUNCHER_CLI=str(install / 'lunalauncher-cli.exe'),
                       LUNA_LAUNCHER_DATA=str(profile), LUNA_WEB_UI='data/handoff-probe',
                       WEBVIEW2_USER_DATA_FOLDER=str(root / 'webview'))
            host = subprocess.Popen([str(install / 'luna-neo-ui.exe')], cwd=install, env=env)
            try:
                event = events.get(timeout=35)
                assert event['stage'] == 'handoff', event
                assert Path(event['updater']).parent.resolve() == install.resolve(), event
                host.wait(timeout=15)
                assert host.returncode == 0, host.returncode
                print('PASS: real NeoUI bridge update response, native exit and CLI cleanup')
            finally:
                if host.poll() is None:
                    subprocess.run(['taskkill', '/PID', str(host.pid), '/T', '/F'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                    host.wait(timeout=15)
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)


if __name__ == '__main__':
    main()
