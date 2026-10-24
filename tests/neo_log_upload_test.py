"""Exercise log preview/upload through MCP using only synthetic logs and a localhost paste server.

Usage: python tests/neo_log_upload_test.py PATH_TO_CLI DLL_DIRECTORY
"""
import gzip
import json
import os
from pathlib import Path
import queue
import subprocess
import sys
import tempfile
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs


def main():
    binary, dlls = (Path(arg).resolve() for arg in sys.argv[1:3])
    received = []

    class PasteServer(BaseHTTPRequestHandler):
        def do_POST(self):
            body = self.rfile.read(int(self.headers['Content-Length'])).decode('utf-8')
            received.append(parse_qs(body)['content'][0])
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            url = '' if self.path.startswith('/empty/') else f'http://127.0.0.1:{self.server.server_port}/log/fixture'
            self.wfile.write(json.dumps(dict(success=True, url=url)).encode())

        def log_message(self, *args):
            pass

    server = ThreadingHTTPServer(('127.0.0.1', 0), PasteServer)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        with tempfile.TemporaryDirectory(prefix='luna-log-test-') as temp:
            root = Path(temp)
            instance = root / 'instances' / 'fixture'
            logs = instance / 'minecraft' / 'logs'
            logs.mkdir(parents=True)
            (instance / 'instance.cfg').write_text('[General]\nInstanceType=OneSix\nname=Log fixture\n', encoding='utf-8')
            (instance / 'mmc-pack.json').write_text('{"formatVersion":1,"components":[]}', encoding='utf-8')
            original = 'C:/Users/PRIVATE_USER/game\n(Session ID is PRIVATE_TOKEN)\n中文 + literal%20 & x=y #fragment\n'
            (logs / 'latest.log').write_text(original, encoding='utf-8')
            long_log = '\n'.join(f'line-{index:05d} ' + 'x' * 70 for index in range(30000))
            with gzip.open(logs / 'archive.log.gz', 'wb') as archive:
                archive.write(long_log.encode())
            env = {**os.environ, 'PATH': str(dlls) + os.pathsep + os.environ.get('PATH', ''),
                   'QT_QPA_PLATFORM_PLUGIN_PATH': str(dlls / 'platforms'), 'QT_QPA_PLATFORM': 'offscreen'}
            with (root / 'sidecar.log').open('w', encoding='utf-8') as stderr:
                process = subprocess.Popen([str(binary), '--mcp', '--dir', str(root)], stdin=subprocess.PIPE,
                                           stdout=subprocess.PIPE, stderr=stderr, text=True, encoding='utf-8', env=env)
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
                    reply = replies.get(timeout=45)
                    assert reply['id'] == request_id, reply
                    result = reply.get('result', {})
                    if check:
                        assert result.get('ok'), reply
                    return result.get('data') if check else result

                try:
                    base = f'http://127.0.0.1:{server.server_port}'
                    for key, value in dict(PastebinType=3, PastebinCustomAPIBase=base).items():
                        call('settings.set', dict(scope='launcher', key=key, value=value))
                    files = call('instance.log.list', dict(instance='fixture'))
                    latest = next(item['path'] for item in files if item['name'] == 'latest.log')
                    result = call('instance.log.upload', dict(instance='fixture', file=latest))
                    assert result['url'].endswith('/log/fixture'), result
                    assert 'PRIVATE_USER' not in received[-1] and 'PRIVATE_TOKEN' not in received[-1]
                    assert '中文 + literal%20 & x=y #fragment' in received[-1], repr(received[-1])
                    print('PASS: configured paste service, file upload, anonymization, special characters')

                    preview = call('instance.log.read', dict(instance='fixture', file='archive.log.gz', maxBytes=1048576))
                    assert preview['truncated'] and 'line-29999' not in preview['content']
                    call('instance.log.upload', dict(instance='fixture', file='archive.log.gz'))
                    assert received[-1] == long_log, 'Upload used the truncated preview'
                    result = call('instance.log.upload', dict(instance='fixture', file='archive.log.gz', truncate=True))
                    assert result['truncated']
                    assert 'line-00000' in received[-1] and 'line-29999' in received[-1]
                    assert 'line-12000' not in received[-1] and len(received[-1].splitlines()) <= 25000
                    print('PASS: gzip preview limit, complete file upload, optional head/tail truncation')

                    call('instance.log.upload', dict(content='Synthetic live console', pasteType=3, baseUrl=base))
                    assert received[-1] == 'Synthetic live console'
                    assert not call('instance.log.upload', dict(content='fixture', pasteType=3, baseUrl=base + '/empty'), False)['ok']
                    before = len(received)
                    assert not call('instance.log.upload', dict(instance='fixture', file=str(root / 'sidecar.log')), False)['ok']
                    assert len(received) == before
                    print('PASS: live text upload, empty link rejection, out-of-instance path rejection')
                finally:
                    process.stdin.close()
                    try:
                        process.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait(timeout=5)
    finally:
        server.shutdown()
        server.server_close()


if __name__ == '__main__':
    main()
