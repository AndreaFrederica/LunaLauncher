#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Package the clean Pixi/Meson deployment as a side-loadable Flatpak.

This is a binary packaging pipeline for GitHub distribution, not a Flathub
source-build manifest. No host filesystem or host process execution is granted.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import tempfile

import pixi_appimage
import pixi_meson


def config(root: Path) -> dict:
    return json.loads((root / "scripts/flatpak/package.json").read_text(encoding="utf-8"))


def host_env() -> dict[str, str]:
    # Packaging tools belong to the host; Pixi's GLib/Qt must not replace theirs.
    env = {k: v for k, v in os.environ.items() if k not in {
        "LD_LIBRARY_PATH", "LD_PRELOAD", "QT_PLUGIN_PATH", "QT_QPA_PLATFORM_PLUGIN_PATH",
        "GIO_MODULE_DIR", "GSETTINGS_SCHEMA_DIR", "APPDIR", "APPIMAGE"}}
    env["PATH"] = "/usr/bin:/bin"
    return env


def flatpak(*args: str, capture: bool = False) -> subprocess.CompletedProcess:
    return subprocess.run(["flatpak", *map(str, args)], env=host_env(), check=True,
                          text=True, capture_output=capture)


def setup(root: Path) -> None:
    cfg = config(root)
    flatpak("--user", "remote-add", "--if-not-exists", "flathub", cfg["runtimeRepo"])
    flatpak("--user", "install", "--noninteractive", "-y", "flathub",
            f"{cfg['runtime']}/{cfg['architecture']}/{cfg['runtimeVersion']}")


def validate_links(directory: Path) -> None:
    for path in directory.rglob("*"):
        if path.is_symlink() and (os.path.isabs(os.readlink(path)) or
                                  not path.resolve().is_relative_to(directory) or not path.exists()):
            raise RuntimeError(f"External or broken package symlink: {path}")


