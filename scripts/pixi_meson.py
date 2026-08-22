#!/usr/bin/env python3
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path


QT_VERSION = "6.10.1"
QT_WINDOWS_ARCH = "msvc2022_64"
QT_LINUX_ARCH = "gcc_64"
QT_MACOS_ARCH = "macos"
WINDOWS_PROFILES = {"release", "debug"}
LINUX_CROSS_PROFILE = "linux-x64-gcc-release"
PROFILES = WINDOWS_PROFILES | {LINUX_CROSS_PROFILE}
DEFAULT_MACOS_DEPLOYMENT_TARGET = "27.0"


def repo_root() -> Path:
    return Path(__file__).resolve().parent.parent


def profile_mode(profile: str) -> str:
    validate_profile(profile)
    return "release" if profile == LINUX_CROSS_PROFILE else profile


def profile_system(profile: str) -> str:
    validate_profile(profile)
    if profile == LINUX_CROSS_PROFILE:
        return "linux"
    system = platform.system().lower()
    if system == "windows":
        return "windows"
    if system == "linux":
        return "linux"
    if system == "darwin":
        return "macos"
    raise SystemExit(f"unsupported native platform: {platform.system()}")


def is_cross_profile(profile: str) -> bool:
    validate_profile(profile)
    return profile == LINUX_CROSS_PROFILE


def qt_tools_root(root: Path) -> Path:
    return root / "third_party" / "qt" / QT_VERSION / QT_WINDOWS_ARCH


def qt_target_root(root: Path, profile: str) -> Path:
    system = profile_system(profile)
    if system == "macos":
        arch = QT_MACOS_ARCH
    elif system == "linux":
        arch = QT_LINUX_ARCH
    else:
        arch = QT_WINDOWS_ARCH
    return root / "third_party" / "qt" / QT_VERSION / arch


def qt_config(root: Path, profile: str) -> Path:
    return qt_target_root(root, profile) / "lib" / "cmake" / "Qt6Core" / "Qt6CoreConfig.cmake"


def build_dir(root: Path, profile: str) -> Path:
    env_override = os.environ.get("LUNA_BUILD_DIR")
    if env_override:
        p = Path(env_override)
        return p if p.is_absolute() else root / p
    return root / f"build-meson-{profile}"


def install_dir(root: Path, profile: str) -> Path:
    env_override = os.environ.get("LUNA_INSTALL_DIR")
    if env_override:
        p = Path(env_override)
        return p if p.is_absolute() else root / p
    return root / f"install-{profile}"


def task_env(root: Path, profile: str) -> dict[str, str]:
    env = os.environ.copy()
    system = profile_system(profile)
    qt_target = qt_target_root(root, profile)
    path_entries = [
        entry
        for entry in env.get("PATH", "").split(os.pathsep)
        if "msys64" not in entry.lower()
    ]
    env["PATH"] = os.pathsep.join([str(qt_target / "bin")] + path_entries)
    env["CMAKE_PREFIX_PATH"] = str(qt_target / "lib" / "cmake")
    if system == "macos":
        env["MACOSX_DEPLOYMENT_TARGET"] = os.environ.get("LUNA_MACOS_DEPLOYMENT_TARGET", DEFAULT_MACOS_DEPLOYMENT_TARGET)
    if system == "windows" or is_cross_profile(profile):
        env["PKG_CONFIG_PATH"] = ""
        env["PKG_CONFIG_LIBDIR"] = str(root / ".meson-empty-pkgconfig")
    if system == "windows":
        env.setdefault("CC", "cl")
        env.setdefault("CXX", "cl")
    else:
        env.pop("CC", None)
        env.pop("CXX", None)
    return env


def run(cmd: list[str], *, cwd: Path, env: dict[str, str] | None = None) -> None:
    print("+", " ".join(cmd))
    subprocess.check_call(cmd, cwd=cwd, env=env)


def validate_profile(profile: str) -> None:
    if profile not in PROFILES:
        raise SystemExit(f"unsupported profile: {profile}")


