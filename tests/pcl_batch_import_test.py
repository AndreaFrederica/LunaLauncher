"""Regression: per-file PCL progress and recovery from one HTTP 403 in a pack.

pixi run python tests/pcl_batch_import_test.py --exe install-webui-release/lunalauncher-cli.exe
Uses only local HTTP fixtures and disposable launcher data.
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', required=True, type=Path)
    args = parser.parse_args()
    content = io.BytesIO()
    with zipfile.ZipFile(content, 'w') as jar:
        jar.writestr('META-INF/MANIFEST.MF', 'Manifest-Version: 1.0\n')
        jar.writestr('fixture', b'x' * 32768)
    payload = content.getvalue()
    digest = hashlib.sha1(payload).hexdigest()
    versions, projects = [], []
    class Mirror(BaseHTTPRequestHandler):
        def log_message(self, *_): pass

        def respond(self, data, status=200):
            body = data if isinstance(data, bytes) else json.dumps(data).encode()
            self.send_response(status)
            self.send_header('Content-Length', str(len(body)))
            self.end_headers()
            try:
                # Keep successful transfers visible long enough to inspect a
                # real active file snapshot, including unknown queued sizes.
                for i in range(0, len(body), 4096):
                    self.wfile.write(body[i:i+4096])
                    self.wfile.flush()
                    if self.path.endswith('.jar'): time.sleep(.08)
            except (ConnectionError, OSError): pass

        def do_HEAD(self):
            self.send_response(403 if self.path == '/blocked.jar' else 200)
            self.send_header('Content-Length', str(len(payload)))
            self.end_headers()

        def do_GET(self):
            if self.path == '/blocked.jar': self.respond(b'Forbidden', 403)
            elif self.path.endswith('.jar'): self.respond(payload)
            elif self.path == '/index.json':
                self.respond(dict(formatVersion=1, packages=[dict(uid='net.minecraft', name='Minecraft')]))
            elif self.path == '/net.minecraft/index.json':
                self.respond(dict(formatVersion=1, uid='net.minecraft', name='Minecraft', versions=[
                    dict(version='1.20.1', releaseTime='2023-06-12T00:00:00Z', type='release')]))
            elif self.path == '/net.minecraft/1.20.1.json':
                assets = json.dumps(dict(objects={})).encode()
                self.respond(dict(formatVersion=1, uid='net.minecraft', name='Minecraft', version='1.20.1',
                                  releaseTime='2023-06-12T00:00:00Z', type='release', libraries=[],
                                  mainJar=dict(name='com.mojang:minecraft:1.20.1:client', downloads=dict(artifact=dict(
                                      url=base+'/client.jar', sha1=digest, size=len(payload)))),
                                  mainClass='net.minecraft.client.main.Main', minecraftArguments='',
                                  assetIndex=dict(id='fixture', url=base+'/assets.json',
                                                  sha1=hashlib.sha1(assets).hexdigest(), size=len(assets), totalSize=0)))
            elif self.path == '/assets.json': self.respond(dict(objects={}))
            elif self.path == '/net.minecraftforge/index.json':
                self.respond(dict(formatVersion=1, uid='net.minecraftforge', name='Minecraft Forge', versions=[
                    dict(version='47.4.20', releaseTime='2025-01-01T00:00:00Z', type='release',
                         requires=[dict(uid='net.minecraft', equals='1.20.1')])]))
            elif self.path == '/net.minecraftforge/47.4.20.json':
                time.sleep(.7)  # Keep the actual metadata request observable.
                self.respond(b'Metadata unavailable', 404)
            elif 'version_files' in self.path: self.respond({})
            else: self.respond(dict(data=projects[0]))

        def do_POST(self):
            self.rfile.read(int(self.headers.get('Content-Length', 0)))
            self.respond(dict(data=versions if '/files' in self.path else projects))

    server = ThreadingHTTPServer(('127.0.0.1', 0), Mirror)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    base = f'http://127.0.0.1:{server.server_port}'
    for i in range(3):
        projects.append(dict(id=100+i, name=f'Mod {i}', slug=f'mod-{i}', classId=6,
                             links=dict(websiteUrl=f'https://www.curseforge.com/minecraft/mc-mods/mod-{i}')))
        versions.append(dict(id=200+i, modId=100+i, fileDate='2025-01-01T00:00:00Z',
                             displayName=f'File {i}', fileName=f'mod-{i}.jar', fileLength=len(payload),
                             downloadUrl=base+('/blocked.jar' if i == 2 else f'/mod-{i}.jar'),
                             releaseType=1, gameVersions=['1.20.1', 'Forge'],
                             hashes=[dict(algo=1, value=digest)], dependencies=[]))
    try:
        with tempfile.TemporaryDirectory(prefix='luna-pcl-batch-') as temp:
            root = Path(temp)
            pack = root / 'fixture.zip'
            manifest = dict(manifestType='minecraftModpack', manifestVersion=1, name='PCL batch fixture',
                            version='1', author='Test', minecraft=dict(version='1.20.1', modLoaders=[]),
                            files=[dict(projectID=100+i, fileID=200+i, required=True) for i in range(3)], overrides='overrides')
            with zipfile.ZipFile(pack, 'w') as archive:
                archive.writestr('manifest.json', json.dumps(manifest))
                archive.writestr('overrides/', '')
            profile = root / 'profile'
            profile.mkdir()
            (profile/'lunalauncher.cfg').write_text('[General]\nLanguage=en_US\n', encoding='utf-8')
            sidecar = Sidecar(args.exe.resolve(), profile)
            try:
                for key, value in dict(CurseForgeMirror=2, CurseForgeMirrorURL=base, ModrinthMirror=2,
                                       ModrinthMirrorURL=base, DownloadBackend=2, FallbackMRBlockedMods=False,
                                       CurseForgeDownloadBrowser='System', MetaURLOverride=base,
                                       LocalServiceCache=False, NumberOfManualRetries=0, RequestTimeout=10).items():
                    sidecar.ok('settings.set', scope='launcher', key=key, value=value)
                # Invalid pack metadata and unavailable references must produce
                # precise errors rather than asserts, empty names, or a crash.
                def expect_failure(broken, expected, inspect_metadata=False):
                    invalid = root / 'invalid.zip'
                    with zipfile.ZipFile(invalid, 'w') as archive:
                        archive.writestr('manifest.json', json.dumps(broken))
                    reply = sidecar.call('launcher/execute', dict(operation='neo.task.start', parameters=dict(
                        operation='instance.import', parameters=dict(source=str(invalid), name='Invalid fixture'))))
                    assert reply['ok'], reply
                    until = time.monotonic()+30
                    saw_metadata = False
                    while time.monotonic() < until:
                        state = sidecar.ok('neo.task.status', taskId=reply['data']['taskId'])
                        for row in state.get('children', []):
                            source = row.get('source', {})
                            if row['state'] == 'running' and source.get('kind') == 'metadata':
                                assert row['progressUnit'] == 'bytes', row
                                assert source['url'].startswith(base), row
                                assert source.get('requestUrl'), row
                                assert source.get('requestStartedAt', 0) > 0, row
                                assert state['summary']['downloads']['total'] == 0, state
                                assert row['label'] == source['fileName'], row
                                if source['fileName'] == 'net.minecraftforge/47.4.20.json':
                                    saw_metadata = True
                        interaction = state.get('interaction') or {}
                        if interaction.get('interactionId'):
                            sidecar.ok('neo.task.respond', taskId=state['taskId'], interactionId=interaction['interactionId'], value=0)
                        if state['finished']:
                            assert state['state'] == 'failed', state
                            text = json.dumps(state)
                            assert all(word in text for word in expected), (expected, state)
                            assert sidecar.process.poll() is None
                            if inspect_metadata: assert saw_metadata, state
                            return
                        time.sleep(.05)
                    raise AssertionError('Invalid manifest test timed out')

                for key, value, expected in [
                    ('files', {}, ['manifest.json', 'files', 'array']),
                    ('files', [dict(projectID=0, fileID=200)], ['manifest.json', 'files[0]', 'projectID']),
                    ('files', [dict(projectID=100, fileID=200)]*2, ['manifest.json', 'files[1]', 'Duplicate fileID 200']),
                    ('minecraft', dict(version='', modLoaders=[]), ['manifest.json', 'minecraft.version']),
                ]:
                    expect_failure({**manifest, key: value}, expected)
                missing = versions.pop()
                expect_failure(manifest, ['manifest.json', 'CurseForge', 'projectID=102', 'fileID=202'])
                versions.append(missing)
                expect_failure({**manifest, 'files': [], 'minecraft': dict(version='1.20.1',
                    modLoaders=[dict(id='forge-47.4.20', primary=True)])}, ['47.4.20', 'Not Found', 'net.minecraftforge'], inspect_metadata=True)
                reply = sidecar.call('launcher/execute', dict(operation='neo.task.start', parameters=dict(
                    operation='instance.import', parameters=dict(source=str(pack), name='Batch fixture'))))
                assert reply['ok'], reply
                task_id = reply['data']['taskId']
                deadline = time.monotonic()+90
                observed_progress = recovered = supplied = False
                while time.monotonic() < deadline:
                    task = sidecar.ok('neo.task.status', taskId=task_id)
                    downloads = task.get('summary', {}).get('downloads', {})
                    if downloads.get('total') == 3 and downloads.get('running', 0):
                        rows = [row for row in task['children'] if row['progressUnit'] == 'bytes']
                        assert len(rows) == 3, rows
                        assert all(row['label'].startswith('mod-') for row in rows), rows
                        assert task['total'] == 3, task
                        observed_progress = True
                    interaction = task.get('interaction', {})
                    if interaction.get('interactionId'):
                        if interaction.get('type') == 'restricted-downloads':
                            assert [file['name'] for file in interaction['files']] == ['mod-2.jar'], interaction
                            assert interaction['downloadFailure'] is True, interaction
                            assert interaction['files'][0]['downloadUrl'].endswith('/blocked.jar'), interaction
                            assert '403' in interaction['files'][0]['downloadError'], interaction
                            assert sidecar.process.poll() is None
                            recovered = True
                            if not supplied:
                                local = root / 'mod-2.jar'
                                local.write_bytes(payload)
                                value = str(local)
                                supplied = True
                            else:
                                assert interaction['complete'], interaction
                                value = 'continue'
                        else: value = 0
                        sidecar.ok('neo.task.respond', taskId=task_id, interactionId=interaction['interactionId'], value=value)
                    if task['finished']:
                        assert task['state'] == 'succeeded', task
                        assert observed_progress and recovered, (observed_progress, recovered)
                        instance = profile/'instances'/task['createdInstanceId']
                        for i in range(3):
                            assert (instance/'minecraft/mods'/f'mod-{i}.jar').read_bytes() == payload
                        components = json.loads((instance/'mmc-pack.json').read_text())['components']
                        assert any(c['uid'] == 'net.minecraft' and c['version'] == '1.20.1' for c in components)
                        print('PASS: 5 invalid/unavailable manifest cases, metadata HTTP 404 details, per-file PCL progress, exact file counts, HTTP 403 details and recovery, only failed file requested, successful import')
                        break
                    time.sleep(.08)
                else: raise AssertionError(f'Batch regression test timed out: {task}')
            except Exception:
                print(''.join(sidecar.stderr))
                raise
            finally:
                sidecar.close()
    finally:
        server.shutdown()
        server.server_close()


if __name__ == '__main__': main()
