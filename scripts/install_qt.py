import os
import platform
import subprocess
import sys

"""
基础配置
"""
QT_INSTALL_PATH = "third_party/qt/"
QT_VERSION = "6.10.1"
QT_BUILD_TOOLCHAIN = {
    "Windows": "win64_msvc2022_64", # Defined in aqt list-qt windows desktop --arch QT_VERSION
    "Linux": "linux_gcc_64",
    "Darwin": "clang_64",
}

"""
判断系统架构
"""
def detect_os_arch():
    os_name = platform.system()          # Windows / Linux / Darwin
    machine = platform.machine()          # x86_64 / AMD64 / arm64 / aarch64

    return {
        "os": os_name,
        "machine": machine,
    }

"""
安装QT
"""
def check_qt_installed():
    install_path = QT_INSTALL_PATH + QT_VERSION
    if os.path.exists(install_path):
        return True
    else:
        return False
def install_qt(osinfo, machineinfo):
    os_name = osinfo
    machine = machineinfo
    qt_install_platform = ""
    qt_install_host = ""

    if check_qt_installed():
        print("Qt already installed")
        return

    if os_name == "Windows":
        qt_install_host = "windows"
        if machine == "x86_64" or machine == "AMD64":
            qt_install_platform = QT_BUILD_TOOLCHAIN[os_name]                
        else:
            print("Unsupported architecture: {machine}")
            sys.exit(1)
    elif os_name == "Linux":
        qt_install_host = "linux"
        if machine == "x86_64" or machine == "AMD64":
            qt_install_platform = QT_BUILD_TOOLCHAIN[os_name]
        else:
            print("Unsupported architecture: {machine}")
    elif os_name == "Darwin":
        qt_install_host = "mac"
        if machine == "x86_64" or machine == "AMD64" or machine == "arm64" or machine == "aarch64":
            qt_install_platform = QT_BUILD_TOOLCHAIN[os_name]
        else:
            print("Unsupported architecture: {machine}")
    else:
        print("Unsupported OS: {os_name}")
    
    print("Installing Qt {QT_VERSION} for {qt_install_platform} in {QT_INSTALL_PATH}")

    cmd = [
        "aqt", "install-qt",
        qt_install_host, "desktop",
        QT_VERSION, qt_install_platform,
        "-m", "qtwebsockets", "qtnetworkauth", "qtmultimedia",
        "-O", str(QT_INSTALL_PATH)
    ]
    print("Executing:", " ".join(cmd))
    subprocess.check_call(cmd)


if __name__ == "__main__":
    info = detect_os_arch()
    install_qt(info["os"], info["machine"])
