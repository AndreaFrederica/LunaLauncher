#!/usr/bin/env bash
# tools/msys2/remove.sh - Remove packages from msys2.toml and uninstall them

set -e

if [[ $# -eq 0 ]]; then
    echo "Usage: $0 <package1> [package2] ..."
    echo "Example: $0 mingw-w64-ucrt-x86_64-qt6-tools"
    exit 1
fi

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CFG="$REPO_DIR/msys2.toml"
PACKAGES=("$@")

# 将请求删除的包转换为关联数组
declare -A requested_map
for pkg in "${PACKAGES[@]}"; do
    requested_map["$pkg"]=1
done

# 查找要删除的包
to_remove=()
in_pkgs=false
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
            pkg=$(echo "${line%%=*}" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
            if [[ -n "${requested_map[$pkg]}" ]]; then
                to_remove+=("$pkg")
            fi
        fi
    fi
done < "$CFG"

if [[ ${#to_remove[@]} -eq 0 ]]; then
    echo ">> No matching packages found in msys2.toml"
    exit 0
fi

echo ">> Removing packages from msys2.toml:"
for pkg in "${to_remove[@]}"; do
    echo "   - $pkg"
done

# 备份原文件
cp "$CFG" "$CFG.bak"

# 从 toml 中删除指定的包
awk -v pkgs="$(IFS='|'; echo "${to_remove[*]}")" '
/^\[packages\]/ { in_pkgs=1; print; next }
{
    if (in_pkgs && /^$/) { in_pkgs=0; print; next }
    if (in_pkgs && /^\[/) { in_pkgs=0 }
}
{
    if (!in_pkgs) {
        print
        next
    }
    # 在 [packages] 部分
    if (/=/) {
        pkg = $0
        gsub(/^[[:space:]]*/, "", pkg)
        gsub(/=.*/, "", pkg)
        gsub(/[[:space:]]*$/, "", pkg)
        # 检查是否是要删除的包
        match = 0
        n = split(pkgs, arr, /\|/)
        for (i = 1; i <= n; i++) {
            if (pkg == arr[i]) {
                match = 1
                break
            }
        }
        if (!match) print
    } else {
        print
    }
}
' "$CFG.bak" > "$CFG"

echo ">> Uninstalling packages..."
if pacman -Rns --noconfirm "${to_remove[@]}"; then
    echo ">> Packages removed successfully"
    rm "$CFG.bak"
else
    echo ">> Removal failed, restoring backup"
    cp "$CFG.bak" "$CFG"
    exit 1
fi