def package(root: Path, profile: str, output: Path | None = None, repo: Path | None = None) -> Path:
    cfg = config(root)
    output = (output or root / "release-assets").resolve()
    output.mkdir(parents=True, exist_ok=True)
    env = pixi_meson.task_env(root, profile)
    with tempfile.TemporaryDirectory(prefix="luna-flatpak-") as temporary:
        work = Path(temporary)
        stage = work / "app"
        # No compilation occurs in flatpak build. The runtime is sufficient for
        # binary staging; a second, multi-GB development SDK is unnecessary.
        flatpak("build-init", "--arch=" + cfg["architecture"], str(stage), cfg["id"],
                cfg["runtime"], cfg["runtime"], cfg["runtimeVersion"])
        files = stage / "files"
        deployed = pixi_appimage.package(root, profile, work / "deployment", "appdir")
        runtime = files / "runtime"
        shutil.move(deployed, runtime)
        if any((runtime / name).exists() for name in (".luna-portable.json", "usr/share/luna-appimage.json",
                                                     "usr/UserData", "usr/portable.txt")):
            raise RuntimeError("Flatpak payload contains portable/update state")
        neo = (runtime / "usr/bin/luna-neo-ui").is_file()
        # A usable Java 21 is present on first launch, without access to host
        # JDKs. Other game Java versions use the launcher's managed downloads.
        jre = files / "jdk/21"
        jre.parent.mkdir(parents=True)
        pixi_meson.run(["jlink", "--add-modules", "ALL-MODULE-PATH", "--strip-debug",
                        "--no-header-files", "--no-man-pages", "--compress=zip-6",
                        "--output", str(jre)], cwd=root, env=env)
        bindir = files / "bin"
        bindir.mkdir()
        wrapper = bindir / cfg["command"]
        shutil.copy2(root / "scripts/flatpak/lunalauncher", wrapper)
        wrapper.chmod(0o755)
        # Export only application metadata, not deployment plugin SDK data.
        share = files / "share"
        desktop = share / "applications" / (cfg["id"] + ".desktop")
        desktop.parent.mkdir(parents=True)
        original = pixi_meson.build_dir(root, profile) / "program_info" / desktop.name
        text = original.read_text(encoding="utf-8")
        if f"Exec={cfg['command']} %U" not in text:
            raise RuntimeError("Desktop entry no longer matches the Flatpak command")
        desktop.write_text(text.rstrip() + "\nActions=classic;\n\n[Desktop Action classic]\n"
                           "Name=Classic UI\nExec=lunalauncher --luna-entry classic\n", encoding="utf-8")
        icon = share / "icons/hicolor/256x256/apps" / (cfg["id"] + ".png")
        icon.parent.mkdir(parents=True)
        shutil.copy2(root / "program_info" / (cfg["id"] + "_256.png"), icon)
        metainfo = share / "metainfo" / (cfg["id"] + ".metainfo.xml")
        metainfo.parent.mkdir(parents=True)
        shutil.copy2(pixi_meson.build_dir(root, profile) / "program_info" / metainfo.name, metainfo)
        # The clean deployed executable is authoritative about its version.
        probe_env = {**env, "LD_LIBRARY_PATH": str(runtime / "usr/lib")}
        version = pixi_appimage.shipped_version(runtime / "usr/bin/lunalauncher", probe_env)
        record = {"schemaVersion": 1, "version": version, "neoUi": neo,
                  "runtime": f"{cfg['runtime']}/{cfg['architecture']}/{cfg['runtimeVersion']}",
                  "sourceCommit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip(),
                  "sourceDirty": subprocess.run(["git", "diff", "--quiet", "HEAD", "--ignore-submodules"], cwd=root).returncode != 0}
        (share / "luna-flatpak.json").write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
        validate_links(files)
        elf_paths = []
        for path in sorted(files.rglob("*")):
            if path.is_file() and not path.is_symlink():
                with path.open("rb") as stream:
                    if stream.read(4) == b"\x7fELF":
                        elf_paths.append("/app/" + path.relative_to(files).as_posix())
        (share / "luna-flatpak-elfs.txt").write_text("\n".join(elf_paths) + "\n", encoding="utf-8")
        flatpak("build-finish", "--command=" + cfg["command"], *cfg["finishArgs"], str(stage))
        repo = (repo or root / ".cache/flatpak-repo").resolve()
        signing = []
        public_keys = []
        if key := os.environ.get("LUNA_FLATPAK_GPG_KEY"):
            signing = ["--gpg-sign=" + key]
            key_file = work / "signing-key.gpg"
            with key_file.open("wb") as stream:
                subprocess.run(["gpg", "--export", key], env=host_env(), stdout=stream, check=True)
            if not key_file.stat().st_size:
                raise RuntimeError("The Flatpak signing public key could not be exported")
            public_keys = ["--gpg-keys=" + str(key_file)]
        flatpak("build-export", *signing, str(repo), str(stage), cfg["branch"])
        bundle = output / f"LunaLauncher-Linux-x64-{version}{'-webui' if neo else ''}.flatpak"
        origin = []
        if url := os.environ.get("LUNA_FLATPAK_REPO_URL"):
            if not signing or not url.startswith("https://"):
                raise RuntimeError("A published Flatpak origin requires HTTPS and LUNA_FLATPAK_GPG_KEY")
            origin = ["--repo-url=" + url]
        flatpak("build-bundle", "--runtime-repo=" + cfg["runtimeRepo"], *public_keys, *origin,
                str(repo), str(bundle), cfg["id"], cfg["branch"])
        commit = repo / "refs/heads/app" / cfg["id"] / cfg["architecture"] / cfg["branch"]
        record["ostreeCommit"] = commit.read_text(encoding="ascii").strip()
        checksum = pixi_appimage.digest(bundle)
        bundle.with_suffix(".flatpak.sha256").write_text(f"{checksum}  {bundle.name}\n", encoding="utf-8")
        record.update(asset=bundle.name, sha256=checksum, size=bundle.stat().st_size)
        (output / "linux-x64-flatpak.json").write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
        print(bundle, flush=True)
        return bundle


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["setup", "package"])
    parser.add_argument("--build", action="store_true")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--repo", type=Path, help="Persistent OSTree repository (default: .cache/flatpak-repo)")
    args = parser.parse_args()
    if platform.system() != "Linux" or platform.machine() != "x86_64":
        raise SystemExit("Flatpak packaging requires native Linux x86_64; WSL2 is supported")
    if not Path("/usr/bin/flatpak").is_file():
        raise SystemExit("Install the host flatpak and desktop build dependencies; see docs/flatpak.md")
    root = pixi_meson.repo_root()
    if args.action == "setup":
        setup(root)
    else:
        if args.build:
            pixi_meson.configure(root, "release")
            pixi_meson.build(root, "release")
            pixi_meson.install(root, "release")
        package(root, "release", args.output, args.repo)


if __name__ == "__main__":
    main()
