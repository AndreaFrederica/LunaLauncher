#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
spec_file="$repo_root/packaging/fedora/lunalauncher.spec"
patches_dir="$repo_root/packaging/fedora/patches"
version="$(awk '/^Version:/ { print $2; exit }' "$spec_file")"
archive_name="lunalauncher-${version}"
output_dir="${1:-$repo_root/dist/fedora}"
topdir="$output_dir/rpmbuild"
staging_dir="$(mktemp -d /tmp/lunalauncher-srpm.XXXXXX)"
source_mode="${LUNALAUNCHER_SRPM_SOURCE_MODE:-git}"
trap 'rm -rf "$staging_dir"' EXIT

mkdir -p "$output_dir" "$topdir"/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}

cmake_major="$(sed -n 's/^set(Launcher_VERSION_MAJOR \([0-9][0-9]*\)).*/\1/p' "$repo_root/CMakeLists.txt" | head -n 1)"
cmake_minor="$(sed -n 's/^set(Launcher_VERSION_MINOR \([0-9][0-9]*\)).*/\1/p' "$repo_root/CMakeLists.txt" | head -n 1)"
cmake_patch="$(sed -n 's/^set(Launcher_VERSION_PATCH \([0-9][0-9]*\)).*/\1/p' "$repo_root/CMakeLists.txt" | head -n 1)"
cmake_version=""
if [[ -n "$cmake_major" && -n "$cmake_minor" && -n "$cmake_patch" ]]; then
    cmake_version="${cmake_major}.${cmake_minor}.${cmake_patch}"
fi

if [[ -n "$cmake_version" && "$cmake_version" != "$version" ]]; then
    echo "Version mismatch: spec=$version, CMakeLists.txt=$cmake_version" >&2
    exit 1
fi

mkdir -p "$staging_dir/$archive_name"
if [[ "$source_mode" == "worktree" ]]; then
    git -C "$repo_root" ls-files -z --cached --modified | \
        rsync -a --from0 --files-from=- "$repo_root"/ "$staging_dir/$archive_name"/

    git -C "$repo_root" submodule status --cached | while read -r _ path _rest; do
        if [[ -z "$path" ]]; then
            continue
        fi
        if [[ ! -d "$repo_root/$path/.git" && ! -f "$repo_root/$path/.git" ]]; then
            echo "Skipping unavailable submodule: $path" >&2
            continue
        fi

        mkdir -p "$staging_dir/$archive_name/$path"
        git -C "$repo_root/$path" ls-files -z --cached --modified | \
            rsync -a --from0 --files-from=- "$repo_root/$path"/ "$staging_dir/$archive_name/$path"/
    done
else
    git -C "$repo_root" archive HEAD | tar -x -C "$staging_dir/$archive_name"

    git -C "$repo_root" submodule status --cached | while read -r sha path _; do
        sha="${sha#-}"
        sha="${sha#+}"
        sha="${sha#U}"
        if [[ -z "$sha" || -z "$path" ]]; then
            continue
        fi

        if [[ ! -d "$repo_root/$path/.git" && ! -f "$repo_root/$path/.git" ]]; then
            echo "Skipping unavailable submodule: $path" >&2
            continue
        fi

        if ! git -C "$repo_root/$path" cat-file -e "$sha^{tree}" 2>/dev/null; then
            echo "Skipping submodule without local commit object: $path ($sha)" >&2
            continue
        fi

        mkdir -p "$staging_dir/$archive_name/$path"
        git -C "$repo_root/$path" archive "$sha" | tar -x -C "$staging_dir/$archive_name/$path"
    done

    if [[ -d "$patches_dir" ]]; then
        while IFS= read -r -d '' patch_file; do
            patch -d "$staging_dir/$archive_name" -p1 < "$patch_file"
        done < <(find "$patches_dir" -maxdepth 1 -type f -name '*.patch' -print0 | sort -z)
    fi
fi

tar -C "$staging_dir" -czf "$topdir/SOURCES/${archive_name}.tar.gz" "$archive_name"
cp "$spec_file" "$topdir/SPECS/"

rpmbuild -bs "$topdir/SPECS/lunalauncher.spec" --define "_topdir $topdir"

echo "Source archive: $topdir/SOURCES/${archive_name}.tar.gz"
echo "SRPM: $(find "$topdir/SRPMS" -maxdepth 1 -name '*.src.rpm' | head -n 1)"
