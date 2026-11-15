# SPDX-License-Identifier: GPL-3.0-only
"""Verify managed local login, legacy account refresh and reauthentication."""
import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import tempfile
import threading
import time

from api_sidecar_smoke import Sidecar


def verify(executable):
    with tempfile.TemporaryDirectory(prefix='luna-local-auth-') as folder:
        root = Path(folder)
        (root / 'lunalauncher.cfg').write_text('[General]\nLanguage=en_US\nLocalServiceCache=false\n', encoding='utf-8')
        name, password = 'LocalFixture', 'local-fixture-password'
        s = Sidecar(executable, root)
        try:
            s.ok('account.login', type='yggdrasil', managedLocal=True, username=name,
                 password=password, sourceName='Local verification fixture')
            # This is the same refresh used at the start of instance.launch.
            s.ok('account.refresh', account=name)
            assert s.ok('account.profile', account=name)['name'] == name
            print('PASS: managed local login and immediate token refresh')
        finally:
            s.close()

        accounts_path = root / 'accounts.json'
        saved = json.loads(accounts_path.read_text(encoding='utf-8'))
        config = saved['accounts'][0]['yggdrasilConfig']
        for key in ['authenticateEndpoint', 'refreshEndpoint', 'validateEndpoint', 'profileEndpoint']:
            config.pop(key, None)  # Match accounts created before this fix.
        config['authServerUrl'] = config['sessionServerUrl'] = 'http://127.0.0.1:1'
        accounts_path.write_text(json.dumps(saved), encoding='utf-8')
        s = Sidecar(executable, root)
        try:
            s.ok('account.refresh', account=name)
            started = s.call('launcher/execute', {'operation': 'neo.task.start',
                             'parameters': {'operation': 'account.refresh', 'parameters': {'account': name}}})
            assert started['ok'], started
            task = started['data']
            deadline = time.monotonic() + 20
            while task['state'] not in ['succeeded', 'failed', 'cancelled']:
                assert time.monotonic() < deadline, task
                time.sleep(0.05)
                task = s.ok('neo.task.status', taskId=task['taskId'])
            assert task['state'] == 'succeeded', task
            print('PASS: legacy account repairs missing endpoints/stale service port; async refresh also succeeds')
        finally:
            s.close()
        saved = json.loads(accounts_path.read_text(encoding='utf-8'))
        config = saved['accounts'][0]['yggdrasilConfig']
        assert config['managedLocal'] is True
        assert config['refreshEndpoint'] == '/authserver/refresh', config

        # Force real reauthentication rather than merely checking a copied flag.
        saved['accounts'][0]['ygg']['token'] = 'revoked-synthetic-token'
        accounts_path.write_text(json.dumps(saved), encoding='utf-8')
        instance = root / 'instances' / 'local-auth-fixture'
        instance.mkdir(parents=True)
        (instance / 'instance.cfg').write_text('[General]\nInstanceType=OneSix\nname=Local auth fixture\n', encoding='utf-8')
        (instance / 'mmc-pack.json').write_text('{"formatVersion":1,"components":[]}', encoding='utf-8')
        s = Sidecar(executable, root)
        try:
            request = s.start('instance.launch', instance='local-auth-fixture', account=name)
            for label, value in [('Username', name), ('Password', password)]:
                prompt = s.receive(lambda m: m.get('params', {}).get('kind') == 'input', timeout=40)['params']
                assert label in json.dumps(prompt), prompt
                s.call('launcher/respond', {'interactionId': prompt['interactionId'], 'value': value})
            # The empty fixture cannot launch a game; authentication must finish
            # and reach the ordinary missing components/agent diagnostic.
            result = s.response(request, timeout=40)['result']
            assert not result['ok'], result
            saved = json.loads(accounts_path.read_text(encoding='utf-8'))
            assert len(saved['accounts']) == 1
            assert saved['accounts'][0]['yggdrasilConfig']['managedLocal'] is True
            s.ok('account.refresh', account=name)
            print('PASS: revoked token reauthenticates and preserves managed service ownership')
        finally:
            s.close()

        class Provider(BaseHTTPRequestHandler):
            status = 404

            def log_message(self, *_):
                pass

            def do_POST(self):
                self.rfile.read(int(self.headers.get('Content-Length', 0)))
                self.send_response(self.status)
                self.end_headers()
                self.wfile.write(b'not JSON')

        server = ThreadingHTTPServer(('127.0.0.1', 0), Provider)
        threading.Thread(target=server.serve_forever, daemon=True).start()
        s = Sidecar(executable, root)
        try:
            url = f'http://127.0.0.1:{server.server_port}'
            for status in [404, 405, 401, 403]:
                Provider.status = status
                result = s.execute('account.login', type='yggdrasil', username='Synthetic', password='synthetic',
                                   authUrl=url, sessionUrl=url)
                assert not result['ok'] and f'HTTP {status}' in result['error'], result
                assert 'parse' not in result['error'].lower(), result
            print('PASS: HTTP endpoint and credential errors remain visible after response cleanup')
        finally:
            s.close()
            server.shutdown()
            server.server_close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, default=Path('install-webui-authfix-release/lunalauncher-cli.exe'))
    verify(parser.parse_args().exe.resolve())
