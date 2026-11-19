#!/usr/bin/env python3
"""Deploy Pixi/Meson runtime dependencies into an AppImage, portable tarball or AppDir."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import tempfile
import tarfile
import urllib.request

import pixi_meson


def digest(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def tool(root: Path, name: str, record: dict) -> Path:
    cache = root / ".cache" / "appimage-tools" / record["sha256"]
    cache.mkdir(parents=True, exist_ok=True)
    path = cache / ("runtime" if name == "runtime" else f"{name}.AppImage")
    if not path.is_file() or digest(path) != record["sha256"]:
        print(f"Downloading pinned AppImage tool: {name}", flush=True)
        with tempfile.NamedTemporaryFile(dir=cache, delete=False) as temporary:
            try:
                with urllib.request.urlopen(record["url"], timeout=120) as response:
                    shutil.copyfileobj(response, temporary)
                temporary.close()
                candidate = Path(temporary.name)
                if digest(candidate) != record["sha256"]:
                    raise RuntimeError(f"{name}: checksum mismatch; update the reviewed tool lock explicitly")
                candidate.replace(path)
            finally:
                Path(temporary.name).unlink(missing_ok=True)
    path.chmod(0o755)
    return path


def shipped_version(executable: Path, env: dict[str, str]) -> str:
    result = subprocess.run([str(executable), "--version"], env={**env, "QT_QPA_PLATFORM": "offscreen"},
                            check=True, capture_output=True, text=True, timeout=30)
    match = re.search(r"(?:^|\n)LunaLauncher (\d+\.\d+\.\d+(?:[-+][A-Za-z0-9.-]+)?)", result.stdout)
    if not match:
        raise RuntimeError(f"Cannot determine shipped launcher version: {result.stdout!r}")
    return match.group(1)


def write_manifest(image: Path, version: str, neo: bool, architecture: str) -> Path:
    checksum = digest(image)
    manifest = {"schemaVersion": 1, "protocol": 1, "version": version,
                "artifacts": [{"platform": "linux", "format": "appimage", "architecture": architecture,
                               "neoUi": neo, "asset": image.name, "size": image.stat().st_size, "sha256": checksum}]}
    destination = image.parent / f"linux-{architecture}-appimage-update.json"
    destination.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    image.with_suffix(image.suffix + ".sha256").write_text(f"{checksum}  {image.name}\n", encoding="utf-8")
    return destination


def portable_archive(appdir: Path, output: Path, version: str, neo: bool, env: dict[str, str]) -> Path:
    """Reuse the deployed runtime; record actual ELF requirements, not distro names."""
    versions = set()
    for path in appdir.rglob("*"):
        if path.is_file() and not path.is_symlink():
            with path.open("rb") as stream:
                if stream.read(4) != b"\x7fELF":
                    continue
            result = subprocess.run(["readelf", "--version-info", str(path)], env=env, check=True, capture_output=True, text=True)
            versions.update(re.findall(r"\bGLIBC_([0-9]+\.[0-9]+(?:\.[0-9]+)?)\b", result.stdout))
    if not versions:
        raise RuntimeError("Cannot determine the portable runtime's minimum glibc version")
    minimum = max(versions, key=lambda value: tuple(map(int, value.split("."))))
    (appdir / "usr/share/luna-portable.requirements").write_text(f"x64\n{minimum}\n", encoding="utf-8")
    files = []
    for path in sorted(appdir.rglob("*")):
        if path.is_symlink():
            link = os.readlink(path)
            if os.path.isabs(link) or not path.resolve().is_relative_to(appdir) or not path.exists():
                raise RuntimeError(f"External or broken portable symlink: {path} -> {link}")
            files.append({"path": path.relative_to(appdir).as_posix(), "link": link})
        elif path.is_file():
            files.append({"path": path.relative_to(appdir).as_posix(), "size": path.stat().st_size, "sha256": digest(path)})
    metadata = {"schemaVersion": 1, "protocol": 1, "version": version, "format": "portable-tar",
                "architecture": "x64", "libc": "glibc", "minGlibc": minimum, "neoUi": neo, "files": files}
    (appdir / ".luna-portable.json").write_text(json.dumps(metadata, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    unpacked = sum(path.stat().st_size for path in appdir.rglob("*") if path.is_file() and not path.is_symlink())
    archive = output / f"LunaLauncher-Linux-x64-{version}{'-webui' if neo else ''}-portable.tar.gz"
    with tarfile.open(archive, "w:gz", compresslevel=6) as bundle:
        for path in sorted(appdir.rglob("*")):
            bundle.inodes.clear()  # Preserve symbolic links, emit regular files without tar hard links.
            info = bundle.gettarinfo(str(path), arcname=path.relative_to(appdir).as_posix())
            info.uid = info.gid = 0
            info.uname = info.gname = "root"
            if info.isfile():
                with path.open("rb") as stream:
                    bundle.addfile(info, stream)
            else:
                bundle.addfile(info)
    checksum = digest(archive)
    artifact = {"platform": "linux", "format": "portable-tar", "architecture": "x64", "libc": "glibc",
                "minGlibc": minimum, "neoUi": neo, "asset": archive.name, "size": archive.stat().st_size,
                "unpackedSize": unpacked, "sha256": checksum}
    manifest = output / "linux-x64-portable-update.json"
    manifest.write_text(json.dumps({"schemaVersion": 1, "protocol": 1, "version": version, "artifacts": [artifact]}, indent=2) + "\n", encoding="utf-8")
    archive.with_name(archive.name + ".sha256").write_text(f"{checksum}  {archive.name}\n", encoding="utf-8")
    # Exercise the actual tar contents with standard host paths, independent of Pixi.
    with tempfile.TemporaryDirectory(prefix="luna-portable-check-") as check_dir:
        with tarfile.open(archive) as bundle:
            bundle.extractall(check_dir, filter="data")
        runtime_env = {key: value for key, value in os.environ.items() if key not in {
            "LD_LIBRARY_PATH", "LD_PRELOAD", "QT_PLUGIN_PATH", "QT_QPA_PLATFORM_PLUGIN_PATH", "APPDIR", "APPIMAGE"}}
        runtime_env.update(PATH="/usr/bin:/bin", QT_QPA_PLATFORM="offscreen")
        for entry, product in (("classic", "LunaLauncher"), ("updater", "LunaLauncherUpdater")):
            result = subprocess.run([str(Path(check_dir) / "AppRun"), "--luna-entry", entry, "--version"],
                                    env=runtime_env, capture_output=True, text=True, timeout=60)
            if result.returncode or f"{product} {version}" not in result.stdout:
                raise RuntimeError(f"Portable {entry} failed: {result.stdout}\n{result.stderr}")
    print(manifest, flush=True)
    print(archive, flush=True)
    return archive


def package(root: Path, profile: str, output: Path | None = None, format: str = "appimage") -> Path:
    if platform.system() != "Linux" or platform.machine() != "x86_64":
        raise SystemExit("AppImage packaging currently requires native Linux x86_64 (WSL2 is supported)")
    if pixi_meson.is_cross_profile(profile):
        raise SystemExit("Use native Linux release, not the Windows Linux cross profile")
    installation = pixi_meson.install_dir(root, profile)
    env = pixi_meson.task_env(root, profile)
    # Meson strips build RPATHs on install. Until linuxdeploy has populated
    # AppDir, the installed binary needs the pinned SDK/runtime search paths.
    env["LD_LIBRARY_PATH"] = ":".join(filter(None, [
        str(pixi_meson.qt_target_root(root, profile) / "lib"),
        str(Path(env["CONDA_PREFIX"]) / "lib"), env.get("LD_LIBRARY_PATH"),
    ]))
    binary = installation / "bin" / "lunalauncher"
    if not binary.is_file() or not (installation / "bin/lunalauncher_updater").is_file():
        raise RuntimeError("Missing Meson launcher/updater installation; run pixi run install release first")
    version = shipped_version(binary, env)
    neo = (installation / "bin/luna-neo-ui").is_file()
    if neo != pixi_meson.webui_enabled():
        raise RuntimeError("Installed NeoUI capability does not match task; use the correct clean install tree")
    output = (output or root / "release-assets").resolve()
    output.mkdir(parents=True, exist_ok=True)
    records = json.loads((root / "scripts/appimage-tools.json").read_text(encoding="utf-8"))["x64"]
    with tempfile.TemporaryDirectory(prefix="luna-appimage-") as temporary:
        work = Path(temporary)
        appdir = work / "LunaLauncher.AppDir"
        usr = appdir / "usr"
        # Reinstall into DESTDIR instead of copying unmanaged files from a
        # developer's runnable install tree (which can contain user profiles).
        staging = work / "meson-install"
        pixi_meson.run(["meson", "install", "-C", str(pixi_meson.build_dir(root, profile)),
                        "--no-rebuild", "--destdir", str(staging)], cwd=root, env=env)
        clean_installation = staging / installation.relative_to("/")
        if not (clean_installation / "bin/lunalauncher").is_file():
            raise RuntimeError("Meson install prefix differs from the selected Pixi install tree; reconfigure the profile")
        # Copy only a clean application tree. Never distribute local instance/account state.
        forbidden = {"UserData", "instances", "accounts.json", ".luna-data-dir", ".luna-use-system-data",
                     ".luna-install.json", ".luna-update.json", "portable.txt", "portable.txt.disabled"}
        def ignore(directory, names):
            return set(names) & forbidden if Path(directory) == clean_installation else set()
        shutil.copytree(clean_installation, usr, symlinks=True, ignore=ignore)
        # Static fallback dependencies also install SDK headers and archives.
        # They are build inputs, not launcher runtime files.
        for relative in ("include", "lib/pkgconfig", "lib/cmake", "lib64/pkgconfig", "lib64/cmake"):
            directory = usr / relative
            if directory.is_dir() and not directory.is_symlink():
                shutil.rmtree(directory)
        for directory in (usr / "lib", usr / "lib64"):
            if directory.is_dir():
                for archive in directory.rglob("*.a"):
                    archive.unlink()
                for sdk in list(directory.rglob("pkgconfig")) + list(directory.rglob("cmake")):
                    if sdk.is_dir() and not sdk.is_symlink():
                        shutil.rmtree(sdk)
        if (usr / "bin/luna-neo-ui").is_file() != neo:
            raise RuntimeError("Meson install manifest and installed NeoUI capability differ; reconfigure and install the selected profile")
        if shipped_version(usr / "bin/lunalauncher", env) != version:
            raise RuntimeError("Runnable install tree is stale; install this build before packaging")
        # The Qt plugin normally deploys only graphical platform backends.
        # Update health checks also need a real headless Qt backend.
        platforms = usr / "plugins/platforms"
        platforms.mkdir(parents=True, exist_ok=True)
        shutil.copy2(pixi_meson.qt_target_root(root, profile) / "plugins/platforms/libqoffscreen.so", platforms)
        cf_webview = usr / "bin/luna-cf-webview"
        uses_webkit = neo or cf_webview.is_file()
        tools = {name: tool(root, name, record) for name, record in records.items()
                 if (name != "gtk" or uses_webkit) and (format == "appimage" or name not in {"appimagetool", "runtime"})}
        if cf_webview.is_file():
            cf_webview.rename(cf_webview.with_suffix(".bin"))
            cf_webview.write_text('#!/bin/sh\nset -eu\nAPPDIR="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"\n'
                                  'export APPDIR\nexec "$APPDIR/AppRun" --luna-entry cf-webview "$@"\n', encoding="utf-8")
            cf_webview.chmod(0o755)
        desktop = appdir / "org.lunalauncher.LunaLauncher.desktop"
        desktop.write_text("[Desktop Entry]\nType=Application\nName=Luna Launcher\nExec=lunalauncher %U\n"
                           "Icon=org.lunalauncher.LunaLauncher\nCategories=Game;\nTerminal=false\n"
                           "MimeType=application/zip;application/x-modrinth-modpack+zip;x-scheme-handler/lunalauncher;\n",
                           encoding="utf-8")
        icon = appdir / "org.lunalauncher.LunaLauncher.png"
        shutil.copy2(root / "program_info/org.lunalauncher.LunaLauncher_256.png", icon)
        (appdir / ".DirIcon").symlink_to(icon.name)
        # Plugin is invoked by linuxdeploy by executable name. Both tools run without FUSE.
        plugin = work / "linuxdeploy-plugin-qt"
        plugin.write_text(f'#!/bin/sh\nexec "{tools["qt"]}" --appimage-extract-and-run "$@"\n', encoding="utf-8")
        plugin.chmod(0o755)
        webkit = None
        if uses_webkit:
            gtk = work / "linuxdeploy-plugin-gtk"
            # GTK/WebKit are provided by the distro SDK, not conda's GLib SDK.
            # Its .pc files refer to schemas/modules absent from the Pixi env.
            host_pc = "/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/lib/pkgconfig:/usr/share/pkgconfig"
            gtk.write_text('#!/bin/sh\n'
                           f'export PKG_CONFIG_PATH="{host_pc}" PKG_CONFIG_LIBDIR="{host_pc}"\n'
                           'export PATH="/usr/bin:/bin:$PATH"\n'
                           f'exec "{tools["gtk"]}" "$@"\n', encoding="utf-8")
            gtk.chmod(0o755)
            for location in ("/usr/lib/x86_64-linux-gnu", "/usr/lib64", "/usr/lib", "/usr/libexec"):
                candidate = Path(location) / "webkit2gtk-4.1"
                if (candidate / "WebKitWebProcess").is_file() and (candidate / "WebKitNetworkProcess").is_file():
                    webkit = usr / candidate.relative_to("/usr")
                    shutil.copytree(candidate, webkit, symlinks=True)
                    break
            if webkit is None:
                raise RuntimeError("WebKit AppImage entries require WebKitGTK 4.1 helper processes from the host development packages")
        # linuxdeploy discovers plugins by scanning every PATH directory. WSL
        # inherits Windows directories (including large Downloads folders),
        # which are neither packaging tools nor a reproducible search path.
        deploy_path = [str(work), str(Path(env["CONDA_PREFIX"]) / "bin"),
                       str(pixi_meson.qt_target_root(root, profile) / "bin"),
                       "/usr/local/bin", "/usr/bin", "/bin"]
        env.update({"PATH": os.pathsep.join(deploy_path), "QMAKE": str(pixi_meson.qt_target_root(root, profile) / "bin/qmake"),
                    "EXTRA_QT_PLUGINS": "platforms;imageformats;iconengines;platformthemes;platforminputcontexts;xcbglintegrations;egldeviceintegrations;multimedia;tls;wayland-shell-integration;wayland-decoration-client;wayland-graphics-integration-client",
                    "ARCH": "x86_64", "APPIMAGE_EXTRACT_AND_RUN": "1", "NO_STRIP": "1"})
        # Resolve dependencies of main entries plus libraries loaded at runtime
        # (PCL.Download) and nested tools (YukariConnect). Keep their install paths.
        executables = []
        for path in sorted((usr / "bin").iterdir()):
            if path.is_file():
                with path.open("rb") as stream:
                    if stream.read(4) == b"\x7fELF":
                        executables.extend(["--executable", str(path)])
        command = [str(tools["linuxdeploy"]), "--appimage-extract-and-run", "--appdir", str(appdir),
                   "--desktop-file", str(desktop), "--icon-file", str(icon), *executables,
                   "--deploy-deps-only", str(usr), "--custom-apprun", str(root / "scripts/appimage/AppRun"), "--plugin", "qt"]
        if uses_webkit:
            command.extend(["--plugin", "gtk"])
            env["DEPLOY_GTK_VERSION"] = "3"
        # Qt's OpenSSL TLS backend uses dlopen, so ldd alone cannot discover it.
        for name in ("libssl.so.3", "libcrypto.so.3", "libstdc++.so.6", "libgcc_s.so.1"):
            library = Path(env["CONDA_PREFIX"]) / "lib" / name
            if not library.is_file():
                raise RuntimeError(f"Missing pinned runtime: {library}")
            command.extend(["--library", str(library)])
        pixi_meson.run(command, cwd=work, env=env)
        # Own AppRun defines entry selection and data-root handling; no upstream launcher scripts.
        (appdir / "AppRun").unlink(missing_ok=True)
        shutil.copy2(root / "scripts/appimage/AppRun", appdir / "AppRun")
        (appdir / "AppRun").chmod(0o755)
        if format == "appimage":
            (usr / "share/luna-appimage.json").write_text(json.dumps({"protocol": 1, "version": version, "neoUi": neo}) + "\n", encoding="utf-8")
        # Build-environment absolute RPATHs must not be needed at runtime.
        clean_env = env.copy()
        clean_env["LD_LIBRARY_PATH"] = str(usr / "lib")
        for path in sorted(usr.rglob("*")):
            if not path.is_file() or path.is_symlink():
                continue
            with path.open("rb") as stream:
                if stream.read(4) != b"\x7fELF":
                    continue
            check = subprocess.run(["ldd", str(path)], env=clean_env, capture_output=True, text=True)
            details = check.stdout + check.stderr
            if check.returncode and ("not a dynamic executable" in details or "statically linked" in details):
                continue
            if check.returncode or "not found" in details or str(root) + "/" in details:
                raise RuntimeError(f"Unresolved or build-tree AppImage dependencies: {path}\n{details}")
        if format == "appdir":
            destination = output / "LunaLauncher.AppDir"
            # A caller can compose this clean deployment into a system package.
            # Refuse to merge a previous tree or user-managed installation.
            shutil.copytree(appdir, destination, symlinks=True)
            return destination
        if format == "portable":
            return portable_archive(appdir, output, version, neo, env)
        image = output / f"LunaLauncher-Linux-x64-{version}{'-webui' if neo else ''}.AppImage"
        pixi_meson.run([str(tools["appimagetool"]), "--appimage-extract-and-run", "--runtime-file", str(tools["runtime"]),
                       "--mksquashfs-opt", "-processors", "--mksquashfs-opt", "1", str(appdir), str(image)], cwd=work, env=env)
        image.chmod(0o755)
        # Exercise the actual runtime and the deployed Qt libraries outside Pixi's LD search paths.
        runtime_env = {key: value for key, value in os.environ.items()
                       if key not in {"LD_LIBRARY_PATH", "LD_PRELOAD", "QT_PLUGIN_PATH", "QT_QPA_PLATFORM_PLUGIN_PATH", "APPDIR", "APPIMAGE"}}
        runtime_env["PATH"] = "/usr/bin:/bin"
        runtime_env["QT_QPA_PLATFORM"] = "offscreen"
        for entry, product in (("classic", "LunaLauncher"), ("updater", "LunaLauncherUpdater")):
            check = subprocess.run([str(image), "--appimage-extract-and-run", "--luna-entry", entry, "--version"],
                                   env=runtime_env, capture_output=True, text=True, timeout=60)
            if check.returncode:
                raise RuntimeError(f"Packaged {entry} exited {check.returncode}:\n{check.stdout}\n{check.stderr}")
            if f"{product} {version}" not in check.stdout:
                raise RuntimeError(f"Packaged {entry} reported unexpected version: {check.stdout}")
        print(write_manifest(image, version, neo, "x64"), flush=True)
        print(image, flush=True)
        return image


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("profile", choices=["release"], nargs="?", default="release")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--format", choices=["appimage", "portable", "appdir"], default="appimage")
    parser.add_argument("--build", action="store_true", help="Configure, build and install with the Pixi/Meson wrapper before packaging")
    args = parser.parse_args()
    root = pixi_meson.repo_root()
    if platform.system() != "Linux" or platform.machine() != "x86_64":
        raise SystemExit("AppImage packaging requires native Linux x86_64 (WSL2 is supported)")
    if args.build:
        pixi_meson.configure(root, args.profile)
        pixi_meson.build(root, args.profile)
        pixi_meson.install(root, args.profile)
    package(root, args.profile, args.output, args.format)
