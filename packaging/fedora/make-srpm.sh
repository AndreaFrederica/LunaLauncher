#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source_mode="${LUNALAUNCHER_SRPM_SOURCE_MODE:-git}"
output_dir="${1:-$repo_root/dist/fedora}"
mkdir -p "$output_dir"
output_dir="$(cd "$output_dir" && pwd)"
topdir="$output_dir/rpmbuild"
staging_dir="$(mktemp -d /tmp/lunalauncher-srpm.XXXXXX)"
trap 'rm -rf "$staging_dir"' EXIT
mkdir -p "$topdir"/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}

# Read both the spec and project version from the SAME snapshot. In git mode,
# working-tree edits (including edits to this helper) must not alter HEAD sources.
python3 - "$repo_root" "$staging_dir/source" "$source_mode" <<'PY'
import fnmatch
import os
from pathlib import Path
import shutil
import subprocess
import sys

root, destination = map(Path, sys.argv[1:3])
mode = sys.argv[3]
if mode not in {"git", "worktree"}:
    sys.exit("Invalid LUNALAUNCHER_SRPM_SOURCE_MODE: expected git or worktree")


def git(repository, *args):
    return subprocess.check_output(["git", "-C", str(repository), *args])


def gitlinks(repository, revision):
    result = {}
    for record in git(repository, "ls-tree", "-rz", revision).split(b"\0"):
        if not record:
            continue
        metadata, path = record.split(b"\t", 1)
        entry_mode, kind, sha = metadata.split()
        if entry_mode == b"160000":
            result[os.fsdecode(path)] = sha.decode("ascii")
    return result


def require_submodule(repository, path, revision):
    module = repository / path
    try:
        actual_root = Path(os.fsdecode(git(module, "rev-parse", "--show-toplevel")).strip()).resolve()
        if actual_root != module.resolve():
            raise ValueError("submodule is not initialized")
        git(module, "cat-file", "-e", revision + "^{commit}")
    except (subprocess.CalledProcessError, ValueError):
        sys.exit(f"Missing pinned submodule object: {module} ({revision}). "
                 "Run git submodule update --init --recursive before creating the SRPM.")
    return module


# Git already excludes ignored untracked files. Also keep local automation,
# credentials and generated outputs out of explicit worktree snapshots.
def excluded(path):
    patterns = (".git", ".codex-agent-mailbox", ".codex", ".agents", ".aws", ".ssh",
                ".env", ".env.*", "secrets", "build", "build-*", "cmake-build-*",
                "install", "install-*", "dist", "node_modules", ".pixi", "__pycache__")
    return any(fnmatch.fnmatch(part, pattern) for part in Path(path).parts for pattern in patterns)


def disabled_submodules(target):
    # The staged .gitmodules belongs to this exact revision in git mode and
    # to the current source files in worktree mode. Do not consult local Git
    # configuration, which may describe a different checkout or override.
    modules_file = target / ".gitmodules"
    if not modules_file.is_file():
        return set()
    config = subprocess.run(
        ["git", "config", "--file", str(modules_file), "--null", "--get-regexp",
         r"^submodule\..*\.(path|update)$"], stdout=subprocess.PIPE, check=False,
    )
    if config.returncode not in {0, 1}:
        sys.exit(f"Cannot parse submodule configuration: {modules_file}")
    modules = {}
    for record in config.stdout.split(b"\0"):
        if record:
            key, value = record.split(b"\n", 1)
            name, field = os.fsdecode(key).rsplit(".", 1)
            modules.setdefault(name, {})[field] = os.fsdecode(value)
    return {values["path"] for values in modules.values()
            if values.get("update") == "none" and "path" in values}


def snapshot(repository, revision, target):
    target.mkdir(parents=True, exist_ok=True)
    links = gitlinks(repository, revision)
    if mode == "git":
        with subprocess.Popen(["git", "-C", str(repository), "archive", revision], stdout=subprocess.PIPE) as archive:
            subprocess.run(["tar", "-x", "-C", str(target)], stdin=archive.stdout, check=True)
            archive.stdout.close()
            if archive.wait():
                sys.exit(f"git archive failed: {repository} ({revision})")
    else:
        # Index gitlinks can differ from HEAD in an explicit worktree snapshot.
        links = {}
        for record in git(repository, "ls-files", "--stage", "-z").split(b"\0"):
            if not record:
                continue
            metadata, path = record.split(b"\t", 1)
            entry_mode, sha, stage = metadata.split()
            if stage != b"0":
                sys.exit(f"Cannot snapshot an unresolved merge: {repository / os.fsdecode(path)}")
            if entry_mode == b"160000":
                links[os.fsdecode(path)] = sha.decode("ascii")
        paths = set(git(repository, "ls-files", "--cached", "--others", "--exclude-standard", "-z").split(b"\0"))
        for raw_path in sorted(paths):
            if not raw_path:
                continue
            path = os.fsdecode(raw_path)
            if path in links or excluded(path):
                continue
            source = repository / path
            if not source.is_file() and not source.is_symlink():
                continue
            output = target / path
            output.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, output, follow_symlinks=False)
    disabled = disabled_submodules(target)
    for path, pin in links.items():
        if path in disabled:
            print(f"Skipping upstream-disabled submodule: {repository / path} (update=none)", file=sys.stderr)
            continue
        module = require_submodule(repository, path, pin)
        # Worktree mode intentionally includes local submodule edits and new
        # source files; git mode always uses the parent tree's exact gitlink.
        snapshot(module, pin, target / path)


snapshot(root, "HEAD", destination)
PY

spec_file="$staging_dir/source/packaging/fedora/lunalauncher.spec"
version="$(awk '/^Version:/ { print $2; exit }' "$spec_file")"
meson_version="$(sed -n "s/^[[:space:]]*version:[[:space:]]*'\([^']*\)'.*/\1/p" "$staging_dir/source/meson.build" | head -n 1)"
if [[ -z "$version" || -z "$meson_version" || "$version" != "$meson_version" ]]; then
    echo "Version mismatch: spec=${version:-missing}, meson.build=${meson_version:-missing}" >&2
    exit 1
fi
if [[ ! "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+([.a-zA-Z0-9+~_-]*)$ ]]; then
    echo "Unsupported source version: $version" >&2
    exit 1
fi
archive_name="lunalauncher-${version}"
cp "$spec_file" "$topdir/SPECS/lunalauncher.spec"
mv "$staging_dir/source" "$staging_dir/$archive_name"
tar -C "$staging_dir" -czf "$topdir/SOURCES/${archive_name}.tar.gz" "$archive_name"
rpmbuild -bs "$topdir/SPECS/lunalauncher.spec" --define "_topdir $topdir"

echo "Source archive: $topdir/SOURCES/${archive_name}.tar.gz"
echo "SRPM: $(find "$topdir/SRPMS" -maxdepth 1 -name '*.src.rpm' | head -n 1)"