def install_qt_tools(root: Path) -> None:
    if (qt_tools_root(root) / "lib" / "cmake" / "Qt6Core" / "Qt6CoreConfig.cmake").exists():
        print("Qt build tools already installed")
        return
    run(
        [
            "aqt",
            "install-qt",
            "windows",
            "desktop",
            QT_VERSION,
            "win64_msvc2022_64",
            "-m",
            "qtwebsockets",
            "qtnetworkauth",
            "qtmultimedia",
            "-O",
            str(root / "third_party" / "qt"),
        ],
        cwd=root,
    )


def install_qt_target(root: Path, profile: str) -> None:
    validate_profile(profile)
    if is_cross_profile(profile):
        install_qt_tools(root)
    if qt_config(root, profile).exists():
        print("Qt target already installed")
        return
    system = profile_system(profile)
    if system == "windows":
        install_qt_tools(root)
        return
    if system == "macos":
        run(
            [
                "aqt",
                "install-qt",
                "mac",
                "desktop",
                QT_VERSION,
                "clang_64",
                "-m",
                "qtwebsockets",
                "qtnetworkauth",
                "qtmultimedia",
                "-O",
                str(root / "third_party" / "qt"),
            ],
            cwd=root,
        )
        return
    run(
        [
            "aqt",
            "install-qt",
            "linux",
            "desktop",
            QT_VERSION,
            "linux_gcc_64",
            "-m",
            "qtwebsockets",
            "qtnetworkauth",
            "qtmultimedia",
            "-O",
            str(root / "third_party" / "qt"),
        ],
        cwd=root,
    )


def patch_qt_cmake_configs(root: Path, profile: str) -> None:
    if profile_system(profile) != "macos":
        return
    qt_root = qt_target_root(root, profile)
    cmake_dir = qt_root / "lib" / "cmake"
    if not cmake_dir.exists():
        return
    import re
    for targets_file in cmake_dir.glob("Qt6*/*Targets*.cmake"):
        content = targets_file.read_text()
        if "INTERFACE_INCLUDE_DIRECTORIES" not in content:
            continue

        def fix_include_dirs(match):
            dirs = match.group(1).split(";")
            filtered = [d for d in dirs if not d.rstrip('"').endswith('.framework')]
            if len(filtered) == len(dirs):
                return match.group(0)
            return 'INTERFACE_INCLUDE_DIRECTORIES "' + ";".join(filtered) + '"'

        new_content = re.sub(
            r'INTERFACE_INCLUDE_DIRECTORIES\s+"([^"]+)"',
            fix_include_dirs,
            content,
        )
        if new_content != content:
            targets_file.write_text(new_content)
            print(f"Patched {targets_file.name}")


def configure(root: Path, profile: str) -> None:
    validate_profile(profile)
    install_qt_target(root, profile)
    patch_qt_cmake_configs(root, profile)
    (root / ".meson-empty-pkgconfig").mkdir(exist_ok=True)

    mode = profile_mode(profile)
    system = profile_system(profile)
    bdir = build_dir(root, profile)
    idir = install_dir(root, profile)

    build_testing = os.environ.get("LUNA_BUILD_TESTING", "false").lower() in ("1", "true", "yes")
    disable_ownership = os.environ.get("LUNA_DISABLE_OWNERSHIP_CHECK", "false").lower() in ("1", "true", "yes")

    args = [
        "meson",
        "setup",
        str(bdir),
        "--prefix",
        str(idir),
        "--buildtype",
        mode,
        "-Ddefault_library=static" if mode == "release" else "-Ddefault_library=shared",
        "-Dwarning_level=0",
        f"-Dbuild_testing={'true' if build_testing else 'false'}",
        f"-Ddisable_ownership_check={'true' if disable_ownership else 'false'}",
        "-Dbuild_updater=false",
        "-Dbuild_filelinker=true" if system == "windows" else "-Dbuild_filelinker=false",
        "-Dlibarchive:tests=disabled",
        "-Dlibarchive:zlib=enabled",
        "-Dlibarchive:bz2lib=enabled",
        "-Dlibarchive:lz4=enabled",
        "-Dlibarchive:zstd=enabled",
        "-Dlibarchive:lzma=enabled",
        "-Dlibarchive:iconv=disabled",
        "--wrap-mode=forcefallback",
    ]
    if system == "windows":
        args += ["--vsenv", "-Db_vscrt=md"]
    elif is_cross_profile(profile):
        args += ["--cross-file", str(root / "meson" / "cross" / "linux-x64-gcc.ini")]
    if (bdir / "build.ninja").exists():
        args.append("--reconfigure")
    args.append(str(root))
    run(args, cwd=root, env=task_env(root, profile))


