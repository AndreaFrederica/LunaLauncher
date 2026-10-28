# Quick smoke test for the PCL.Download NativeAOT library (T2/T3/T4)
import ctypes
import json
import os
import sys
import tempfile
import time

DLL = r"D:\Projcets\PrismLauncher\libraries\PCL.Download\publish-aot\PCL.Download.dll"

lib = ctypes.CDLL(DLL)
lib.pcl_download_init.restype = ctypes.c_int
lib.pcl_download_file.restype = ctypes.c_int
lib.pcl_download_file.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int64]
lib.pcl_download_get_state.restype = ctypes.c_int
lib.pcl_download_get_state.argtypes = [ctypes.c_int]
lib.pcl_download_get_progress.restype = ctypes.c_double
lib.pcl_download_get_progress.argtypes = [ctypes.c_int]
lib.pcl_download_get_progress_detail.restype = ctypes.c_void_p
lib.pcl_download_get_progress_detail.argtypes = [ctypes.c_int]
lib.pcl_download_get_error.restype = ctypes.c_void_p
lib.pcl_download_get_error.argtypes = [ctypes.c_int]
lib.pcl_free_string.argtypes = [ctypes.c_void_p]
lib.pcl_download_file_abort.argtypes = [ctypes.c_int]
lib.pcl_download_set_event_callback.argtypes = [ctypes.c_void_p]

CALLBACK = ctypes.CFUNCTYPE(None, ctypes.c_int, ctypes.c_int, ctypes.c_int64, ctypes.c_int64, ctypes.c_int64, ctypes.c_int)
events = []


def on_event(task_id, event_type, downloaded, total, speed, threads):
    events.append((task_id, event_type, downloaded, total, speed, threads))
    if event_type != 0:
        print(f"  EVENT task={task_id} type={event_type} downloaded={downloaded}/{total}")


callback = CALLBACK(on_event)
lib.pcl_download_set_event_callback(ctypes.cast(callback, ctypes.c_void_p))


def read_string(ptr):
    if not ptr:
        return None
    s = ctypes.cast(ptr, ctypes.c_char_p).value
    lib.pcl_free_string(ptr)
    return s.decode("utf-8") if s else None


assert lib.pcl_download_init() == 0, "init failed"
print("T2 OK: library loaded, init succeeded")

# T3: single file download
target = os.path.join(tempfile.gettempdir(), "pcl_test_download.html")
if os.path.exists(target):
    os.remove(target)
urls = json.dumps(["https://example.com/"]).encode("utf-8")
tid = lib.pcl_download_file(urls, target.encode("utf-8"), None, -1)
assert tid > 0, f"download_file failed: {tid}"
print(f"task id: {tid}")

deadline = time.time() + 60
state = 0
while time.time() < deadline:
    state = lib.pcl_download_get_state(tid)
    if state != 0:
        break
    time.sleep(0.2)

assert state == 1, f"download did not finish, state={state}, error={read_string(lib.pcl_download_get_error(tid))}"
assert os.path.exists(target) and os.path.getsize(target) > 0
print(f"T3 OK: downloaded {os.path.getsize(target)} bytes")
assert any(e[0] == tid and e[1] == 1 for e in events), "missing 'finished' push event"
print(f"push events received: {len(events)} (progress={sum(1 for e in events if e[1] == 0)}, finished={sum(1 for e in events if e[1] == 1)})")

# T4: multi-URL failover (first URL invalid)
target2 = os.path.join(tempfile.gettempdir(), "pcl_test_failover.html")
if os.path.exists(target2):
    os.remove(target2)
urls = json.dumps(["https://invalid.example.invalid/nonexistent", "https://example.com/"]).encode("utf-8")
tid = lib.pcl_download_file(urls, target2.encode("utf-8"), None, -1)
assert tid > 0
deadline = time.time() + 90
state = 0
while time.time() < deadline:
    state = lib.pcl_download_get_state(tid)
    if state != 0:
        break
    time.sleep(0.5)
assert state == 1, f"failover failed, state={state}, error={read_string(lib.pcl_download_get_error(tid))}"
assert os.path.exists(target2) and os.path.getsize(target2) > 0
print(f"T4 OK: failover downloaded {os.path.getsize(target2)} bytes")

print("speed:", lib.pcl_download_get_speed(), "threads:", lib.pcl_download_get_active_threads())
lib.pcl_download_shutdown()
print("ALL PASS")
