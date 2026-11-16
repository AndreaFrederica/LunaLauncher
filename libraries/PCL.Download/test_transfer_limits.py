"""Verify independent file concurrency and per-file range limits against local HTTP.

pixi run python libraries/PCL.Download/test_transfer_limits.py install-webui-release/PCL.Download.dll
"""
import ctypes
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import select
import socket
import sys
import tempfile
import threading
import time

payload = bytes(range(256)) * (32768)  # 8 MiB, large enough to split.
lock = threading.Lock()
active = {}
file_peak = chunk_peak = 0


class Handler(BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'

    def log_message(self, *_): pass
    def do_HEAD(self): self.respond(False)
    def do_GET(self): self.respond(True)

    def respond(self, body):
        global file_peak, chunk_peak
        start, end = 0, len(payload)-1
        ranged = self.headers.get('Range', '')
        if ranged:
            bounds = ranged.removeprefix('bytes=').split('-')
            start = int(bounds[0] or 0)
            end = min(int(bounds[1] or end), end)
        self.send_response(206 if ranged else 200)
        self.send_header('Accept-Ranges', 'bytes')
        self.send_header('Content-Length', str(end-start+1))
        if ranged: self.send_header('Content-Range', f'bytes {start}-{end}/{len(payload)}')
        self.end_headers()
        if not body: return
        counted = False
        try:
            # Exclude Downloader's closed metadata probes from live transfers.
            first = min(end+1, start+4096)
            self.wfile.write(payload[start:first]); self.wfile.flush()
            time.sleep(.03)
            if select.select([self.connection], [], [], 0)[0] and not self.connection.recv(1, socket.MSG_PEEK): return
            if end-start+1 > 4096:
                with lock:
                    active[self.path] = active.get(self.path, 0)+1
                    file_peak = max(file_peak, len(active))
                    chunk_peak = max(chunk_peak, active[self.path])
                    counted = True
            for offset in range(first, end+1, 65536):
                self.wfile.write(payload[offset:min(offset+65536, end+1)])
                self.wfile.flush()
                time.sleep(.006)
        except (OSError, ConnectionError): pass
        finally:
            if counted:
                with lock:
                    active[self.path] -= 1
                    if not active[self.path]: del active[self.path]


lib = ctypes.CDLL(str(Path(sys.argv[1]).resolve()))


def bind(name, args, result=ctypes.c_int):
    fn = getattr(lib, 'pcl_download_'+name)
    fn.argtypes, fn.restype = args, result
    return fn


assert bind('init', [])() == 0
set_files = bind('set_thread_limit', [ctypes.c_int], None)
set_chunks = bind('set_chunk_limit', [ctypes.c_int], None)
bind('set_smart_concurrency', [ctypes.c_int], None)(0)
bind('set_proxy', [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_char_p], None)(b'None', b'', 0, b'', b'')
start_batch = bind('batch_start', [ctypes.c_char_p])
state = bind('batch_get_state', [ctypes.c_int])
error = bind('get_error', [ctypes.c_int], ctypes.c_void_p)
release = bind('release', [ctypes.c_int], None)
server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
server.daemon_threads = True
server.request_queue_size = 128
threading.Thread(target=server.serve_forever, daemon=True).start()
try:
    with tempfile.TemporaryDirectory(prefix='luna-transfer-limits-') as temp:
        for files, chunks in [(1, 1), (1, 4), (3, 1), (3, 2)]:
            deadline = time.monotonic()+5
            while active and time.monotonic() < deadline: time.sleep(.05)
            assert not active
            file_peak = chunk_peak = 0
            set_files(files); set_chunks(chunks)
            folder = Path(temp)/f'{files}-{chunks}'
            records = [dict(urls=[f'http://127.0.0.1:{server.server_port}/{files}-{chunks}/{i}.jar'],
                            localPath=str(folder/f'{i}.jar'), expectedSize=len(payload), hash=hashlib.sha1(payload).hexdigest()) for i in range(6)]
            task = start_batch(json.dumps(records).encode())
            assert task > 0
            deadline = time.monotonic()+60
            while state(task) == 0 and time.monotonic() < deadline: time.sleep(.02)
            assert state(task) == 1, ctypes.string_at(error(task)).decode() if error(task) else state(task)
            assert all(Path(r['localPath']).read_bytes() == payload for r in records)
            assert file_peak <= files, (files, chunks, file_peak)
            assert chunk_peak <= chunks, (files, chunks, chunk_peak)
            if files > 1: assert file_peak > 1, (files, chunks, file_peak)
            if chunks > 1: assert chunk_peak > 1, (files, chunks, chunk_peak)
            print(json.dumps(dict(files=files, chunks=chunks, filePeak=file_peak, chunkPeak=chunk_peak)), flush=True)
            release(task)
finally:
    server.shutdown()
print('PASS: file concurrency and per-file range limits operate independently', flush=True)
