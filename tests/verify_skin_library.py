# SPDX-License-Identifier: GPL-3.0-only
"""Check local skin imports against an installed sidecar, without real accounts."""
import argparse
import json
from pathlib import Path
import struct
import tempfile
import zlib

from api_sidecar_smoke import Sidecar


def write_png(path, width=64, height=64):
    def chunk(kind, data):
        return (struct.pack('>I', len(data)) + kind + data
                + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff))
    path.write_bytes(b'\x89PNG\r\n\x1a\n'
                     + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0))
                     + chunk(b'IDAT', zlib.compress((b'\0' + b'\xff\x80\x40\xff' * width) * height))
                     + chunk(b'IEND', b''))


def verify(executable):
    with tempfile.TemporaryDirectory(prefix='luna-skin-library-') as temporary:
        root = Path(temporary)
        account = 'SkinFixture'
        (root / 'accounts.json').write_text(json.dumps({'formatVersion': 3, 'accounts': [{
            'type': 'MSA', 'active': True, 'msa-client-id': 'fixture',
            'msa': {'token': 'synthetic', 'refresh_token': 'synthetic'},
            'profile': {'id': '123456789abc4def8123456789abcdef', 'name': account,
                        'skin': {'id': '', 'url': '', 'variant': ''}, 'capes': []},
            'entitlement': {'ownsMinecraft': True, 'canPlayMinecraft': True},
        }]}), encoding='utf-8')
        source = root / '测试 皮肤.PNG'
        legacy = root / 'legacy.png'
        write_png(source)
        write_png(legacy, height=32)
        installed = []
        s = Sidecar(executable, root)
        try:
            for path, name in [(source, ''), (source, ''), (source, '中文名称'),
                               (source, 'Upper.PNG'), (legacy, '')]:
                result = s.ok('account.skin.install', account=account, path=str(path), name=name)
                destination = Path(result['path'])
                assert destination.is_file() and destination != path, result
                assert destination.suffix == '.png', result
                assert destination.read_bytes() == path.read_bytes(), result
                assert result['name'] == destination.stem, result
                assert destination not in installed, result
                installed.append(destination)
                # Import must update the index synchronously, before the next list request.
                index = json.loads((destination.parent / 'index.json').read_text(encoding='utf-8'))
                assert any(skin['name'] == destination.stem for skin in index['skins']), index
                listed = s.ok('account.skin.list', account=account)
                assert any(Path(skin['path']) == destination for skin in listed), listed
            print('PASS: modern/legacy PNG, Unicode, spaces, uppercase extension, duplicate names, immediate index/list')
            before = s.ok('account.skin.list', account=account)
            bad = root / 'invalid.png'
            bad.write_text('not an image', encoding='utf-8')
            wrong = root / 'wrong-size.png'
            write_png(wrong, width=128, height=128)
            corrupt = root / 'corrupt.png'
            corrupt.write_bytes(source.read_bytes()[:40])
            disguised = root / 'disguised.png'
            disguised.write_bytes(b'P6\n64 64\n255\n' + b'\xff\x80\x40' * 64 * 64)
            for path, name in [(bad, ''), (wrong, ''), (corrupt, ''), (disguised, ''),
                               (root, ''), (root / 'missing.png', ''),
                               (source, '../escape'), (source, 'sub\\escape'), (source, '.png')]:
                result = s.execute('account.skin.install', account=account, path=str(path), name=name)
                assert not result.get('ok'), result
                assert s.ok('account.skin.list', account=account) == before
            print('PASS: invalid/corrupt/disguised/wrong-size images and invalid names rejected without library changes')
        finally:
            s.close()
        s = Sidecar(executable, root)
        try:
            after = s.ok('account.skin.list', account=account)
            assert {Path(skin['path']) for skin in after} == set(installed), after
            assert all(skin['model'] == 'CLASSIC' for skin in after), after
            print('PASS: imported skins and metadata survive sidecar restart')
        finally:
            s.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, default=Path('install-webui-release/lunalauncher-cli.exe'))
    verify(parser.parse_args().exe.resolve())
