# SPDX-License-Identifier: GPL-3.0-only
"""Verify directory ZIP imports through a deployed CLI using isolated stores.

Optional --mite/--gtnh archives supply real profiles and selected runtime files.
This does not launch Minecraft or execute anything supplied by an archive.
"""
import argparse
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import io
import json
from pathlib import Path, PurePosixPath
import tempfile
import threading
import zipfile

from api_sidecar_smoke import Sidecar


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def sha(data):
    return hashlib.sha256(data).hexdigest()


def answer(s, index=None, choice_id=None):
    event = s.receive(lambda m: m.get('method') == 'launcher/event'
                      and m.get('params', {}).get('kind') == 'input')['params']
    if choice_id is not None:
        index = next(i for i, c in enumerate(event['choices']) if c.get('id') == choice_id)
    s.call('launcher/respond', {'interactionId': event['interactionId'], 'value': index, 'cancel': index is None})
    return event


def archive_subset(source, target, password=None):
    """Keep real version metadata, client, bootstrap, loader and native inputs."""
    clients = {}
    profiles = []
    with zipfile.ZipFile(source) as original, zipfile.ZipFile(target, 'w', zipfile.ZIP_STORED) as output:
        for info in original.infolist():
            if info.is_dir():
                continue
            p = PurePosixPath(info.filename)
            is_profile = p.suffix == '.json' and p.parent.parent.name == 'versions' and p.stem == p.parent.name
            is_client = p.suffix == '.jar' and p.parent.parent.name == 'versions' and p.stem == p.parent.name
            lower = info.filename.lower()
            keep = is_profile or is_client or (lower.endswith(('setup.ini', 'hmclversion.cfg')) and '/versions/' in lower)
            keep |= '/assets/indexes/' in lower or ('/assets/' in lower and '/versions/' in lower and is_profile)
            keep |= '/libraries/' in lower and lower.endswith('.jar') and any(x in lower for x in (
                'fishmodloader', 'mmc-bootstrap', 'retrofum', 'lwjgl3-3.4.3-natives-windows', 'lwjgl3ify'))
            if not keep:
                continue
            data = original.read(info, pwd=password)
            output.writestr(info.filename, data)
            if is_client:
                clients[p.stem] = sha(data)
            if is_profile and '/assets/' not in lower:
                profiles.append((p.stem, json.loads(data)))
    assert len(profiles) == 1, profiles
    version, profile = profiles[0]
    return version, profile, clients[version]


