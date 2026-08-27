#!/usr/bin/env python3
import os
import platform
import re
import shutil
import subprocess
import sys
from pathlib import Path


QT_VERSION = "6.10.1"
QT_WINDOWS_ARCH = "msvc2022_64"
QT_LINUX_ARCH = "gcc_64"
QT_MACOS_ARCH = "macos"
DEFAULT_MACOS_DEPLOYMENT_TARGET = "13.0"
WINDOWS_PROFILES = {"release", "debug"}
LINUX_CROSS_PROFILE = "linux-x64-gcc-release"
PROFILES = WINDOWS_PROFILES | {LINUX_CROSS_PROFILE}


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
    if system == "linux":
        arch = QT_LINUX_ARCH
    elif system == "macos":
        arch = QT_MACOS_ARCH
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
    qt_tools = qt_tools_root(root) if is_cross_profile(profile) else qt_target_root(root, profile)
    qt_target = qt_target_root(root, profile)
    path_entries = [
        entry
        for entry in env.get("PATH", "").split(os.pathsep)
        if "msys64" not in entry.lower()
    ]
    env["PATH"] = os.pathsep.join([str(qt_tools / "bin")] + path_entries)
    cmake_prefix_paths = [str(qt_target / "lib" / "cmake")]
    if platform.system().lower() == "linux" and not is_cross_profile(profile) and env.get("CONDA_PREFIX"):
        cmake_prefix_paths.append(env["CONDA_PREFIX"])
    env["CMAKE_PREFIX_PATH"] = os.pathsep.join(cmake_prefix_paths)
    if platform.system().lower() == "linux" and not is_cross_profile(profile) and env.get("CONDA_PREFIX"):
        env["PKG_CONFIG_PATH"] = os.pathsep.join(
            [
                str(Path(env["CONDA_PREFIX"]) / "lib" / "pkgconfig"),
                str(Path(env["CONDA_PREFIX"]) / "share" / "pkgconfig"),
            ]
        )
        env["PKG_CONFIG_LIBDIR"] = env["PKG_CONFIG_PATH"]
    elif profile_system(profile) == "windows" or is_cross_profile(profile):
        env["PKG_CONFIG_PATH"] = ""
        env["PKG_CONFIG_LIBDIR"] = str(root / ".meson-empty-pkgconfig")
    if profile_system(profile) == "windows":
        env.setdefault("CC", "cl")
        env.setdefault("CXX", "cl")
    else:
        env.pop("CC", None)
        env.pop("CXX", None)
    if profile_system(profile) == "macos":
        env["MACOSX_DEPLOYMENT_TARGET"] = os.environ.get(
            "LUNA_MACOS_DEPLOYMENT_TARGET",
            os.environ.get("MACOSX_DEPLOYMENT_TARGET", DEFAULT_MACOS_DEPLOYMENT_TARGET),
        )
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
    if profile_system(profile) == "windows":
        install_qt_tools(root)
        return
    if profile_system(profile) == "macos":
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


def patch_macos_qt_cmake_configs(root: Path, profile: str) -> None:
    if profile_system(profile) != "macos":
        return

    cmake_root = qt_target_root(root, profile) / "lib" / "cmake"
    property_pattern = re.compile(r'(INTERFACE_INCLUDE_DIRECTORIES\s+")([^"]*)(")')
    patched_files = 0

    for target_file in sorted(cmake_root.glob("Qt6*/Qt6*Targets.cmake")):
        original = target_file.read_text(encoding="utf-8")

        def remove_framework_roots(match: re.Match[str]) -> str:
            paths = match.group(2).split(";")
            framework_roots = [path for path in paths if path.rstrip("/").endswith(".framework")]
            if not framework_roots:
                return match.group(0)

            for framework_root in framework_roots:
                header_prefix = framework_root.rstrip("/") + "/"
                if not any(path.startswith(header_prefix) and path.endswith("Headers") for path in paths):
                    raise SystemExit(
                        f"Refusing to patch unexpected Qt framework include metadata in {target_file}: "
                        f"{framework_root} has no matching Headers entry"
                    )

            filtered = [path for path in paths if path not in framework_roots]
            return match.group(1) + ";".join(filtered) + match.group(3)

        updated = property_pattern.sub(remove_framework_roots, original)
        if updated != original:
            target_file.write_text(updated, encoding="utf-8")
            patched_files += 1

    print(f"Patched Qt framework include metadata in {patched_files} CMake target file(s)")


def configure(root: Path, profile: str) -> None:
    validate_profile(profile)
    install_qt_target(root, profile)
    patch_macos_qt_cmake_configs(root, profile)
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
        exe = idir / "LunaLauncher.app" / "Contents" / "MacOS" / "lunalauncher"
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
    if system == "macos":
        idir = install_dir(root, profile)
        app_bundle = idir / "LunaLauncher.app"
        macdeployqt = qt_target_root(root, profile) / "bin" / "macdeployqt"
        if not app_bundle.is_dir():
            raise SystemExit(f"missing macOS app bundle: {app_bundle}")
        if not macdeployqt.exists():
            raise SystemExit(f"missing macdeployqt: {macdeployqt}")
        run(
            [str(macdeployqt), str(app_bundle), "-always-overwrite", "-verbose=2"],
            cwd=idir,
            env=task_env(root, profile),
        )
        return
    if system != "windows":
        print(f"No deploy step for {profile}")
        return
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
