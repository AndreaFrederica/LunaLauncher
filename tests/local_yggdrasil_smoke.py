# SPDX-License-Identifier: GPL-3.0-only
"""Real Qt launcher + Rust child integration, using only loopback fixtures.

python tests/local_yggdrasil_smoke.py path/to/lunalauncher-cli.exe
The Rust service must be beside the launcher. No real Microsoft tokens or game
installation are needed; the launch case stops at the missing agent diagnostic.
"""
import json
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import time
import urllib.request

from api_sidecar_smoke import Sidecar


def main():
    executable = Path(sys.argv[1]).resolve()
    daemon = executable.with_name('yggdrasil-server' + executable.suffix)
    hits = []
    unavailable = threading.Event()

    class Origin(BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass

        def do_CONNECT(self):
            self.send_error(502)  # Synthetic MSA refresh: never contact Microsoft.

        def do_GET(self):
            hits.append(self.path)
            if unavailable.is_set():
                self.send_error(503)
                return
            payload = {'formatVersion': 1, 'packages': [{'uid': 'net.minecraft', 'name': 'Minecraft'}]}
            if self.path.endswith('/net.minecraft/index.json'):
                payload = {'formatVersion': 1, 'uid': 'net.minecraft', 'name': 'Minecraft', 'versions': [
                    {'version': '1.21.1', 'type': 'release', 'releaseTime': '2024-08-08T00:00:00Z', 'requires': []}]}
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Cache-Control', 'max-age=0')
            self.send_header('ETag', 'fixture')
            self.end_headers()
            self.wfile.write(json.dumps(payload).encode())

    server = ThreadingHTTPServer(('127.0.0.1', 0), Origin)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        with tempfile.TemporaryDirectory(prefix='ygg-launcher-') as folder:
            root = Path(folder)
            (root / 'lunalauncher.cfg').write_text(
                '[General]\nLanguage=en_US\nLocalServiceCache=true\nMSAClientIDOverride=fixture-client\n'
                f'MetaURLOverride=http://127.0.0.1:{server.server_port}/v1/\n', encoding='utf-8')
            sidecar = Sidecar(executable, root)
            try:
                versions = sidecar.ok('component.versions', uid='net.minecraft')
                assert versions['versions'][0]['version'] == '1.21.1', versions
                assert hits, 'Metadata never reached the local origin'
            finally:
                sidecar.close()
            assert (root / 'yggdrasil' / 'identity.sqlite3').exists(), 'Qt did not start the Rust gateway'
            # Force Qt to request the metadata again; preserve only the Rust cache.
            for file in (root / 'meta').rglob('*.json'):
                file.unlink()
            unavailable.set()
            previous_hits = len(hits)
            sidecar = Sidecar(executable, root)
            try:
                versions = sidecar.ok('component.versions', uid='net.minecraft')
                assert versions['versions'][0]['version'] == '1.21.1', versions
                assert len(hits) > previous_hits, 'Restart did not revalidate against the failing origin'
            finally:
                sidecar.close()
            print('PASS: Qt metadata requests use persistent Rust stale cache after restart')

            uid = '123456789abc4def8123456789abcdef'
            profile = {'id': uid, 'name': 'Alice', 'skin': {'id': '', 'url': '', 'variant': ''}, 'capes': []}
            verified = int(time.time())
            child = subprocess.Popen([str(daemon), '--launcher', '--listen', '127.0.0.1:0', '--data-dir', str(root / 'yggdrasil')],
                                     stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            try:
                ready = json.loads(child.stdout.readline())
                opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
                request = urllib.request.Request(ready['url'] + '/api/v1/account-snapshots/' + uid,
                    data=json.dumps({'profile': profile, 'entitlement': {'ownsMinecraft': True, 'canPlayMinecraft': True},
                                     'verified_at': verified}).encode(), method='PUT',
                    headers={'Authorization': 'Bearer ' + ready['managementToken'], 'Content-Type': 'application/json'})
                with opener.open(request, timeout=10) as response:
                    assert response.status == 200
            finally:
                child.stdin.close()
                child.wait(timeout=15)
                child.stdout.close()
                child.stderr.close()
            account = {'type': 'MSA', 'active': True, 'msa-client-id': 'fixture-client',
                       'msa': {'token': 'synthetic', 'refresh_token': 'synthetic-refresh-token'},
                       'profile': profile, 'entitlement': {'ownsMinecraft': True, 'canPlayMinecraft': True},
                       'officialVerifiedAt': verified}
            (root / 'accounts.json').write_text(json.dumps({'formatVersion': 3, 'accounts': [account]}), encoding='utf-8')
            config_path = root / 'lunalauncher.cfg'
            lines = [line for line in config_path.read_text(encoding='utf-8').splitlines()
                     if not line.startswith(('ProxyType=', 'ProxyAddr=', 'ProxyPort='))]
            offset = lines.index('[General]') + 1
            lines[offset:offset] = ['ProxyType=HTTP', 'ProxyAddr=127.0.0.1', f'ProxyPort={server.server_port}']
            config_path.write_text('\n'.join(lines) + '\n', encoding='utf-8')
            instance = root / 'instances' / 'cached-account'
            instance.mkdir(parents=True)
            (instance / 'instance.cfg').write_text('[General]\nInstanceType=OneSix\nname=Cached account\n', encoding='utf-8')
            (instance / 'mmc-pack.json').write_text('{"formatVersion":1,"components":[]}', encoding='utf-8')
            sidecar = Sidecar(executable, root)
            try:
                launch = sidecar.start('instance.launch', instance='cached-account')
                prompt = sidecar.receive(lambda m: m.get('params', {}).get('kind') == 'input', timeout=40)['params']
                assert 'Use Local Authentication' in prompt.get('choices', []), prompt
                sidecar.call('launcher/respond', {'interactionId': prompt['interactionId'], 'value': 0})
                result = sidecar.response(launch, timeout=15)['result']
                assert not result['ok'], result
                assert 'authlib-injector' in json.dumps(result), result
                assert sidecar.call('ping') == {}
                print('PASS: unreachable official authentication offers local identity; missing agent fails explicitly')
            finally:
                sidecar.close()
    finally:
        server.shutdown()
        server.server_close()


if __name__ == '__main__':
    main()
