# SPDX-License-Identifier: GPL-3.0-only
"""Check imported modpack trust interactions through the installed Luna backend.

python tests/untrusted_modpack_regression.py --exe /path/to/installed/lunalauncher
Uses temporary profiles, an inert embedded JAR and a loopback metadata fixture.
"""
import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import subprocess
import tempfile
import threading
import zipfile

from api_sidecar_smoke import Sidecar


def main(executable):
    metadata_requested = threading.Event()

    class Metadata(BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass

        def do_GET(self):
            metadata_requested.set()
            self.send_response(404)
            self.end_headers()

    server = ThreadingHTTPServer(('127.0.0.1', 0), Metadata)
    server.daemon_threads = True
    threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        with tempfile.TemporaryDirectory(prefix='luna-pack-trust-') as folder:
            root = Path(folder)
            pack = root / 'untrusted.mrpack'
            with zipfile.ZipFile(pack, 'w') as archive:
                archive.writestr('modrinth.index.json', json.dumps({
                    'formatVersion': 1, 'game': 'minecraft', 'versionId': 'fixture',
                    'name': 'Trust fixture', 'files': [], 'dependencies': {'minecraft': '1.21.1'},
                }))
                archive.writestr('overrides/mods/fixture.jar', b'inert test fixture, never executed')

            def profile(name):
                path = root / name
                path.mkdir()
                (path / 'lunalauncher.cfg').write_text(
                    f'[General]\nLanguage=en_US\nMetaURLOverride=http://127.0.0.1:{server.server_port}/\n')
                return path

            noninteractive = subprocess.run(
                [str(executable), '--dir', str(profile('cli')), '--cli', '--json',
                 '--non-interactive', 'import', str(pack)],
                stdin=subprocess.DEVNULL, capture_output=True, text=True, timeout=30,
                env={**os.environ, 'QT_QPA_PLATFORM': 'offscreen'},
            )
            assert noninteractive.returncode != 0, noninteractive.stdout
            assert 'requires explicit confirmation' in (noninteractive.stdout + noninteractive.stderr), (
                noninteractive.stdout, noninteractive.stderr)
            print('PASS: non-interactive CLI refuses embedded mods without confirmation')

            for name, response in [('decline', {'value': 0}), ('cancel', {'cancel': True}), ('trust', {'value': 1})]:
                metadata_requested.clear()
                sidecar = Sidecar(executable, profile(name))
                try:
                    request = sidecar.start('instance.import', source=str(pack), name='Trust fixture')
                    prompt = sidecar.receive(lambda message: (
                        message.get('params', {}).get('requestId') == request
                        and message['params'].get('kind') == 'input'), timeout=15)['params']
                    assert 'mods/fixture.jar' in prompt['prompt'], prompt
                    assert [choice['id'] for choice in prompt['choices']] == ['cancel', 'trust'], prompt
                    assert all(choice['name'] for choice in prompt['choices']), prompt
                    sidecar.call('launcher/respond', {'interactionId': prompt['interactionId'], **response})
                    result = sidecar.response(request, timeout=30)['result']
                    # Even trusted imports cannot complete against the deliberate 404
                    # metadata fixture. Only explicit trust may reach that next stage.
                    assert result['ok'] is False, result
                    if name == 'trust':
                        assert metadata_requested.is_set(), result
                    else:
                        assert not metadata_requested.is_set(), result
                    assert sidecar.call('ping') == {}
                    print(f'PASS: sidecar {name} follows the explicit choice and remains responsive')
                finally:
                    sidecar.close()
    finally:
        server.shutdown()
        server.server_close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--exe', type=lambda value: Path(value).resolve(), required=True)
    main(parser.parse_args().exe)
