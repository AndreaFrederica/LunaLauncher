# SPDX-License-Identifier: GPL-3.0-only
"""Verify nested directory packs, password retry/cancellation and client preservation.

Uses disposable profiles; never starts Minecraft or any bundled executable.
An optional real wrapper ZIP exercises encrypted inner packs in both store formats.
"""
import argparse
import hashlib
import io
import json
from pathlib import Path
import tempfile
import time
import zipfile

from api_sidecar_smoke import Sidecar


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def sha(data):
    return hashlib.sha256(data).hexdigest()


def zip_bytes(files):
    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, 'w', zipfile.ZIP_STORED) as archive:
        for name, data in files.items():
            archive.writestr(name, data)
    return buffer.getvalue()


def main(args):
    with tempfile.TemporaryDirectory(prefix='luna-nested-import-') as temporary:
        root = Path(temporary)
        profile = root / 'data'
        profile.mkdir()
        (profile / 'lunalauncher.cfg').write_text(
            '[General]\nLanguage=en_US\nLocalServiceCache=false\n', encoding='utf-8')
        s = Sidecar(args.exe.resolve(), profile)
        try:
            for fmt in ('prism', 'vanilla-compatible'):
                s.ok('instance-store.add', id=fmt, name=fmt, path=str(root / fmt),
                     format=fmt, enabled=True, writable=True)

            def import_pack(path, name, fmt='prism', passwords=(), background=False):
                if background:
                    started = s.call('launcher/execute', {'operation': 'neo.task.start', 'parameters': {
                        'operation': 'instance.import', 'parameters': {
                            'source': str(path), 'targetStoreId': fmt, 'name': name}}})
                    assert started['ok'], started
                    task = started['data']
                    deadline = time.monotonic() + 120
                    remaining = iter(passwords)
                    while time.monotonic() < deadline:
                        task = s.ok('neo.task.status', taskId=task['taskId'])
                        interaction = task.get('interaction') or {}
                        if interaction.get('interactionId'):
                            assert interaction.get('secret'), interaction
                            s.ok('neo.task.respond', taskId=task['taskId'],
                                 interactionId=interaction['interactionId'], value=next(remaining))
                        if task['finished']:
                            assert task['state'] == 'succeeded', task
                            dest = root / fmt / name if fmt == 'prism' else root / fmt / '.minecraft/versions' / name
                            return {'ok': True, 'data': {'root': str(dest), 'id': task['createdInstanceId']}}
                        time.sleep(.1)
                    s.ok('neo.task.cancel', taskId=task['taskId'])
                    raise AssertionError(task)
                request = s.start('instance.import', source=str(path), targetStoreId=fmt, name=name)
                for password in passwords:
                    event = s.receive(lambda m: m.get('method') == 'launcher/event'
                                      and m.get('params', {}).get('kind') == 'input')['params']
                    assert event.get('secret'), event
                    assert args.archive is not None
                    assert 'MITE' in json.dumps(event, ensure_ascii=False), event
                    s.call('launcher/respond', {'interactionId': event['interactionId'], 'value': password,
                                               'cancel': password is None})
                return s.response(request, timeout=120)['result']

            metadata = {'id': 'Custom', 'minecraftVersion': '1.20.1', 'mainClass': 'example.CustomMain',
                        'minecraftArguments': '--gameDir ${game_directory}', 'libraries': [],
                        'javaVersion': {'majorVersion': 17}}
            client = zip_bytes({'example/CustomMain.class': bytes.fromhex('cafebabe0000003d')})
            directory = {'.minecraft/versions/Custom/Custom.json': json.dumps(metadata),
                         '.minecraft/versions/Custom/Custom.jar': client,
                         '.minecraft/versions/Custom/config/preserved.txt': b'config',
                         'Resources.zip': zip_bytes({'pack.mcmeta': b'{}'})}
            wrapper = root / 'wrapper.zip'
            wrapper.write_bytes(zip_bytes({'Readme.txt': b'wrapper', 'Custom pack.zip': zip_bytes(directory)}))
            data = import_pack(wrapper, 'NestedDirectory')
            assert data['ok'], data
            dest = Path(data['data']['root'])
            record = read(dest / 'luna-imported/client-artifact.json')
            assert sha((dest / record['path']).read_bytes()) == sha(client)
            assert (dest / 'minecraft/config/preserved.txt').read_bytes() == b'config'
            print('PASS: arbitrarily named nested directory pack uses shared conversion', flush=True)

            direct = root / 'direct.zip'
            direct.write_bytes(zip_bytes({**directory, 'Other.zip': b'not-a-game-pack'}))
            data = import_pack(direct, 'DirectPriority')
            assert data['ok'], data
            print('PASS: direct game profile takes priority over bundled resource ZIPs', flush=True)

            mmc = zip_bytes({'instance.cfg': '[General]\nInstanceType=OneSix\nname=NestedMMC\n',
                             'mmc-pack.json': '{"formatVersion":1,"components":[]}',
                             'minecraft/config/marker.txt': b'MMC'})
            wrapper.write_bytes(zip_bytes({'Renamed.zip': mmc}))
            data = import_pack(wrapper, 'NestedMMC')
            assert data['ok'], data
            assert (Path(data['data']['root']) / 'minecraft/config/marker.txt').read_bytes() == b'MMC'
            print('PASS: nested MMC pack uses the existing MMC importer', flush=True)

            for name, files, message in (
                ('Multiple', {'First.zip': mmc, 'Second.zip': mmc}, '多个'),
                ('TooDeep', {'One.zip': zip_bytes({'Two.zip': mmc})}, '嵌套'),
                ('Unrecognized', {'Unknown.zip': zip_bytes({'readme.txt': b'no game'})}, 'Unknown.zip'),
            ):
                wrapper.write_bytes(zip_bytes(files))
                data = import_pack(wrapper, name)
                assert not data['ok'] and message in data['error'], data
                assert not (root / 'prism' / name).exists()
            print('PASS: ambiguous, excessive and invalid nesting fail with actionable errors', flush=True)

            if args.archive:
                before = sha(args.archive.read_bytes())
                with zipfile.ZipFile(args.archive) as outer:
                    inner_name = next(n for n in outer.namelist() if n.endswith('.zip'))
                    with zipfile.ZipFile(io.BytesIO(outer.read(inner_name))) as inner:
                        version_json = next(n for n in inner.namelist()
                                            if '/versions/' in n and n.endswith('/1.6.4-MITE.json'))
                        version_root = version_json.rsplit('/', 1)[0] + '/'
                        original = json.loads(inner.read(version_json, pwd=args.password.encode()))
                        original_client = inner.read(version_root + '1.6.4-MITE.jar', pwd=args.password.encode())
                        mods = {n[len(version_root):]: sha(inner.read(n, pwd=args.password.encode()))
                                for n in inner.namelist()
                                if n.startswith(version_root + 'mods/') and not n.endswith('/')}
                data = import_pack(args.archive, 'PasswordCancelled', passwords=(None,))
                assert not data['ok'], data
                assert not (root / 'prism/PasswordCancelled').exists()
                print('PASS: cancelling the real inner ZIP password leaves no instance', flush=True)
                for fmt in ('prism', 'vanilla-compatible'):
                    passwords = ('wrong-password', args.password) if fmt == 'prism' else (args.password,)
                    data = import_pack(args.archive, 'MITE-' + fmt, fmt, passwords, background=fmt == 'vanilla-compatible')
                    assert data['ok'], data
                    dest = Path(data['data']['root'])
                    game = dest / 'minecraft' if fmt == 'prism' else dest
                    record = read(dest / 'luna-imported/client-artifact.json')
                    assert sha((dest / record['path']).read_bytes()) == sha(original_client)
                    assert read(dest / 'luna-imported/source-profile.json') == original
                    for path, digest in mods.items():
                        assert sha((game / path).read_bytes()) == digest, path
                    info = s.ok('instance.info', instance=data['data']['id'])
                    assert info['minecraftVersion'] == '1.6.4', info
                    assert info['loaderType'] == 'fishmodloader', info
                    assert info['loaderVersion'] == 'v3.4.4', info
                    if fmt == 'prism':
                        patch = read(dest / 'patches/net.minecraft.json')
                        assert patch['mainClass'] == original['mainClass']
                        assert patch['compatibleJavaMajors'] == [17], patch
                        assert (dest / 'mmc-pack.json').exists()
                    else:
                        active = read(dest / (dest.name + '.json'))
                        assert active['mainClass'] == original['mainClass']
                        assert active['javaVersion']['majorVersion'] == 17
                        assert not (dest / 'mmc-pack.json').exists()
                        assert read(dest / 'hmclversion.cfg')['gameDirType'] == 1
                    print(f'PASS: full encrypted MITE wrapper -> {fmt}; MC 1.6.4, FML v3.4.4, '
                          f'Java 17, original client and all {len(mods)} mod files preserved', flush=True)
                    if fmt == 'vanilla-compatible':
                        print('PASS: NeoUI background task exposes the password interaction and completes import', flush=True)
                assert sha(args.archive.read_bytes()) == before
                assert not list((root / 'prism/.tmp').glob('*'))
        finally:
            s.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', required=True, type=Path)
    parser.add_argument('--archive', type=Path)
    parser.add_argument('--password', default='mod')
    main(parser.parse_args())
