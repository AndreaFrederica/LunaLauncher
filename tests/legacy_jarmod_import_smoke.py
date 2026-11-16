# SPDX-License-Identifier: GPL-3.0-only
"""Import legacy class-overlay installers through the deployed CLI in isolated stores.

The optional real archive is inspected and imported, never executed. A local
metadata mirror supplies a hash-verified official base for the real MITE case.
"""
import argparse
from collections import deque
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import io
import json
from pathlib import Path
import tempfile
import threading
import time
import urllib.request
import zipfile

from api_sidecar_smoke import Sidecar


def jar(files):
    output = io.BytesIO()
    with zipfile.ZipFile(output, 'w') as archive:
        for name, data in files.items():
            archive.writestr(name, data)
    return output.getvalue()


def digest(data):
    return hashlib.sha256(data).hexdigest()


def main(args):
    real_meta = None
    if args.archive:
        with urllib.request.urlopen('https://meta.prismlauncher.org/v1/net.minecraft/1.6.4.json', timeout=30) as response:
            real_meta = json.load(response)
        artifact = real_meta['mainJar']['downloads']['artifact']
        with urllib.request.urlopen(artifact['url'], timeout=45) as response:
            real_base = response.read()
        assert hashlib.sha1(real_base).hexdigest() == artifact['sha1'] and len(real_base) == artifact['size']
    base = jar({'net/minecraft/client/main/Main.class': bytes.fromhex('cafebabe00000033') + b'base-main',
                'example/Replace.class': bytes.fromhex('cafebabe00000033') + b'old',
                'keep.txt': b'unchanged-base-content', 'META-INF/TEST.SF': b'remove-signature'})
    state = {'base': base, 'real': False, 'delay': False, 'missing_integrity': False, 'entered': threading.Event()}
    hits = []

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass

        def do_GET(self):
            hits.append(self.path)
            if self.path == '/client.jar':
                state['entered'].set()
                if state['delay']:
                    time.sleep(2)
                data = state['base']
            elif self.path.endswith(('/net.minecraft/1.6.4.json', '/net.minecraft/1.7.10.json')):
                data = dict(real_meta) if state['real'] else {
                    'formatVersion': 1, 'name': 'Minecraft', 'uid': 'net.minecraft', 'version': self.path.rsplit('/', 1)[1][:-5],
                    'releaseTime': '2013-09-19T15:52:37Z',
                    'mainClass': 'net.minecraft.client.main.Main', 'libraries': [],
                    'compatibleJavaMajors': [8], 'minecraftArguments': '--gameDir ${game_directory}',
                    'assetIndex': {'id': 'fixture', 'url': origin + '/assets.json', 'sha1': '0' * 40, 'size': 14, 'totalSize': 0},
                    'mainJar': {'name': 'com.mojang:minecraft:1.6.4:client', 'downloads': {'artifact': {}}}}
                data['mainJar']['downloads']['artifact'] = {
                    'url': origin + '/client.jar', 'sha1': hashlib.sha1(state['base']).hexdigest(), 'size': len(state['base'])}
                if state['missing_integrity']:
                    data['mainJar']['downloads']['artifact']['sha1'] = ''
            elif self.path.endswith('/net.minecraft/index.json'):
                data = {'formatVersion': 1, 'uid': 'net.minecraft', 'name': 'Minecraft',
                        'versions': [{'version': '1.6.4', 'type': 'release', 'releaseTime': '2013-09-19T15:52:37Z'}]}
            elif self.path.endswith('/index.json'):
                data = {'formatVersion': 1, 'packages': [{'uid': 'net.minecraft', 'name': 'Minecraft'}]}
            else:
                self.send_error(404)
                return
            if isinstance(data, dict):
                data = json.dumps(data).encode()
            self.send_response(200)
            self.send_header('Content-Length', str(len(data)))
            self.send_header('Connection', 'close')
            self.end_headers()
            try:
                self.wfile.write(data)
            except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
                pass

    server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    origin = f'http://127.0.0.1:{server.server_port}'
    threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        with tempfile.TemporaryDirectory(prefix='luna-legacy-jarmod-') as temporary:
            root = Path(temporary)
            source = root / 'installer.zip'
            profile = {'id': '1.6.4-Example', 'mainClass': 'net.minecraft.client.main.Main', 'libraries': [],
                       'minecraftArguments': '--gameDir ${game_directory} --assetsDir ${game_assets}'}
            overlay = {'example/Replace.class': bytes.fromhex('cafebabe00000033') + b'new',
                       'example/Added.class': bytes.fromhex('cafebabe00000033') + b'added', 'example/data.txt': b'patch-resource'}
            resource = jar({'pack.mcmeta': b'{"pack":{"pack_format":1,"description":"fixture"}}'})

            def write_pack(path, metadata=profile, replacements=overlay, prefix='Wrapper/'):
                version = metadata['id']
                with zipfile.ZipFile(path, 'w') as archive:
                    folder = prefix + version + '/'
                    archive.writestr(folder + version + '.json', json.dumps(metadata))
                    for name, data in replacements.items():
                        archive.writestr(folder + 'class/' + name, data)
                    archive.writestr(folder + 'Resources.zip', resource)
                    archive.writestr(prefix + 'Setup.exe', b'installer-must-not-execute-or-be-copied')

            def connect(name):
                data = root / name
                data.mkdir(parents=True, exist_ok=True)
                (data / 'lunalauncher.cfg').write_text(
                    '[General]\nLanguage=en_US\nLocalServiceCache=false\nDownloadBackend=0\n'
                    f'MetaURLOverride={origin}/\n', encoding='utf-8')
                s = Sidecar(args.exe.resolve(), root / name)
                s.stderr = deque(maxlen=2000)
                s.ok('settings.set', scope='launcher', key='MetaURLOverride', value=origin + '/')
                s.ok('settings.set', scope='launcher', key='LocalServiceCache', value=False)
                s.ok('settings.set', scope='launcher', key='DownloadBackend', value=0)
                for fmt in ('prism', 'vanilla-compatible'):
                    s.ok('instance-store.add', id=fmt, name=fmt, path=str(root / name / fmt), format=fmt, enabled=True, writable=True)
                return s

            def verify(s, archive_path, fmt, name, expected, original, resource_name, resource_bytes):
                before = digest(archive_path.read_bytes())
                result = s.execute('instance.import', source=str(archive_path), targetStoreId=fmt, name=name)
                assert result['ok'], (result, ''.join(s.stderr))
                result = result['data']
                target = Path(result['root'])
                game = target / 'minecraft' if fmt == 'prism' else target
                info = s.ok('instance.info', instance=result['id'])
                assert info['minecraftVersion'] == '1.6.4', info
                record = json.loads((game / 'luna-installer/install-report.json').read_text())
                if fmt == 'prism':
                    patch = json.loads((target / 'patches/net.minecraft.json').read_text())
                    client = target / 'libraries' / patch['mainJar']['MMC-filename']
                    assert patch['compatibleJavaMajors'] == [8], patch
                else:
                    config = json.loads((target / (name + '.json')).read_text())
                    client = target / (name + '.jar')
                    assert config['jar'] == name and 'client' not in config.get('downloads', {})
                    assert not any((target / p).exists() for p in ('instance.cfg', 'mmc-pack.json', 'patches'))
                    assert json.loads((target / 'hmclversion.cfg').read_text())['gameDirType'] == 1
                assert digest(client.read_bytes()) == record['client']['sha256']
                with zipfile.ZipFile(client) as generated:
                    assert generated.testzip() is None
                    assert not any(n.startswith('META-INF/') for n in generated.namelist())
                    assert 'net/minecraft/client/main/Main.class' in generated.namelist()
                    for path, data in expected.items():
                        assert generated.read(path) == data, path
                    with zipfile.ZipFile(io.BytesIO(state['base'])) as original_base:
                        for path in original_base.namelist():
                            if path not in expected and not path.startswith('META-INF/'):
                                assert generated.read(path) == original_base.read(path), path
                assert (game / 'resourcepacks' / resource_name).read_bytes() == resource_bytes
                assert json.loads((game / 'luna-installer/source-profile.json').read_text()) == original
                assert not list(target.rglob('*.exe'))
                assert not list(target.rglob('Setup.command'))
                assert digest(archive_path.read_bytes()) == before
                print('PASS:', name, fmt, len(expected), 'patch entries; base contents, signatures, resource pack, identity and source verified', flush=True)

            write_pack(source)
            s = connect('fixture')
            try:
                for fmt in ('prism', 'vanilla-compatible'):
                    verify(s, source, fmt, 'Example-' + fmt, overlay, profile, 'Resources.zip', resource)
                # A different profile name and a versions/ wrapper use the same generic handler.
                custom = {**profile, 'id': 'OtherCore', 'minecraftVersion': '1.6.4'}
                renamed = root / 'renamed.zip'
                write_pack(renamed, custom, prefix='.minecraft/versions/')
                verify(s, renamed, 'prism', 'OtherCore', overlay, custom, 'Resources.zip', resource)
                for label, metadata, replacements, message in (
                    ('conflicting', {**profile, 'minecraftVersion': '1.6.4', 'inheritsFrom': '1.7.10'}, overlay, '冲突'),
                    ('unknown-base', {**profile, 'id': 'UnspecifiedCore'}, overlay, '基础'),
                    ('invalid-class', profile, {'Broken.class': b'not-a-class'}, 'class')):
                    pack = root / (label + '.zip')
                    write_pack(pack, metadata, replacements)
                    result = s.execute('instance.import', source=str(pack), targetStoreId='prism', name=label)
                    assert not result['ok'] and message in result['error'], result
                    assert not (root / 'fixture/prism' / label).exists()
                    assert not list((root / 'fixture/prism/.tmp').glob('*'))
                print('PASS: conflicting/unknown base and invalid class fail without committing an instance', flush=True)
                state['missing_integrity'] = True
                unsafe = root / 'missing-integrity.zip'
                write_pack(unsafe, {**profile, 'id': '1.7.10-Unsafe', 'minecraftVersion': '1.7.10'})
                downloads = hits.count('/client.jar')
                result = s.execute('instance.import', source=str(unsafe), targetStoreId='prism', name='UnsafeBase')
                assert not result['ok'] and 'SHA-1' in result['error'], result
                assert hits.count('/client.jar') == downloads, 'Downloaded a base without trusted integrity metadata'
                assert not (root / 'fixture/prism/UnsafeBase').exists()
                assert not list((root / 'fixture/prism/.tmp').glob('*'))
                print('PASS: missing base integrity stops before download and removes staging', flush=True)
                state['missing_integrity'] = False
                state['delay'] = True
                state['entered'].clear()
                started = s.call('launcher/execute', {'operation': 'neo.task.start', 'parameters': {
                    'operation': 'instance.import', 'parameters': {
                        'source': str(source), 'targetStoreId': 'prism', 'name': 'Cancelled'}}})
                assert started['ok'], started
                task = started['data']
                assert state['entered'].wait(15), 'Base download did not start'
                s.ok('neo.task.cancel', taskId=task['taskId'])
                deadline = time.monotonic() + 15
                while not task['finished']:
                    assert time.monotonic() < deadline, task
                    time.sleep(0.1)
                    task = s.ok('neo.task.status', taskId=task['taskId'])
                assert task['state'] == 'cancelled', task
                assert not (root / 'fixture/prism/Cancelled').exists()
                assert not list((root / 'fixture/prism/.tmp').glob('*'))
                print('PASS: base-download cancellation responds and removes staging', flush=True)
            finally:
                s.close()
                state['delay'] = False
            if args.archive:
                state['real'] = True
                state['base'] = real_base
                with zipfile.ZipFile(args.archive) as archive:
                    name = next(n for n in archive.namelist() if n.endswith('.json'))
                    original = json.loads(archive.read(name))
                    folder = name.rsplit('/', 1)[0] + '/'
                    expected = {n[len(folder + 'class/'):]: archive.read(n) for n in archive.namelist()
                                if n.startswith(folder + 'class/') and not n.endswith('/')}
                    rp = next(n for n in archive.namelist() if n.startswith(folder) and n.endswith('.zip'))
                    resource_bytes = archive.read(rp)
                s = connect('real-mite')
                try:
                    for fmt in ('prism', 'vanilla-compatible'):
                        verify(s, args.archive, fmt, 'MITE-R196-' + fmt, expected, original, rp.rsplit('/', 1)[1], resource_bytes)
                finally:
                    s.close()
    finally:
        server.shutdown()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--archive', type=Path)
    main(parser.parse_args())
