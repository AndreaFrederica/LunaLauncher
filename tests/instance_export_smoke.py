# SPDX-License-Identifier: GPL-3.0-only
"""Check real ZIP contents and hash resolution through both export API paths.

Uses a disposable instance and local platform API fixtures; no public network needed.
Run after pixi run install-webui release:
  pixi run python tests/instance_export_smoke.py --exe install-webui-release/lunalauncher-cli.exe
"""
import argparse
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import io
import json
from pathlib import Path
import tempfile
import threading
import time
import zipfile

from api_sidecar_smoke import Sidecar


def main(executable):
    with tempfile.TemporaryDirectory(prefix='luna-export-test-') as temporary:
        root = Path(temporary)
        profile = root / 'data'
        instance = profile / 'instances/export-fixture'
        game = instance / '.minecraft'
        game.mkdir(parents=True)
        (instance / 'instance.cfg').write_text('[General]\nInstanceType=OneSix\nname=Export fixture\n', encoding='utf-8')
        (instance / 'mmc-pack.json').write_text('{"formatVersion":1,"components":[]}', encoding='utf-8')
        (instance / 'patches').mkdir()
        (instance / 'patches/local.json').write_text('{}', encoding='utf-8')
        jar = io.BytesIO()
        with zipfile.ZipFile(jar, 'w') as archive:
            archive.writestr('fabric.mod.json', json.dumps({'schemaVersion': 1, 'id': 'export_fixture', 'version': '1.0', 'name': 'Export fixture'}))
        mod_bytes = jar.getvalue()
        sha512 = hashlib.sha512(mod_bytes).hexdigest()
        sha1 = hashlib.sha1(mod_bytes).hexdigest()
        for name, data in {
            'mods/fixture.jar': mod_bytes, 'config/keep.json': b'keep', 'config/dev/junk.txt': b'dev',
            'config/dev/keep.txt': b'explicit', 'options.txt': b'options', 'saves/world/level.dat': b'world',
            'screenshots/screenshot.png': b'screenshot', 'shaderpacks/local.txt': b'shader',
            'resourcepacks/local.txt': b'resource', 'servers.dat': b'servers', '.git/objects/blob': b'git',
            '.venv/Lib/junk.py': b'venv', '.hidden-settings': b'hidden', 'logs/latest.log': b'log', 'custom-folder/needed.txt': b'custom',
        }.items():
            path = game / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        state = {'matches': True, 'requests': []}

        class Handler(BaseHTTPRequestHandler):
            def log_message(self, *args):
                pass

            def reply(self, data):
                data = json.dumps(data).encode()
                self.send_response(200)
                self.send_header('Content-Type', 'application/json')
                self.send_header('Content-Length', str(len(data)))
                self.end_headers()
                self.wfile.write(data)

            def do_POST(self):
                body = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
                state['requests'].append((self.path, body))
                if self.path == '/mr/version_files':
                    assert body['algorithm'] == 'sha512'
                    assert body['hashes'] == [sha512], body
                    self.reply({sha512: {'files': [{'hashes': {'sha512': sha512, 'sha1': sha1},
                        'url': 'https://cdn.modrinth.com/data/fixture/versions/1/fixture.jar', 'size': len(mod_bytes)}]}}
                        if state['matches'] else {})
                elif self.path == '/cf/fingerprints':
                    self.reply({'data': {'exactMatches': [{'file': {'fileFingerprint': int(fingerprint),
                        'modId': 123, 'id': 456, 'isAvailable': True}} for fingerprint in body['fingerprints']]
                        if state['matches'] else []}})
                else:
                    self.reply({})

            def do_GET(self):
                self.reply({'data': {'id': 123, 'name': 'Fixture', 'slug': 'fixture'}})

        server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        server.daemon_threads = True
        threading.Thread(target=server.serve_forever, daemon=True).start()
        port = server.server_port
        (profile / 'lunalauncher.cfg').write_text('[General]\nLanguage=en_US\nModrinthMirror=2\n'
            f'ModrinthMirrorURL=http://127.0.0.1:{port}/mr\nCurseForgeMirror=2\n'
            f'CurseForgeMirrorURL=http://127.0.0.1:{port}/cf\n', encoding='utf-8')
        sidecar = Sidecar(executable.resolve(), profile)
        sequence = 0

        def export(fmt='zip', background=False, expected=True, output=None, **extra):
            nonlocal sequence
            sequence += 1
            output = output or root / f'export-{sequence}.zip'
            parameters = dict(instance='export-fixture', output=str(output), format=fmt,
                              name='Fixture', version='1.0', **extra)
            if background:
                started = sidecar.call('launcher/execute', {'operation': 'neo.task.start', 'parameters': {'operation': 'instance.export', 'parameters': parameters}})
                assert started['ok'], started
                result = started['data']
                deadline = time.monotonic() + 30
                while not result['finished'] and time.monotonic() < deadline:
                    time.sleep(.05)
                    result = sidecar.ok('neo.task.status', taskId=result['taskId'])
                assert result['finished'], result
                success = result['state'] == 'succeeded'
            else:
                result = sidecar.execute('instance.export', **parameters)
                success = result['ok']
            assert success == expected, result
            if expected:
                with zipfile.ZipFile(output) as archive:
                    return {name: archive.read(name) for name in archive.namelist() if not name.endswith('/')}
            return result

        try:
            listing = sidecar.ok('instance.export-files', instance='export-fixture', path='.minecraft')
            entries = {entry['name']: entry for entry in listing['entries']}
            assert not entries['.git']['defaultSelected'] and not entries['.venv']['defaultSelected']
            assert entries['mods']['category'] == 'mod'
            assert not sidecar.execute('instance.export-files', instance='export-fixture', path='../outside')['ok']
            for background in (False, True):
                contents = export(items=['mod'], background=background)
                assert set(contents) == {'.minecraft/mods/fixture.jar'}, contents.keys()
                assert not export(items=[], background=background)
                for fmt in ('modrinth', 'curseforge'):
                    contents = export(fmt, background=background, items=['mod'], modMode='bundled')
                    assert contents['overrides/mods/fixture.jar'] == mod_bytes
                    assert not any(name.startswith('overrides/') and name != 'overrides/mods/fixture.jar' for name in contents)
                    manifest = json.loads(contents['modrinth.index.json' if fmt == 'modrinth' else 'manifest.json'])
                    assert manifest['files'] == []
            core = export(items=['game'])
            assert 'patches/local.json' in core and 'instance.cfg' not in core, core.keys()
            print('PASS: content categories work in ZIP/MR/CF through direct and background APIs', flush=True)
            rules = {'.minecraft/config': True, '.minecraft/config/dev': False,
                     '.minecraft/config/dev/keep.txt': True, '.minecraft/custom-folder': True, '.minecraft/.hidden-settings': True}
            for fmt in ('zip', 'modrinth', 'curseforge'):
                contents = export(fmt, fileSelection=rules, modMode='bundled')
                prefix = '.minecraft/' if fmt == 'zip' else 'overrides/'
                assert set(name for name in contents if name.startswith(prefix)) == {
                    prefix + name for name in ('config/keep.json', 'config/dev/keep.txt', 'custom-folder/needed.txt', '.hidden-settings')}
            print('PASS: nested exclusion, reselected descendant and custom folder produce exact archive contents', flush=True)
            assert not state['requests'], state['requests']
            stale = game / 'mods/.index/stale.pw.toml'
            stale.parent.mkdir(parents=True)
            stale.write_text('name = "Stale binding"\nfilename = "fixture.jar"\nside = "both"\n'
                '[download]\nmode = "url"\nurl = "https://cdn.modrinth.com/data/wrong/wrong.jar"\n'
                'hash-format = "sha1"\nhash = "0000000000000000000000000000000000000000"\n'
                '[update.curseforge]\nproject-id = 999\nfile-id = 999\n', encoding='utf-8')
            for fmt in ('modrinth', 'curseforge'):
                contents = export(fmt, items=['mod'], modMode='links')
                assert 'overrides/mods/fixture.jar' not in contents
                manifest = json.loads(contents['modrinth.index.json' if fmt == 'modrinth' else 'manifest.json'])
                assert len(manifest['files']) == 1
                if fmt == 'modrinth':
                    assert manifest['files'][0]['hashes'] == {'sha1': sha1, 'sha512': sha512}
                else:
                    assert manifest['files'][0] == {'projectID': 123, 'fileID': 456, 'required': True}
            print('PASS: strict links resolve actual file hashes and omit bundled mod bytes', flush=True)
            state['matches'] = False
            for fmt in ('modrinth', 'curseforge'):
                output = root / f'preserved-{fmt}.zip'
                output.write_bytes(b'previous export')
                failure = export(fmt, background=True, items=['mod'], modMode='links', expected=False,
                                 output=output, overwrite=True)
                assert 'mods/fixture.jar' in failure['error'], failure
                assert output.read_bytes() == b'previous export'
                contents = export(fmt, items=['mod'], modMode='auto')
                assert contents['overrides/mods/fixture.jar'] == mod_bytes
            print('PASS: missing links fail with filenames, preserve existing output, and auto mode bundles unmatched mods', flush=True)
        finally:
            sidecar.close()
            server.shutdown()
            server.server_close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--exe', type=Path, required=True)
    main(parser.parse_args().exe)
