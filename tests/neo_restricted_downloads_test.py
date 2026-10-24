"""End-to-end restricted-download checks against a built MCP sidecar.

Uses a local fake CurseForge mirror and a temporary launcher data directory.
Usage: python tests/neo_restricted_downloads_test.py PATH_TO_CLI DLL_DIRECTORY
"""
import hashlib
import io
import json
import os
from pathlib import Path
import queue
import subprocess
import sys
import tempfile
import threading
import time
import zipfile
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


def main():
    binary, dlls = map(lambda value: Path(value).resolve(), sys.argv[1:3])
    contents = io.BytesIO()
    with zipfile.ZipFile(contents, 'w') as jar:
        jar.writestr('META-INF/MANIFEST.MF', 'Manifest-Version: 1.0\n')
    payload = contents.getvalue()
    digest = hashlib.sha1(payload).hexdigest()
    version = dict(id=200, modId=100, fileDate='2025-01-01T00:00:00Z',
                   displayName='Restricted fixture', fileName='restricted-fixture.jar',
                   downloadUrl=None, releaseType=1, gameVersions=['1.20.1', 'Forge'],
                   hashes=[dict(algo=1, value=digest)], dependencies=[])
    project = dict(id=100, name='Restricted fixture', slug='restricted-fixture', classId=6,
                   links=dict(websiteUrl='https://www.curseforge.com/minecraft/mc-mods/restricted-fixture'))
    pack_payload = b''

    class Mirror(BaseHTTPRequestHandler):
        def respond(self, body):
            data = json.dumps(body).encode()
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Content-Length', str(len(data)))
            self.end_headers()
            self.wfile.write(data)

        def do_GET(self):
            if self.path == '/pack.zip':
                self.send_response(200)
                self.send_header('Content-Length', str(len(pack_payload)))
                self.end_headers()
                self.wfile.write(pack_payload)
            elif self.path.startswith('/mods/300/files'):
                pack_version = {**version, 'id': 400, 'modId': 300, 'fileName': 'pack.zip',
                                'downloadUrl': f'http://127.0.0.1:{self.server.server_port}/pack.zip',
                                'hashes': [dict(algo=1, value=hashlib.sha1(pack_payload).hexdigest())]}
                self.respond(dict(data=[pack_version], pagination=dict(index=0, resultCount=1, totalCount=1)))
            elif self.path == '/mods/300':
                self.respond(dict(data={**project, 'id': 300, 'classId': 4471, 'name': 'Online pack fixture'}))
            elif self.path == '/client.jar':
                self.send_response(200)
                self.send_header('Content-Length', str(len(payload)))
                self.end_headers()
                self.wfile.write(payload)
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
                                      url=f'http://127.0.0.1:{self.server.server_port}/client.jar', sha1=digest, size=len(payload)))),
                                  mainClass='net.minecraft.client.main.Main', minecraftArguments='',
                                  assetIndex=dict(id='fixture', url=f'http://127.0.0.1:{self.server.server_port}/assets.json',
                                                  sha1=hashlib.sha1(assets).hexdigest(), size=len(assets), totalSize=0)))
            elif self.path == '/assets.json':
                self.respond(dict(objects={}))
            elif '/files/200' in self.path:
                self.respond(dict(data=version))
            elif '/files' in self.path:
                self.respond(dict(data=[version], pagination=dict(index=0, resultCount=1, totalCount=1)))
            elif '/description' in self.path:
                self.respond(dict(data='Fixture'))
            else:
                self.respond(dict(data=project))

        def do_POST(self):
            self.rfile.read(int(self.headers.get('Content-Length', 0)))
            self.respond({} if 'version_files' in self.path else dict(data=[version] if '/files' in self.path else [project]))

        def log_message(self, *args):
            pass

    server = ThreadingHTTPServer(('127.0.0.1', 0), Mirror)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    with tempfile.TemporaryDirectory(prefix='luna-restricted-test-') as temp:
        root = Path(temp)
        downloads = root / 'downloads'
        downloads.mkdir()
        instance = root / 'instances' / 'fixture'
        (instance / 'minecraft' / 'mods').mkdir(parents=True)
        (instance / 'instance.cfg').write_text('[General]\nInstanceType=OneSix\nname=Restricted fixture\n', encoding='utf-8')
        (instance / 'mmc-pack.json').write_text(json.dumps(dict(formatVersion=1, components=[
            dict(uid='net.minecraft', version='1.20.1', important=True)
        ])), encoding='utf-8')
        log = (root / 'sidecar.log').open('w', encoding='utf-8')
        env = {**os.environ, 'PATH': str(dlls) + os.pathsep + os.environ.get('PATH', ''),
               'QT_QPA_PLATFORM_PLUGIN_PATH': str(dlls / 'platforms'), 'QT_QPA_PLATFORM': 'offscreen'}
        process = subprocess.Popen([str(binary), '--mcp', '--dir', str(root)], stdin=subprocess.PIPE,
                                   stdout=subprocess.PIPE, stderr=log, text=True, encoding='utf-8', env=env)
        replies = queue.Queue()

        def read():
            for line in process.stdout:
                try:
                    item = json.loads(line)
                    if 'id' in item:
                        replies.put(item)
                except ValueError:
                    pass

        threading.Thread(target=read, daemon=True).start()
        request_id = 0

        def call(operation, parameters=None, check=True):
            nonlocal request_id
            request_id += 1
            process.stdin.write(json.dumps(dict(jsonrpc='2.0', id=request_id, method='launcher/execute',
                params=dict(operation=operation, parameters=parameters or {}))) + '\n')
            process.stdin.flush()
            reply = replies.get(timeout=30)
            assert reply['id'] == request_id, reply
            result = reply.get('result', {})
            if check:
                assert result.get('ok'), reply
            return result.get('data') if check else result

        def prompt(task_id, previous=None):
            until = time.monotonic() + 35
            while time.monotonic() < until:
                task = call('neo.task.status', dict(taskId=task_id))
                request = task.get('interaction', {})
                if request.get('interactionId') and request['interactionId'] != previous:
                    return request
                assert not task.get('finished'), task
                time.sleep(.1)
            raise AssertionError('No pending interaction')

        def answer(task_id, request, value=None, cancel=False):
            return call('neo.task.respond', dict(taskId=task_id, interactionId=request['interactionId'], value=value, cancel=cancel))

        def finished(task_id):
            until = time.monotonic() + 35
            while time.monotonic() < until:
                task = call('neo.task.status', dict(taskId=task_id))
                if task.get('finished'):
                    return task
                time.sleep(.1)
            raise AssertionError('Task did not finish')

        try:
            mirror = f'http://127.0.0.1:{server.server_port}'
            for key, value in dict(CurseForgeMirror=2, CurseForgeMirrorURL=mirror,
                                   ModrinthMirror=2, ModrinthMirrorURL=mirror,
                                   DownloadsDir=str(downloads), CurseForgeDownloadBrowser='System',
                                   FallbackMRBlockedMods=False, DownloadBackend=0, MetaURLOverride=mirror).items():
                call('settings.set', dict(scope='launcher', key=key, value=value))
            args = dict(provider='curseforge', kind='mods', instance='fixture', projectId='100',
                        versionId='200', minecraftVersion='1.20.1', loaders=['forge'])
            task_id = call('neo.task.start', dict(operation='resource.install-version', parameters=args))['taskId']
            request = prompt(task_id)
            assert request['type'] == 'restricted-downloads' and not request['complete'], request
            stale = request['interactionId']
            answer(task_id, request, 'continue')
            request = prompt(task_id, stale)
            assert not request['complete'], 'UI bypass accepted unverified files'
            wrong = downloads / version['fileName']
            wrong.write_bytes(b'incorrect contents')
            previous = request['interactionId']
            answer(task_id, request, str(wrong))
            request = prompt(task_id, previous)
            assert request['error'] and not request['complete'], request
            wrong.write_bytes(payload)
            previous = request['interactionId']
            answer(task_id, request, 'scan')
            request = prompt(task_id, previous)
            assert request['complete'] and all(item['matched'] for item in request['files']), request
            assert not call('neo.task.respond', dict(taskId=task_id, interactionId=stale, value='continue'), False)['ok']
            # Changed after verification: a stale green UI must not authorize installation.
            wrong.write_bytes(b'changed after verification')
            previous = request['interactionId']
            answer(task_id, request, 'continue')
            request = prompt(task_id, previous)
            assert not request['complete'], request
            wrong.write_bytes(payload)
            previous = request['interactionId']
            answer(task_id, request, 'scan')
            request = prompt(task_id, previous)
            answer(task_id, request, 'continue')
            task = finished(task_id)
            assert task['state'] == 'succeeded', task
            installed = instance / 'minecraft' / 'mods' / version['fileName']
            assert hashlib.sha1(installed.read_bytes()).hexdigest() == digest
            print('PASS: restricted file, bad hash, premature confirmation, stale reply, changed file, installation')

            wrong.unlink()
            args['replace'] = True
            task_id = call('neo.task.start', dict(operation='resource.install-with-dependencies', parameters=args))['taskId']
            request = prompt(task_id)
            call('neo.task.cancel', dict(taskId=task_id))
            assert finished(task_id)['state'] == 'cancelled'
            assert not call('neo.task.respond', dict(taskId=task_id, interactionId=request['interactionId'], value='continue'), False)['ok']
            print('PASS: dependency install cancellation and late reply rejection')

            # A CF pack archive takes the same interactive path as online packs.
            pack = root / 'restricted-pack.zip'
            manifest = dict(manifestType='minecraftModpack', manifestVersion=1, name='Restricted pack fixture',
                            version='1', minecraft=dict(version='1.20.1', modLoaders=[]),
                            files=[dict(projectID=100, fileID=200, required=True)], overrides='overrides')
            with zipfile.ZipFile(pack, 'w') as archive:
                archive.writestr('manifest.json', json.dumps(manifest))
                archive.writestr('overrides/config/fixture.txt', 'fixture')
            pack_payload = pack.read_bytes()
            task_id = call('neo.task.start', dict(operation='instance.import', parameters=dict(source=str(pack), name='Pack fixture')))['taskId']
            request = prompt(task_id)
            assert request['type'] == 'restricted-downloads' and not request['complete'], request
            selected_directory = root / 'browser-downloads'
            selected_directory.mkdir()
            (selected_directory / version['fileName']).write_bytes(payload)
            previous = request['interactionId']
            answer(task_id, request, str(selected_directory))
            request = prompt(task_id, previous)
            assert request['complete'], request
            answer(task_id, request, 'continue')
            task = finished(task_id)
            assert task['state'] == 'succeeded', task
            assert task['createdInstanceId'], task
            new_mod = root / 'instances' / task['createdInstanceId'] / 'minecraft' / 'mods' / version['fileName']
            assert new_mod.read_bytes() == payload
            print('PASS: CF pack import, alternate download directory, confirmation, created instance')

            task_id = call('neo.task.start', dict(operation='modpack.install', parameters=dict(
                provider='curseforge', projectId='300', versionId='400', name='Online pack fixture')))['taskId']
            request = prompt(task_id)
            assert request['type'] == 'restricted-downloads', request
            previous = request['interactionId']
            answer(task_id, request, str(selected_directory))
            request = prompt(task_id, previous)
            assert request['complete'], request
            answer(task_id, request, 'continue')
            task = finished(task_id)
            assert task['state'] == 'succeeded' and task['createdInstanceId'], task
            assert (root / 'instances' / task['createdInstanceId'] / 'minecraft' / 'mods' / version['fileName']).read_bytes() == payload
            print('PASS: online CF pack install and restricted file confirmation')
        except Exception:
            log.flush()
            print((root / 'sidecar.log').read_text(encoding='utf-8', errors='replace')[-6500:])
            raise
        finally:
            process.terminate()
            process.wait(timeout=10)
            process.stdin.close()
            process.stdout.close()
            log.close()
    server.shutdown()


if __name__ == '__main__':
    main()
