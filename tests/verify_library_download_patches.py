# SPDX-License-Identifier: GPL-3.0-only
"""Exercise bundled natives and opt-in URL patches against a deployed CLI."""
import argparse
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import io
import json
from pathlib import Path
import tempfile
import threading
import zipfile

from api_sidecar_smoke import Sidecar


def main(executable):
    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, 'w') as archive:
        archive.writestr('fixture.dll', b'synthetic native; never executed')
    payload = buffer.getvalue()
    index = b'{"objects":{}}'
    hits = []

    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            hits.append(self.path)
            body = index if self.path == '/assets.json' else payload
            if self.path.startswith('/missing/'):
                self.send_response(404)
                self.end_headers()
                return
            if self.path == '/wrong.jar':
                body = b'invalid content with the wrong checksum'
            self.send_response(200)
            self.send_header('Content-Length', str(len(body)))
            self.send_header('Content-Type', 'application/octet-stream')
            self.end_headers()
            self.wfile.write(body)

        def log_message(self, *_):
            pass

    server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    origin = f'http://127.0.0.1:{server.server_port}'
    relative = Path('gtnh/natives/lwjgl3/3.4.3/lwjgl3-3.4.3-natives-windows.jar')

    def put(path, data):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data if isinstance(data, bytes) else json.dumps(data).encode())

    def seed(root, native=None, bundled=False):
        inst = root / 'instances/native'
        inst.mkdir(parents=True)
        (inst / 'instance.cfg').write_text('[General]\nInstanceType=OneSix\nname=Native fixture\nOverrideJavaLocation=true\nJavaArchitecture=64\nJavaRealArchitecture=amd64\n', encoding='utf-8')
        put(inst / 'mmc-pack.json', {'formatVersion': 1, 'components': [{'uid': 'net.minecraft', 'version': '1.7.10'}]})
        # No explicit URL, exactly like the reported pack. Redirect only the
        # default Maven repository to our local HTTP fixture.
        (root / 'lunalauncher.cfg').write_text('[General]\nLibrariesURL=' + origin + '/missing/\n', encoding='utf-8')
        library = native or {'name': 'gtnh.natives:lwjgl3:3.4.3', 'natives': {'windows': 'natives-windows'}}
        profile = {'formatVersion': 1, 'uid': 'net.minecraft', 'version': '1.7.10',
                   'mainClass': 'net.minecraft.client.main.Main', 'minecraftArguments': '--gameDir ${game_directory}',
                   'libraries': [library], 'mainJar': {'name': 'example:client:1', 'MMC-hint': 'local', 'MMC-filename': 'main.jar'},
                   'assetIndex': {'id': 'fixture', 'url': origin + '/assets.json', 'sha1': hashlib.sha1(index).hexdigest(), 'size': len(index), 'totalSize': 0}}
        put(inst / 'patches/net.minecraft.json', profile)
        put(inst / 'libraries/main.jar', payload)
        put(root / 'assets/indexes/fixture.json', index)
        if bundled:
            put(inst / 'libraries' / relative, payload)
        return inst

    def rules(root, urls, mode='fallback', **match):
        put(root / 'library-download-patches.overlay.json5', {'formatVersion': 1, 'rules': [
            {'id': 'gtnh-lwjgl3-3.4.3-windows', 'match': match or {'library': 'gtnh.natives:lwjgl3:3.4.3', 'classifier': 'natives-windows'},
             'mode': mode, 'urls': urls}]})

    try:
        with tempfile.TemporaryDirectory(prefix='luna-library-patches-') as temporary:
            temp = Path(temporary)
            for backend in (0, 2):
                root = temp / f'backend-{backend}'
                seed(root)
                rules(root, [origin + '/fallback.jar'])
                s = Sidecar(executable, root)
                try:
                    s.ok('settings.get', scope='instance', instance='native', key='JavaArchitecture')
                    s.ok('instance.info', instance='native')
                    for key, value in [('DownloadBackend', backend), ('NumberOfManualRetries', 0)]:
                        s.ok('settings.set', scope='launcher', key=key, value=value)
                    settings = s.ok('settings.get', scope='launcher', key='LibraryDownloadPatchesEnabled')
                    assert settings.get('value') is False, settings
                    config = s.ok('resource.library-patches')
                    assert not config['errors'] and Path(config['overlayPath']) == root / 'library-download-patches.overlay.json5', config
                    hits.clear()
                    result = s.execute('instance.update', instance='native')
                    assert not result['ok'], result
                    assert any(p.startswith('/missing/') for p in hits) and '/fallback.jar' not in hits, (result, hits, list(s.stderr))
                    s.ok('settings.set', scope='launcher', key='LibraryDownloadPatchesEnabled', value=True)
                    hits.clear()
                    result = s.execute('instance.update', instance='native')
                    assert result['ok'], (result, hits, list(s.stderr))
                    assert any(p.startswith('/missing/') for p in hits) and '/fallback.jar' in hits, hits
                    assert (root / 'libraries' / relative).read_bytes() == payload
                    hits.clear()
                    assert s.execute('instance.update', instance='native')['ok']
                    assert '/fallback.jar' not in hits and not any(p.startswith('/missing/') for p in hits), hits
                    s.ok('settings.set', scope='launcher', key='LibraryDownloadPatchesEnabled', value=False)
                    print(f'PASS backend {backend}: disabled by default; failed primary -> overlay fallback; expected destination')
                finally:
                    s.close()
                s = Sidecar(executable, root)
                try:
                    settings = s.ok('settings.get', scope='launcher', key='LibraryDownloadPatchesEnabled')
                    assert settings.get('value') is False, settings
                    print(f'PASS backend {backend}: cache reused; disabled setting persists as boolean after restart')
                finally:
                    s.close()

            root = temp / 'bundled'
            seed(root, bundled=True)
            rules(root, [origin + '/must-not-fetch.jar'], mode='replace')
            s = Sidecar(executable, root)
            try:
                s.ok('settings.get', scope='instance', instance='native', key='JavaArchitecture')
                s.ok('instance.info', instance='native')
                s.ok('settings.set', scope='launcher', key='LibraryDownloadPatchesEnabled', value=True)
                hits.clear()
                result = s.execute('instance.update', instance='native')
                assert result['ok'], (result, hits, list(s.stderr))
                assert not any(p.startswith('/missing/') or p == '/must-not-fetch.jar' for p in hits), hits
                assert not (root / 'libraries' / relative).exists()
                print('PASS: bundled Maven native takes priority over URL replacement')
            finally:
                s.close()

            for backend in (0, 2):
                root = temp / f'replace-and-integrity-{backend}'
                native = {'name': 'gtnh.natives:lwjgl3:3.4.3', 'natives': {'windows': 'natives-windows'},
                          'downloads': {'classifiers': {'natives-windows': {'url': origin + '/missing/original.jar',
                              'path': relative.as_posix(), 'sha1': hashlib.sha1(payload).hexdigest(), 'size': len(payload)}}}}
                seed(root, native=native)
                rules(root, [origin + '/wrong.jar', origin + '/correct.jar'], mode='replace', url=origin + '/missing/original.jar')
                s = Sidecar(executable, root)
                try:
                    s.ok('settings.get', scope='instance', instance='native', key='JavaArchitecture')
                    s.ok('instance.info', instance='native')
                    s.ok('settings.set', scope='launcher', key='DownloadBackend', value=backend)
                    s.ok('settings.set', scope='launcher', key='LibraryDownloadPatchesEnabled', value=True)
                    hits.clear()
                    result = s.execute('instance.update', instance='native')
                    assert result['ok'], (result, hits, list(s.stderr))
                    assert '/missing/original.jar' not in hits and '/wrong.jar' in hits and '/correct.jar' in hits, hits
                    assert (root / 'libraries' / relative).read_bytes() == payload
                    print(f'PASS backend {backend}: explicit URL replacement, incorrect alternative rejected, checksum preserved across fallback')
                finally:
                    s.close()

            # Import the same directory structure shown in the user's archive.
            root = temp / 'import'
            root.mkdir()
            archive_path = temp / 'hmcl-pack.zip'
            with zipfile.ZipFile(archive_path, 'w') as archive:
                archive.writestr('.minecraft/versions/custom/custom.json', json.dumps({
                    'id': 'custom', 'mainClass': 'net.minecraft.client.main.Main', 'minecraftArguments': '--gameDir ${game_directory}',
                    'libraries': [{'name': 'gtnh.natives:lwjgl3:3.4.3', 'natives': {'windows': 'natives-windows'}}]}))
                archive.writestr('.minecraft/versions/custom/custom.jar', payload)
                archive.writestr('.minecraft/libraries/' + relative.as_posix(), payload)
            s = Sidecar(executable, root)
            try:
                result = s.execute('instance.import', source=str(archive_path), name='Imported natives', targetFormat='prism')
                assert result['ok'], (result, list(s.stderr))
                copies = list((root / 'instances').glob('*/libraries/' + relative.as_posix()))
                assert len(copies) == 1 and copies[0].read_bytes() == payload, copies
                print('PASS: HMCL/plain-pack import preserves bundled native classifiers')
            finally:
                s.close()
    finally:
        server.shutdown()
        server.server_close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, default=Path('install-webui-authfix-release/lunalauncher-cli.exe'))
    main(parser.parse_args().exe.resolve())
