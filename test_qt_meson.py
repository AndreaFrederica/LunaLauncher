#!/usr/bin/env python3
import os
import subprocess
import sys

QT_BIN = "third_party/qt/6.10.1/macos/bin"
QMAKE = os.path.join(QT_BIN, "qmake6")

print("=== Environment ===")
print(f"qmake6 exists: {os.path.isfile(QMAKE)}")

# Test qmake queries
for query in ["QT_HOST_LIBS", "QT_INSTALL_HEADERS", "QT_INSTALL_LIBS"]:
    result = subprocess.run([QMAKE, "-query", query], capture_output=True, text=True)
    print(f"qmake -query {query}: {result.stdout.strip()}")

# Check if meson can find config-tool
print("\n=== Meson version ===")
result = subprocess.run(["meson", "--version"], capture_output=True, text=True)
print(f"meson: {result.stdout.strip()}")

# Check what meson does with config-tool
print("\n=== Testing meson dependency probe ===")
env = os.environ.copy()
env["PATH"] = os.pathsep.join([os.path.abspath(QT_BIN)] + env.get("PATH", "").split(os.pathsep))
env["MACOSX_DEPLOYMENT_TARGET"] = "27.0"

# Create a minimal meson.build to test
test_dir = "/tmp/meson_qt_test"
os.makedirs(test_dir, exist_ok=True)
with open(os.path.join(test_dir, "meson.build"), "w") as f:
    f.write("""project('test', 'cpp')
qt_core = dependency('Qt6Core', method: 'config-tool', required: true)
message('Qt6Core found: ' + qt_core.found().to_string())
""")

# Run meson setup
build_dir = os.path.join(test_dir, "build")
result = subprocess.run(
    ["meson", "setup", build_dir, test_dir],
    capture_output=True, text=True, env=env
)
print(f"stdout:\n{result.stdout}")
print(f"stderr:\n{result.stderr}")
print(f"returncode: {result.returncode}")