def build(root: Path, profile: str) -> None:
    validate_profile(profile)
    run(["meson", "compile", "-C", str(build_dir(root, profile))], cwd=root, env=task_env(root, profile))


def test_build(root: Path, profile: str) -> None:
    validate_profile(profile)
    system = profile_system(profile)
    bdir = build_dir(root, profile)
    exe_name = "lunalauncher.exe" if system == "windows" else "lunalauncher"
    exe = bdir / "launcher" / exe_name
    if not exe.exists():
        raise SystemExit(f"Build executable not found: {exe}")
    run([str(exe)], cwd=bdir, env=task_env(root, profile))


def test_install(root: Path, profile: str) -> None:
    validate_profile(profile)
    system = profile_system(profile)
    idir = install_dir(root, profile)
    if system == "windows":
        exe = idir / "lunalauncher.exe"
    elif system == "macos":
        exe = idir / "lunalauncher"
    else:
        exe = idir / "bin" / "lunalauncher"
    if not exe.exists():
        raise SystemExit(f"Installed executable not found: {exe}")
    run([str(exe)], cwd=idir, env=task_env(root, profile))


def clean(root: Path, profile: str) -> None:
    validate_profile(profile)
    bdir = build_dir(root, profile)
    if bdir.exists():
        shutil.rmtree(bdir)
        print(f"Removed {bdir}")


def clean_all(root: Path) -> None:
    for d in root.iterdir():
        if d.is_dir() and (d.name.startswith("build-meson-") or d.name.startswith("install-")):
            shutil.rmtree(d)
            print(f"Removed {d}")


def copy_runtime_dlls(root: Path, profile: str) -> None:
    return


def deploy(root: Path, profile: str) -> None:
    validate_profile(profile)
    system = profile_system(profile)
    if system == "windows":
        deploy_windows(root, profile)
    elif system == "macos":
        deploy_macos(root, profile)
    else:
        print(f"No deploy step for {profile}")


def deploy_windows(root: Path, profile: str) -> None:
    mode = profile_mode(profile)
    idir = install_dir(root, profile)
    env = task_env(root, profile)
    windeployqt = qt_tools_root(root) / "bin" / "windeployqt.exe"
    if not windeployqt.exists():
        raise SystemExit(f"missing windeployqt: {windeployqt}")

    for exe_name in ["lunalauncher.exe", "lunalauncher_filelink.exe"]:
        exe = idir / exe_name
        if not exe.exists():
            continue
        run(
            [
                str(windeployqt),
                f"--{mode}",
                "--no-opengl-sw",
                "--no-quick-import",
                "--no-system-d3d-compiler",
                "--no-system-dxc-compiler",
                str(exe),
            ],
            cwd=idir,
            env=env,
        )
    copy_runtime_dlls(root, profile)


