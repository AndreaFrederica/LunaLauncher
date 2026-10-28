import ctypes, json, os, tempfile, time

DLL = r"D:\Projcets\PrismLauncher\libraries\PCL.Download\publish-aot\PCL.Download.dll"
lib = ctypes.CDLL(DLL)
lib.pcl_download_init.restype = ctypes.c_int
lib.pcl_download_file.restype = ctypes.c_int
lib.pcl_download_file.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int64]
lib.pcl_download_set_event_callback.argtypes = [ctypes.c_void_p]

CALLBACK = ctypes.CFUNCTYPE(None, ctypes.c_int, ctypes.c_int, ctypes.c_int64, ctypes.c_int64, ctypes.c_int64, ctypes.c_int)
finished = {}

def on_event(task_id, event_type, downloaded, total, speed, threads):
    if event_type == 0:
        print(f"  PROGRESS downloaded={downloaded:>10} total={total:>10} speed={speed:>10} threads={threads}")
    else:
        print(f"  TERMINAL type={event_type} downloaded={downloaded} total={total} speed={speed}")
        finished[task_id] = event_type

cb = CALLBACK(on_event)
assert lib.pcl_download_init() == 0
lib.pcl_download_set_event_callback(ctypes.cast(cb, ctypes.c_void_p))

def run(name, url, timeout=120):
    target = os.path.join(tempfile.gettempdir(), name)
    for p in (target, target + ".PCLDownloading"):
        if os.path.exists(p):
            os.remove(p)
    tid = lib.pcl_download_file(json.dumps([url]).encode(), target.encode(), None, -1)
    print(f"task {tid}: {url}")
    deadline = time.time() + timeout
    while time.time() < deadline and tid not in finished:
        time.sleep(0.1)
    ok = finished.get(tid) == 1 and os.path.exists(target)
    size = os.path.getsize(target) if os.path.exists(target) else 0
    print(f"  => {'OK' if ok else 'FAIL'} size={size}\n")
    return ok

# small file (forgecdn, like the screenshot case)
run("pcl_t_small.jar", "https://edge.forgecdn.net/files/2777/74/ThaumicComputers-MC1.12.2-0.5.1.jar")
# ~10 MB file to see ongoing progress/speed
run("pcl_t_big.bin", "https://speed.hetzner.de/10MB.bin")
