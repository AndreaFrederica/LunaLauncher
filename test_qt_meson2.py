#!/usr/bin/env python3
import os
import subprocess

QT_BIN = "third_party/qt/6.10.1/macos/bin"
QMAKE = os.path.join(QT_BIN, "qmake6")

env = os.environ.copy()
env["PATH"] = os.pathsep.join([os.path.abspath(QT_BIN)] + env.get("PATH", "").split(os.pathsep))
env["MACOSX_DEPLOYMENT_TARGET"] = "27.0"

test_dir = "/tmp/meson_qt_test2"
os.makedirs(test_dir, exist_ok=True)

# Test 1: auto method
with open(os.path.join(test_dir, "meson.build"), "w") as f:
    f.write("""project('test', 'cpp')
qt_core = dependency('Qt6Core', method: 'auto', required: true)
message('Qt6Core found: ' + qt_core.found().to_string())
""")

build_dir = os.path.join(test_dir, "build")
if os.path.exists(build_dir):
    import shutil
    shutil.rmtree(build_dir)

result = subprocess.run(
    ["meson", "setup", build_dir, test_dir],
    capture_output=True, text=True, env=env
)
print("=== method: auto ===")
print(f"stdout:\n{result.stdout[-500:]}")
print(f"stderr:\n{result.stderr[-500:]}")

# Check log
log_path = os.path.join(build_dir, "meson-logs", "meson-log.txt")
if os.path.exists(log_path):
    with open(log_path) as f:
        log = f.read()
    # Search for qmake related lines
    for line in log.split("\n"):
        if "qmake" in line.lower() or "config" in line.lower() or "qt6" in line.lower():
            print(f"LOG: {line}")
