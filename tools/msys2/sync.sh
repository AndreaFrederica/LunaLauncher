#!/usr/bin/env bash
# tools/msys2/sync.sh - Sync packages from msys2.toml

set -e

# 默认参数
PRUNE=false

# 解析参数
while [[ $# -gt 0 ]]; do
    case $1 in
        --prune)
            PRUNE=true
            shift
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [--prune]"
            exit 1
            ;;
    esac
done

# 获取仓库根目录
REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CFG="$REPO_DIR/msys2.toml"
LOCK="$REPO_DIR/msys2.lock"

# 获取 toml 中声明的包
get_toml_packages() {
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
            # 跳过注释和空行
            if [[ "$line" =~ ^[[:space:]]*# ]] || [[ -z "${line// }" ]]; then
                continue
            fi
            # 提取包名（等号左边）
            if [[ "$line" =~ = ]]; then
                echo "${line%%=*}" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//'
            fi
        fi
    done < "$CFG"
}

echo ">> Reading declared packages from msys2.toml"
mapfile -t declared < <(get_toml_packages)

echo ">> Querying installed explicit packages"
mapfile -t installed < <(pacman -Qq --explicit)

# 将已安装包转换为关联数组以便快速查找
declare -A installed_map
for pkg in "${installed[@]}"; do
    installed_map["$pkg"]=1
done

# 1) 安装缺失的包
missing=()
for pkg in "${declared[@]}"; do
    if [[ -z "${installed_map[$pkg]}" ]]; then
        missing+=("$pkg")
    fi
done

if [[ ${#missing[@]} -gt 0 ]]; then
    echo ">> Installing missing packages:"
    for pkg in "${missing[@]}"; do
        echo "   + $pkg"
    done
    pacman -S --needed --noconfirm "${missing[@]}"
else
    echo ">> No missing packages"
fi

# 2) 可选：删除多余的包
if $PRUNE; then
    # 将声明包转换为关联数组
    declare -A declared_map
    for pkg in "${declared[@]}"; do
        declared_map["$pkg"]=1
    done

    extra=()
    for pkg in "${installed[@]}"; do
        if [[ -z "${declared_map[$pkg]}" ]]; then
            extra+=("$pkg")
        fi
    done

    if [[ ${#extra[@]} -gt 0 ]]; then
        echo ">> Removing extra packages:"
        for pkg in "${extra[@]}"; do
            echo "   - $pkg"
        done
        pacman -Rns --noconfirm "${extra[@]}"
    else
        echo ">> No extra packages"
    fi
else
    echo ">> Prune disabled (use --prune to remove extra packages)"
fi

echo ">> Generating msys2.lock"
pacman -Q --explicit | sed 's/ /=/' > "$LOCK"
