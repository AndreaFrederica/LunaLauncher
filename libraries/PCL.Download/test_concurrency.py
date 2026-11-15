"""Exercise the built native library against a controlled HTTP server.

Run: pixi run python libraries/PCL.Download/test_concurrency.py <path-to-library>
"""
import ctypes
import hashlib
import json
import select
import socket
import sys
import tempfile
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path


class Server(ThreadingHTTPServer):
    request_queue_size = 128
    daemon_threads = True


lock = threading.Lock()
active = large_active = peak = large_peak = 0
requests = {}
payloads = {'small': b's' * 65536, 'large': b'l' * 524288, 'unknown': b'u' * 65536}


class Handler(BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'

    def log_message(self, *args):
        pass

    def do_HEAD(self):
        self.respond(False)

    def do_GET(self):
        self.respond(True)

    def respond(self, body):
        global active, large_active, peak, large_peak
        kind = self.path.split('/')[1]
        payload = payloads[kind]
        self.send_response(200)
        self.send_header('Content-Length', str(len(payload)))
        self.end_headers()
        if not body:
            return
        # Downloader can issue a headers-only GET before the body request.
        # Do not count that already-closed probe as a live transfer.
        try:
            self.wfile.write(payload[:1024])
            self.wfile.flush()
            time.sleep(0.05)
            if select.select([self.connection], [], [], 0)[0] and not self.connection.recv(1, socket.MSG_PEEK):
                return
        except (BrokenPipeError, ConnectionResetError):
            return
        large = kind != 'small'
        with lock:
            # The library may overlap its metadata GET and body GET for the
            # same file; the policy limits files, not HTTP probe requests.
            if not requests.get(self.path, 0):
                active += 1
                large_active += large
            requests[self.path] = requests.get(self.path, 0) + 1
            peak = max(peak, active)
            large_peak = max(large_peak, large_active)
        try:
            time.sleep(0.2)
        except (BrokenPipeError, ConnectionResetError):
            pass
        finally:
            with lock:
                requests[self.path] -= 1
                if requests[self.path] == 0:
                    active -= 1
                    large_active -= large
        try:
            self.wfile.write(payload[1024:])
        except (BrokenPipeError, ConnectionResetError):
            pass


lib = ctypes.CDLL(str(Path(sys.argv[1]).resolve()))


def bind(name, args, result=ctypes.c_int):
    function = getattr(lib, 'pcl_download_' + name)
    function.argtypes, function.restype = args, result
    return function


init = bind('init', [])
smart = bind('set_smart_concurrency', [ctypes.c_int], None)
limit = bind('set_thread_limit', [ctypes.c_int], None)
proxy = bind('set_proxy', [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_char_p], None)
start = bind('batch_start', [ctypes.c_char_p])
state = bind('batch_get_state', [ctypes.c_int])
abort = bind('batch_abort', [ctypes.c_int], None)
release = bind('release', [ctypes.c_int], None)
assert init() == 0
proxy(b'None', b'', 0, b'', b'')
server = Server(('127.0.0.1', 0), Handler)
threading.Thread(target=server.serve_forever, daemon=True).start()
root = Path(tempfile.mkdtemp(prefix='luna-concurrency-test-'))


def run(name, groups, enabled, manual=4, cancel=False, switch=False):
    global peak, large_peak
    deadline = time.monotonic() + 5
    while active and time.monotonic() < deadline:
        time.sleep(0.05)
    assert active == 0
    peak = large_peak = 0
    smart(int(enabled))
    limit(manual)
    records, tasks = [], []
    for group, kinds in enumerate(groups):
        files = []
        for i, kind in enumerate(kinds):
            path = root / name / f'{group}-{i}'
            data = payloads[kind]
            files.append(dict(urls=[f'http://127.0.0.1:{server.server_port}/{kind}/{i}'], localPath=str(path),
                              expectedSize=-1 if kind == 'unknown' else len(data), hash=hashlib.sha1(data).hexdigest()))
            records.append((path, data))
        task = start(json.dumps(files).encode())
        assert task > 0
        tasks.append(task)
    if switch:
        time.sleep(0.1)
        smart(1)
    if cancel:
        time.sleep(0.1)
        for task in tasks:
            abort(task)
    deadline = time.monotonic() + 30
    while any(state(t) == 0 for t in tasks) and time.monotonic() < deadline:
        time.sleep(0.02)
    assert all(state(t) == (3 if cancel else 1) for t in tasks), (name, [state(t) for t in tasks])
    if not cancel:
        assert all(p.read_bytes() == data for p, data in records), name
    assert peak <= (32 if enabled or switch else manual), (name, peak)
    if enabled or switch:
        assert large_peak <= 8, (name, large_peak)
    if enabled and not cancel and any('small' in group for group in groups):
        assert peak > 8, (name, peak)
    if switch:
        assert peak > manual, (name, peak)
    print(json.dumps(dict(test=name, peak=peak, large_peak=large_peak, files=len(records))), flush=True)
    for task in tasks:
        release(task)


try:
    run('manual', [['small'] * 96], False)
    run('smart-small', [['small'] * 96], True)
    run('smart-large', [['large'] * 32], True)
    run('smart-unknown', [['unknown'] * 32], True)
    run('shared-batches', [['small'] * 64, ['large'] * 24], True)
    run('enable-during-batch', [['small'] * 96], False, switch=True)
    run('cancel', [['small'] * 96], True, cancel=True)
    run('after-cancel', [['small'] * 64], True)
    run('manual-restored', [['small'] * 32], False)
    print('PASS: global caps, unknown sizes, live enable, cancellation and manual restore', flush=True)
finally:
    server.shutdown()
