#!/usr/bin/env bash
# tools/msys2/add.sh - Add packages to msys2.toml and install them

set -e

if [[ $# -eq 0 ]]; then
    echo "Usage: $0 <package1> [package2] ..."
    echo "Example: $0 mingw-w64-ucrt-x86_64-qt6-tools"
    exit 1
fi

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CFG="$REPO_DIR/msys2.toml"
PACKAGES=("$@")

# 先尝试安装包
echo ">> Installing packages with pacman..."
if ! pacman -S --needed --noconfirm "${PACKAGES[@]}"; then
    echo ">> Failed to install packages. Not updating msys2.toml"
    exit 1
fi

# 安装成功，更新 toml
# 检查包是否已存在
get_existing_packages() {
    local in_pkgs=false
    while IFS= read -r line; do
        if [[ "$line" =~ ^\[packages\] ]]; then
            in_pkgs=true
            continue
        fi
        if $in_pkgs; then
            if [[ "$line" =~ ^\[ ]]; then
                break
            fi
            if [[ "$line" =~ = ]]; then
                echo "${line%%=*}" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//'
            fi
        fi
    done < "$CFG"
}

mapfile -t existing < <(get_existing_packages)

# 将已存在包转换为关联数组
declare -A existing_map
for pkg in "${existing[@]}"; do
    existing_map["$pkg"]=1
done

# 过滤已存在的包
to_add=()
for pkg in "${PACKAGES[@]}"; do
    if [[ -z "${existing_map[$pkg]}" ]]; then
        to_add+=("$pkg")
    fi
done

if [[ ${#to_add[@]} -eq 0 ]]; then
    echo ">> Packages already exist in msys2.toml (skipping update)"
    exit 0
fi

# 在 [packages] 部分末尾添加新包
echo ">> Adding packages to msys2.toml:"
for pkg in "${to_add[@]}"; do
    echo "   + $pkg"
done

# 备份原文件
cp "$CFG" "$CFG.bak"

# 找到 [packages] 部分结束位置，在结束前插入新包
awk -v packages="$(IFS=$'\n'; echo "${to_add[*]}")" '
/^\[packages\]/ { in_pkgs=1; next }
in_pkgs && /^$/ { in_pkgs=0; print ""; print_packages(); next }
in_pkgs && /^\[/ { in_pkgs=0; print_packages(); print $0; next }
{ print }
END { if (in_pkgs) print_packages() }

function print_packages() {
    n = split(packages, pkgs, /\n/)
    for (i = 1; i <= n; i++) {
        print pkgs[i] " = \"*\""
    }
    printed = 1
}
' "$CFG.bak" > "$CFG"

echo ">> Done"
rm "$CFG.bak"