def deploy_macos(root: Path, profile: str) -> None:
    idir = install_dir(root, profile)
    env = task_env(root, profile)
    qt_target = qt_target_root(root, profile)
    macdeployqt = qt_target / "bin" / "macdeployqt"
    if not macdeployqt.exists():
        raise SystemExit(f"missing macdeployqt: {macdeployqt}")

    app_name = "LunaLauncher"
    app_bundle = idir / f"{app_name}.app"
    contents = app_bundle / "Contents"
    macos_dir = contents / "MacOS"
    resources_dir = contents / "Resources"

    app_bundle.mkdir(exist_ok=True)
    contents.mkdir(exist_ok=True)
    macos_dir.mkdir(exist_ok=True)
    resources_dir.mkdir(exist_ok=True)

    exe = idir / "lunalauncher"
    if exe.exists():
        shutil.copy2(exe, macos_dir / "lunalauncher")

    icns = idir / "resource" / "lunalauncher.icns"
    if icns.exists():
        shutil.copy2(icns, resources_dir / "lunalauncher.icns")

    jars_dir = idir / "jars"
    if jars_dir.exists():
        dest_jars = macos_dir / "jars"
        if dest_jars.exists():
            shutil.rmtree(dest_jars)
        shutil.copytree(jars_dir, dest_jars)

    resource_dir = idir / "resource"
    if resource_dir.exists():
        dest_resource = macos_dir / "resource"
        if dest_resource.exists():
            shutil.rmtree(dest_resource)
        shutil.copytree(resource_dir, dest_resource)

    plist = contents / "Info.plist"
    plist.write_text(f"""<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>lunalauncher</string>
    <key>CFBundleIconFile</key>
    <string>lunalauncher</string>
    <key>CFBundleIdentifier</key>
    <string>org.lunalauncher.LunaLauncher</string>
    <key>CFBundleName</key>
    <string>{app_name}</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>11.0.2</string>
    <key>CFBundleVersion</key>
    <string>11.0.2</string>
    <key>NSHighResolutionCapable</key>
    <true/>
</dict>
</plist>
""")

    run(
        [str(macdeployqt), str(app_bundle), "-verbose=1"],
        cwd=idir,
        env=env,
    )

    frameworks_dir = contents / "Frameworks"
    frameworks_dir.mkdir(exist_ok=True)
    exe_path = macos_dir / "lunalauncher"
    _copy_dylibs_recursive(root, profile, exe_path, frameworks_dir, env)

    print(f"Deployed {app_bundle}")


def _copy_dylibs_recursive(root: Path, profile: str, binary: Path, frameworks_dir: Path, env: dict, seen: set | None = None) -> None:
    if seen is None:
        seen = set()
    result = subprocess.run(
        ["otool", "-L", str(binary)],
        capture_output=True, text=True, env=env,
    )
    for line in result.stdout.splitlines():
        line = line.strip()
        if "@rpath/" not in line or ".dylib" not in line:
            continue
        dylib_name = line.split("@rpath/")[1].split(" ")[0]
        if dylib_name.startswith("Qt") or dylib_name.startswith("libc++"):
            continue
        if dylib_name in seen:
            continue
        seen.add(dylib_name)
        dest = frameworks_dir / dylib_name
        if not dest.exists():
            dylib_path = _find_dylib(root, profile, dylib_name)
            if dylib_path and dylib_path.exists():
                shutil.copy2(dylib_path, dest)
                print(f"Copied {dylib_name}")
            else:
                print(f"WARNING: dylib not found: {dylib_name}")
                continue
        _copy_dylibs_recursive(root, profile, dest, frameworks_dir, env, seen)


def _find_dylib(root: Path, profile: str, dylib_name: str) -> Path | None:
    bdir = build_dir(root, profile)
    candidates = list(bdir.glob(f"**/{dylib_name}"))
    if candidates:
        return candidates[0]
    pixi_lib = root / ".pixi" / "envs" / "default" / "lib" / dylib_name
    if pixi_lib.exists():
        return pixi_lib
    return None


def install(root: Path, profile: str) -> None:
    validate_profile(profile)
    idir = install_dir(root, profile)
    if idir.exists():
        shutil.rmtree(idir)
    run(["meson", "install", "-C", str(build_dir(root, profile)), "--no-rebuild"], cwd=root, env=task_env(root, profile))
    deploy(root, profile)


def main() -> None:
    if len(sys.argv) < 2:
        raise SystemExit("usage: pixi_meson.py <install-qt|configure|build|install|deploy|test|test-install|clean|clean-all> [profile]")

    root = repo_root()
    command = sys.argv[1]
    profile = sys.argv[2] if len(sys.argv) > 2 else "release"

    if command == "install-qt":
        install_qt_target(root, profile)
    elif command == "configure":
        configure(root, profile)
    elif command == "build":
        build(root, profile)
    elif command == "install":
        install(root, profile)
    elif command == "deploy":
        deploy(root, profile)
    elif command == "test":
        test_build(root, profile)
    elif command == "test-install":
        test_install(root, profile)
    elif command == "clean":
        clean(root, profile)
    elif command == "clean-all":
        clean_all(root)
    else:
        raise SystemExit(f"unknown command: {command}")


if __name__ == "__main__":
    main()
