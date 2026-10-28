# SPDX-License-Identifier: GPL-3.0-only
"""Check headless launch starts account refresh and accepts cancellation.

Run against an installed CLI: python tests/neo_launch_auth_regression.py EXE
Uses an isolated profile, synthetic credentials and a loopback HTTP fixture.
"""
import json
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import sys
import tempfile
import threading

from api_sidecar_smoke import Sidecar


def main():
    received = threading.Event()
    release = threading.Event()

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass

        def do_CONNECT(self):
            # Never tunnel traffic: hold the synthetic MSA refresh at the local
            # proxy so cancellation can be checked without contacting Microsoft.
            if 'login.live.com' in self.path or 'microsoftonline.com' in self.path:
                received.set()
                release.wait(20)
            self.send_error(502)

    server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        with tempfile.TemporaryDirectory(prefix='neo-auth-regression-') as folder:
            root = Path(folder)
            (root / 'lunalauncher.cfg').write_text(
                f'[General]\nMSAClientIDOverride=fixture-client\nProxyType=HTTP\nProxyAddr=127.0.0.1\nProxyPort={server.server_port}\n')
            instance = root / 'instances' / 'auth-test'
            instance.mkdir(parents=True)
            (instance / 'instance.cfg').write_text('[General]\nInstanceType=OneSix\nname=auth-test\n')
            (instance / 'mmc-pack.json').write_text('{"formatVersion":1,"components":[]}')
            account = {
                'type': 'MSA', 'active': True,
                'msa-client-id': 'fixture-client',
                'msa': {'token': 'fixture', 'refresh_token': 'synthetic-refresh-token'},
                'profile': {'id': '12345678901234567890123456789012', 'name': 'Fixture',
                            'skin': {'id': '', 'url': '', 'variant': ''}, 'capes': []},
            }
            (root / 'accounts.json').write_text(json.dumps({'formatVersion': 3, 'accounts': [account]}))
            sidecar = Sidecar(Path(sys.argv[1]).resolve(), root)
            try:
                launch = sidecar.start('instance.launch', instance='auth-test')
                if not received.wait(10):
                    print('Fixture task:', sidecar.ok('task.status'))
                    print('Fixture events:', sidecar.backlog)
                    raise AssertionError('Launch never started the account refresh HTTP request')
                status = sidecar.ok('task.status')
                assert status['canAbort'], status
                assert status['status'], 'Refresh progress must be visible'
                cancelled = sidecar.ok('task.cancel')
                assert cancelled['cancelled'], cancelled
                response = sidecar.response(launch, timeout=5)
                assert response['result']['ok'] is False, response
                assert sidecar.call('ping') == {}
                assert all(a['state'].lower() != 'working' for a in sidecar.ok('account.list'))
                print('PASS: refresh started, launch cancelled, sidecar remained responsive')
            finally:
                release.set()
                sidecar.close()
    finally:
        release.set()
        server.shutdown()
        server.server_close()


if __name__ == '__main__':
    main()
