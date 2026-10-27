# SPDX-License-Identifier: GPL-3.0-only
"""Exercise catalog cover installation through the real sidecar and importer.

pixi run python tests/modpack_icon_regression.py --exe install-neo-ui-api/lunalauncher-cli.exe

Uses an isolated profile and a local JS catalog on the same resource-provider
install branch as Modrinth/CurseForge. No external downloads or user data writes.
"""
import argparse
import base64
from collections import Counter
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import io
import json
from pathlib import Path
import struct
import tempfile
import threading
from urllib.parse import urlparse
import zipfile
import zlib

from api_sidecar_smoke import Sidecar


def png(rgb):
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 16, 16, 8, 2, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress((b'\0' + bytes(rgb) * 16) * 16)) + chunk(b'IEND', b''))


def main(executable):
    covers = {'green': png((24, 180, 75)), 'blue': png((40, 65, 215))}
    bundled = png((240, 60, 40))
    archive = io.BytesIO()
    with zipfile.ZipFile(archive, 'w') as pack:
        pack.writestr('instance.cfg', '[General]\nInstanceType=OneSix\nname=Bundled fixture\niconKey=bundled\n')
        pack.writestr('mmc-pack.json', '{"formatVersion":1,"components":[]}')
        pack.writestr('bundled.png', bundled)
        pack.writestr('minecraft/config/marker.txt', 'pack contents preserved')
    requests = Counter()
    slow_started, release = threading.Event(), threading.Event()

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass

        def do_GET(self):
            path = urlparse(self.path).path
            requests[path] += 1
            status, content_type = 200, 'application/json'
            if path.startswith('/project/'):
                project = path.rsplit('/', 1)[-1]
                body = json.dumps({'addonId': project, 'name': 'Catalog ' + project,
                                   'logoUrl': '' if project == 'missing' else base + '/cover/' + project}).encode()
            elif path.startswith('/versions/'):
                body = json.dumps({'data': [{'versionId': '1', 'version': '1',
                                    'downloadUrl': base + '/pack.zip', 'fileName': 'pack.zip'}]}).encode()
            elif path == '/pack.zip':
                content_type, body = 'application/zip', archive.getvalue()
            elif path.startswith('/cover/'):
                name = path.rsplit('/', 1)[-1]
                if name == 'slow':
                    slow_started.set()
                    release.wait(20)
                content_type, body = 'image/png', covers.get(name, b'not an image')
                if name == 'unavailable':
                    status = 404
            else:
                status, body = 404, b'not found'
            self.send_response(status)
            self.send_header('Content-Type', content_type)
            self.send_header('Content-Length', str(len(body)))
            self.end_headers()
            try:
                self.wfile.write(body)
            except (ConnectionError, OSError):
                pass

    server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    server.daemon_threads = True
    base = f'http://127.0.0.1:{server.server_port}'
    threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        with tempfile.TemporaryDirectory(prefix='luna-pack-cover-') as folder:
            root = Path(folder)
            (root / 'jsapi').mkdir()
            (root / 'jsapi/cover-fixture.js').write_text('var base = ' + json.dumps(base) + ''';
var api = {
    metadata: {id: 'cover-fixture', displayName: 'Cover fixture', version: '1',
               provider: 'MODRINTH', supportedTypes: [4], enabled: true},
    getInfoURL(id) { return base + '/project/' + id; },
    getVersionsURL(args) { return base + '/versions/' + args.addonId; },
    loadIndexedPack(obj) { return obj; },
    loadExtraPackInfo(pack, obj) { return {}; },
    loadIndexedPackVersion(obj) { return obj; },
    documentToArray(doc) { return doc.data; },
    getSortingMethods() { return []; }
};
''', encoding='utf-8')
            reference = root / 'instances/reference'
            reference.mkdir(parents=True)
            (reference / 'instance.cfg').write_text('[General]\nInstanceType=OneSix\nname=Reference\n')
            (reference / 'mmc-pack.json').write_text('{"formatVersion":1,"components":[]}')
            sidecar = Sidecar(executable, root)
            try:
                expected = {}
                for name, data in {**covers, 'bundled': bundled}.items():
                    path = root / (name + '.png')
                    path.write_bytes(data)
                    sidecar.ok('instance.set-icon', instance='reference', path=str(path), name='reference-' + name + '.png')
                    expected[name] = sidecar.ok('instance.icon', instance='reference')['dataUrl']

                def install(project, **params):
                    result = sidecar.ok('modpack.install', provider='js:cover-fixture', projectId=project, versionId='1', **params)
                    assert result['installed'] and len(result['instances']) == 1, result
                    instance = result['instances'][0]
                    icon = sidecar.ok('instance.icon', instance=instance)
                    cfg = (root / 'instances' / instance / 'instance.cfg').read_text(encoding='utf-8')
                    assert 'iconKey=' + icon['icon'] in cfg, cfg
                    assert (root / 'instances' / instance / 'minecraft/config/marker.txt').read_text() == 'pack contents preserved'
                    return instance, icon, result

                first, green, result = install('green')
                assert green['dataUrl'] == expected['green'], 'Catalog cover did not replace the archive/provider fallback'
                assert not result['warnings'], result
                assert first == 'Catalog green', first
                assert (root / 'icons' / (green['icon'] + '.png')).is_file()
                second, blue, _ = install('blue')
                assert blue['dataUrl'] == expected['blue'] and blue['icon'] != green['icon']
                assert sidecar.ok('instance.icon', instance=first) == green, 'Another pack overwrote the first cover'
                _, repeat, _ = install('green', name='Second green copy')
                assert repeat['id'] != green['id'], 'The second install must create its own instance'
                assert (repeat['icon'], repeat['dataUrl']) == (green['icon'], green['dataUrl']), (
                    'Installing the same cover again must remain readable', repeat, green)
                print('PASS: catalog cover overrides bundled icon, persists to disk, and survives repeated installs')

                count = requests['/cover/green']
                _, custom, _ = install('green', name='Custom icon', icon='reference-blue')
                assert custom['icon'] == 'reference-blue' and custom['dataUrl'] == expected['blue']
                assert requests['/cover/green'] == count, 'Explicit icons must skip cover downloads'
                for project in ('missing', 'invalid', 'unavailable'):
                    _, icon, result = install(project)
                    assert icon['dataUrl'] == expected['bundled'], (project, icon['icon'])
                    assert bool(result['warnings']) == (project != 'missing'), result
                print('PASS: explicit icon preserved; absent, invalid and unavailable covers keep the bundled fallback')

                before = sidecar.ok('instance.list')
                count = requests['/pack.zip']
                request_id = sidecar.start('modpack.install', provider='js:cover-fixture', projectId='slow', versionId='1')
                assert slow_started.wait(10), 'Cover download did not start'
                assert sidecar.ok('task.cancel')['cancelled']
                assert not sidecar.response(request_id, timeout=10)['result']['ok']
                release.set()
                assert requests['/pack.zip'] == count, 'Cancellation still started the archive installation'
                assert sidecar.ok('instance.list') == before
                print('PASS: cancellation during cover download does not create an instance')

                sidecar.close()
                sidecar = Sidecar(executable, root)
                assert sidecar.ok('instance.icon', instance=first) == green
                assert sidecar.ok('instance.icon', instance=second) == blue
                assert base64.b64decode(green['dataUrl'].split(',', 1)[1]).startswith(b'\x89PNG')
                print('PASS: restarted sidecar returns the exact same covers without a catalog lookup')
            finally:
                release.set()
                sidecar.close()
    finally:
        release.set()
        server.shutdown()
        server.server_close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=lambda p: Path(p).resolve(), required=True)
    main(parser.parse_args().exe)