def main(args):
    hits = []
    index = b'{"objects":{}}'
    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            hits.append(self.path)
            data = index if self.path == '/assets.json' else b'official replacement must never be fetched'
            self.send_response(200)
            self.send_header('Content-Length', str(len(data)))
            self.end_headers()
            self.wfile.write(data)
        def log_message(self, *_):
            pass
    server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        with tempfile.TemporaryDirectory(prefix='luna-foreign-import-') as temporary:
            root = Path(temporary)
            url = f'http://127.0.0.1:{server.server_port}'
            jar = io.BytesIO()
            with zipfile.ZipFile(jar, 'w') as z:
                z.writestr('example/CustomMain.class', bytes.fromhex('cafebabe0000003d'))
            client = jar.getvalue()
            profile = {'id': 'AnotherCore', 'minecraftVersion': '1.20.1', 'mainClass': 'example.CustomMain',
                       'minecraftArguments': '--gameDir ${game_directory}', 'javaVersion': {'majorVersion': 17},
                       'downloads': {'client': {'url': url + '/client.jar', 'sha1': '0' * 40, 'size': 1}},
                       'assetIndex': {'id': 'fixture', 'url': url + '/assets.json', 'sha1': hashlib.sha1(index).hexdigest(),
                                      'size': len(index), 'totalSize': 0}, 'libraries': []}
            pack = root / 'custom.zip'
            with zipfile.ZipFile(pack, 'w') as z:
                z.writestr('.minecraft/versions/AnotherCore/AnotherCore.json', json.dumps(profile))
                z.writestr('.minecraft/versions/AnotherCore/AnotherCore.jar', client)
                z.writestr('.minecraft/versions/AnotherCore/config/example.cfg', 'preserve=true')
                z.writestr('.minecraft/assets/indexes/fixture.json', index)
                z.writestr('.minecraft/assets/asset_director/versions/fake/fake.json', '{"id":"fake"}')
            s = Sidecar(args.exe, root / 'data')
            try:
                prism = root / 'mmc'
                s.ok('instance-store.add', id='mmc', name='MMC', path=str(prism), format='prism', enabled=True, writable=True)
                # No vanilla store: fail before extracting or writing an instance.
                result = s.execute('instance.import', source=str(pack), targetFormat='vanilla-compatible', name='NoStore')
                assert not result['ok'], result
                for sid in ('raw-a', 'raw-b'):
                    s.ok('instance-store.add', id=sid, name=sid, path=str(root / sid), format='vanilla-compatible', enabled=True, writable=True)
                result = s.execute('instance.import', source=str(pack), targetStoreId='mmc', targetFormat='vanilla-compatible')
                assert not result['ok'], result
                request = s.start('instance.import', source=str(pack), name='Cancelled')
                answer(s)  # null cancels the format selection.
                assert not s.response(request)['result']['ok']
                assert not any(root.rglob('Cancelled.json'))
                # Every import asks again for format and then the matching store.
                for number, sid in enumerate(('raw-a', 'raw-b')):
                    request = s.start('instance.import', source=str(pack), name=f'Raw{number}')
                    answer(s, choice_id='vanilla-compatible')
                    answer(s, choice_id=sid)
                    result = s.response(request)['result']
                    assert result['ok'], result
                    data = result['data']
                    target = Path(data['root'])
                    assert target == root / sid / '.minecraft' / 'versions' / f'Raw{number}', target
                    active = read(target / f'Raw{number}.json')
                    assert active['jar'] == f'Raw{number}' and 'client' not in active.get('downloads', {})
                    assert sha((target / f'Raw{number}.jar').read_bytes()) == sha(client)
                    assert read(target / 'hmclversion.cfg')['gameDirType'] == 1
                    assert read(target / 'luna-imported/source-profile.json') == profile
                    assert not any((target / name).exists() for name in ('mmc-pack.json', 'instance.cfg', 'patches'))
                    s.ok('settings.set', scope='instance', instance=data['id'], key='notes', value='keep verification')
                    assert (target / 'luna-imported/client-artifact.json').exists()
                    (target / f'Raw{number}.jar').write_bytes(b'tampered')
                    hits.clear()
                    result = s.execute('instance.update', instance=data['id'])
                    assert not result['ok'] and '/client.jar' not in hits, (result, hits)
                    assert 'preserved' in result['error'].lower() or 'changed' in result['error'].lower(), result
                print('PASS: format/store selection on each import; empty vanilla stores; cancellation; raw integrity protection', flush=True)
                imported = s.ok('instance.import', source=str(pack), targetStoreId='mmc', name='CustomMMC')
                target = Path(imported['root'])
                runtime = read(target / 'patches/net.minecraft.json')
                local = target / 'libraries' / runtime['mainJar']['MMC-filename']
                assert sha(local.read_bytes()) == sha(client)
                assert runtime['mainClass'] == profile['mainClass'] and runtime['compatibleJavaMajors'] == [17]
                assert read(target / 'luna-imported/client-artifact.json')['comparison'] == 'differs-from-profile'
                for replacement in (b'tampered', None):
                    if replacement is None:
                        local.unlink()
                    else:
                        local.write_bytes(replacement)
                    hits.clear()
                    result = s.execute('instance.update', instance=imported['id'])
                    assert not result['ok'] and '/client.jar' not in hits, (result, hits)
                print('PASS: unrelated modified client preserved in MMC; tampered/missing client never downloads official replacement', flush=True)

                for kind, source, password, mc, java in (
                    ('MITE', args.mite, b'mod', '1.6.4', 17), ('GTNH', args.gtnh, None, '1.7.10', 25)):
                    if not source:
                        continue
                    subset = root / f'{kind}.zip'
                    vid, original, digest = archive_subset(source, subset, password)
                    for store in ('mmc', 'raw-a'):
                        data = s.ok('instance.import', source=str(subset), targetStoreId=store, name=f'{kind}-{store}')
                        dest = Path(data['root'])
                        record = read(dest / 'luna-imported/client-artifact.json')
                        assert record['sha256'] == digest
                        assert read(dest / 'luna-imported/source-profile.json') == original
                        assert sha((dest / record['path']).read_bytes()) == digest
                        info = s.ok('instance.info', instance=data['id'])
                        assert info['minecraftVersion'] == mc, info
                        if store == 'mmc':
                            patch = read(dest / 'patches/net.minecraft.json')
                            assert patch['compatibleJavaMajors'] == [java], patch.get('compatibleJavaMajors')
                            assert patch['mainClass'] == original['mainClass']
                            if kind == 'MITE':
                                assert 'UseLocalAssets=false' in (dest / 'instance.cfg').read_text()
                            else:
                                assert any('java.system.class.loader' in a for a in patch['+jvmArgs'])
                                native = dest / 'libraries/gtnh/natives/lwjgl3/3.4.3/lwjgl3-3.4.3-natives-windows.jar'
                                assert native.stat().st_size == 16483083
                                with zipfile.ZipFile(native) as z:
                                    assert len([n for n in z.namelist() if n.endswith('.dll')]) == 23
                        else:
                            active = read(dest / (dest.name + '.json'))
                            assert active['javaVersion']['majorVersion'] == java
                            assert active['mainClass'] == original['mainClass']
                            assert active.get('arguments') == original.get('arguments')
                            assert active.get('libraries') == original.get('libraries')
                        print(f'PASS: real {kind} -> {store}: MC {mc}, Java {java}, source/runtime/client preserved', flush=True)
            finally:
                s.close()
    finally:
        server.shutdown()
        server.server_close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', required=True, type=Path)
    parser.add_argument('--mite', type=Path)
    parser.add_argument('--gtnh', type=Path)
    args = parser.parse_args()
    args.exe = args.exe.resolve()
    main(args)
