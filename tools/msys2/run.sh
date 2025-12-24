#!/usr/bin/env bash
# tools/msys2/run.sh - Run commands or tasks defined in msys2.toml

show_list=false

# 解析参数
while [[ $# -gt 0 ]]; do
    case $1 in
        --list|-l)
            show_list=true
            shift
            ;;
        *)
            break
            ;;
    esac
done

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CFG="$REPO_DIR/msys2.toml"

# 读取 toml 中的任务定义
get_tasks() {
    local in_tasks=false
    while IFS= read -r line; do
        if [[ "$line" =~ ^\[tasks\] ]]; then
            in_tasks=true
            continue
        fi
        if $in_tasks; then
            if [[ "$line" =~ ^\[ ]]; then
                break
            fi
            # 跳过注释和空行
            if [[ "$line" =~ ^[[:space:]]*# ]] || [[ -z "${line// }" ]]; then
                continue
            fi
            # 提取任务名和命令
            if [[ "$line" =~ ^[[:space:]]*([A-Za-z_][A-Za-z0-9_]*)[[:space:]]*=[[:space:]]*\"(.*)\"[[:space:]]*$ ]]; then
                echo "${BASH_REMATCH[1]}|${BASH_REMATCH[2]}"
            fi
        fi
    done < "$CFG"
}

# 列出任务
if $show_list; then
    echo ">> Available tasks in msys2.toml:"
    while IFS='|' read -r name cmd; do
        echo "   $name = $cmd"
    done < <(get_tasks)
    exit 0
fi

if [[ $# -eq 0 ]]; then
    # 交互式 shell（不需要特殊处理，当前就在 bash 中）
    exec bash
else
    # 检查是否是任务名称
    task_name="$1"
    shift

    declare -A tasks
    while IFS='|' read -r name cmd; do
        tasks["$name"]="$cmd"
    done < <(get_tasks)

    if [[ -n "${tasks[$task_name]}" ]]; then
        # 运行定义的任务
        cmd="${tasks[$task_name]}"
        if [[ $# -gt 0 ]]; then
            cmd="$cmd $*"
        fi
        bash -lc "$cmd"
    else
        # 直接运行命令
        bash -lc "$task_name $*"
    fi
fi
