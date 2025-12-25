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

# 用于存储任务信息的关联数组
declare -A task_commands
declare -A task_dependencies
declare -A task_descriptions

# 读取 toml 中的任务定义
parse_toml_tasks() {
    local in_tasks=false
    local current_task=""
    local in_table=false

    while IFS= read -r line; do
        # 跳过注释和空行
        if [[ "$line" =~ ^[[:space:]]*# ]] || [[ -z "${line// }" ]]; then
            continue
        fi

        # 检测 [tasks] 段落
        if [[ "$line" =~ ^\[tasks\] ]]; then
            in_tasks=true
            continue
        fi

        # 检测新的段落（退出 tasks 段落）
        if $in_tasks && [[ "$line" =~ ^\[ ]]; then
            if [[ ! "$line" =~ ^\[tasks\. ]]; then
                break
            fi
        fi

        if $in_tasks; then
            # 检测任务子表 [tasks.task_name]
            if [[ "$line" =~ ^\[tasks\.([A-Za-z_][A-Za-z0-9_]*)\] ]]; then
                current_task="${BASH_REMATCH[1]}"
                in_table=true
                continue
            fi

            # 简单的任务定义：task_name = "command"
            if [[ "$line" =~ ^[[:space:]]*([A-Za-z_][A-Za-z0-9_]*)[[:space:]]*=[[:space:]]*\"(.*)\"[[:space:]]*$ ]]; then
                local task_name="${BASH_REMATCH[1]}"
                local command="${BASH_REMATCH[2]}"
                task_commands["$task_name"]="$command"
                current_task="$task_name"
                in_table=false
                continue
            fi

            # 在子表内的属性
            if $in_table && [[ -n "$current_task" ]]; then
                # command = "..."
                if [[ "$line" =~ ^[[:space:]]*command[[:space:]]*=[[:space:]]*\"(.*)\"[[:space:]]*$ ]]; then
                    task_commands["$current_task"]="${BASH_REMATCH[1]}"
                # commands = ["cmd1", "cmd2"]
                elif [[ "$line" =~ ^[[:space:]]*commands[[:space:]]*=[[:space:]]*\[(.*)\][[:space:]]*$ ]]; then
                    local cmds="${BASH_REMATCH[1]}"
                    # 移除引号和逗号，保留命令
                    cmds=$(echo "$cmds" | sed 's/","/\n/g' | sed 's/"//g' | tr '\n' ';' | sed 's/;$//')
                    task_commands["$current_task"]="$cmds"
                # depends_on = ["dep1", "dep2"]
                elif [[ "$line" =~ ^[[:space:]]*depends_on[[:space:]]*=[[:space:]]*\[(.*)\][[:space:]]*$ ]]; then
                    local deps="${BASH_REMATCH[1]}"
                    # 移除引号和逗号
                    deps=$(echo "$deps" | sed 's/","/ /g' | sed 's/"//g')
                    task_dependencies["$current_task"]="$deps"
                # description = "..."
                elif [[ "$line" =~ ^[[:space:]]*description[[:space:]]*=[[:space:]]*\"(.*)\"[[:space:]]*$ ]]; then
                    task_descriptions["$current_task"]="${BASH_REMATCH[1]}"
                fi
            fi
        fi
    done < "$CFG"
}

# 拓扑排序解析依赖
resolve_dependencies() {
    local task_name=$1
    local visited=""
    local visiting=""
    local result=""

    visit() {
        local name=$1
        # 检查循环依赖
        if [[ " $visiting " =~ " $name " ]]; then
            echo "Error: Circular dependency detected involving task '$name'" >&2
            return 1
        fi
        # 已访问
        if [[ " $visited " =~ " $name " ]]; then
            return 0
        fi

        visiting="$visiting $name"

        # 先处理依赖
        local deps="${task_dependencies[$name]}"
        for dep in $deps; do
            if [[ -n "$dep" ]]; then
                visit "$dep" || return 1
            fi
        done

        visiting="${visiting//$name /}"
        visited="$visited $name"
        result="$result $name"
        return 0
    }

    if visit "$task_name"; then
        echo "$result"
        return 0
    else
        return 1
    fi
}

# 运行任务
run_task() {
    local task_name=$1
    shift
    local extra_args="$*"

    # 解析依赖顺序
    local order=$(resolve_dependencies "$task_name")
    if [[ $? -ne 0 ]]; then
        return 1
    fi

    # 按顺序执行
    for t in $order; do
        if [[ -z "${task_commands[$t]}" ]]; then
            echo "Error: Task '$t' not found or has no command" >&2
            return 1
        fi

        local cmd="${task_commands[$t]}"
        if [[ "$t" == "$task_name" && -n "$extra_args" ]]; then
            cmd="$cmd $extra_args"
        fi

        echo ">> Running task: $t"
        bash -lc "$cmd"
        local exit_code=$?
        if [[ $exit_code -ne 0 ]]; then
            echo "Error: Task '$t' failed with exit code $exit_code" >&2
            return $exit_code
        fi
    done

    return 0
}

# 解析 TOML
parse_toml_tasks

# 列出任务
if $show_list; then
    echo ">> Available tasks in msys2.toml:"
    for task in "${!task_commands[@]}"; do
        local cmd="${task_commands[$task]}"
        local desc="${task_descriptions[$task]}"
        local deps="${task_dependencies[$task]}"
        echo "   $task"
        if [[ -n "$desc" ]]; then
            echo "      Description: $desc"
        fi
        echo "      Command: $cmd"
        if [[ -n "$deps" ]]; then
            echo "      Depends on: $deps"
        fi
    done
    exit 0
fi

if [[ $# -eq 0 ]]; then
    # 交互式 shell
    exec bash
else
    # 检查是否是任务名称
    task_name="$1"
    shift

    if [[ -n "${task_commands[$task_name]}" ]]; then
        # 运行定义的任务（包括依赖）
        run_task "$task_name" "$*"
    else
        # 直接运行命令
        bash -lc "$task_name $*"
    fi
fi
