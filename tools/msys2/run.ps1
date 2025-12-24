# tools/msys2/run.ps1
param(
    [string]$msystem = "ucrt64",

    [Parameter(Position = 0)]
    [string[]]$cmd,

    [switch]$list   # 列出所有可用脚本
)

$repo = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$bash = Join-Path $repo ".msys2\msys64\usr\bin\bash.exe"
$cfg  = Join-Path $repo "msys2.toml"

if (-not (Test-Path $bash)) {
    throw "MSYS2 environment not found. Run bootstrap.ps1 first."
}

# 读取 toml 中的脚本定义
function Get-Tasks {
    if (-not (Test-Path $cfg)) {
        return @{}
    }

    $tasks = @{}
    $lines = Get-Content $cfg
    $inTasks = $false
    $currentTask = $null

    foreach ($l in $lines) {
        if ($l -match '^\s*\[tasks\]\s*$') {
            $inTasks = $true
            continue
        }
        if ($inTasks) {
            if ($l -match '^\s*\[') {
                break
            }
            # 匹配 task_name = "command" 或 task_name = ["cmd1", "cmd2"]
            if ($l -match '^\s*(\w+)\s*=\s*"(.+)"\s*$') {
                $tasks[$matches[1]] = $matches[2]
            } elseif ($l -match '^\s*(\w+)\s*=\s*\[(.+)\]\s*$') {
                # 简单的数组解析（仅处理单行）
                $arrayContent = $matches[2]
                # 提取所有引号内的内容
                $cmds = ([regex]::Matches($arrayContent, '"([^"]*)"')).Forwards | ForEach-Object { $_.Groups[1].Value }
                $tasks[$matches[1]] = $cmds -join " "
            }
        }
    }
    return $tasks
}

$env:MSYSTEM = $msystem.ToUpper()
$env:CHERE_INVOKING = "1"

# 列出脚本
if ($list) {
    $tasks = Get-Tasks
    if ($tasks.Count -eq 0) {
        Write-Host ">> No tasks defined in msys2.toml"
    } else {
        Write-Host ">> Available tasks in msys2.toml:"
        foreach ($task in $tasks.GetEnumerator()) {
            Write-Host "   $($task.Key) = $($task.Value)"
        }
    }
    exit 0
}

if (-not $cmd -or $cmd.Count -eq 0) {
    # 交互 shell
    & $bash
} else {
    # 检查是否是脚本名称
    $tasks = Get-Tasks
    $taskName = $cmd[0]
    if ($tasks.ContainsKey($taskName)) {
        # 运行定义的脚本
        $taskCmd = $tasks[$taskName]
        # 如果有额外参数，追加到脚本后
        if ($cmd.Count -gt 1) {
            $taskCmd = "$taskCmd $($cmd[1..($cmd.Count-1)] -join ' ')"
        }
        & $bash -lc $taskCmd
    } else {
        # 直接运行命令
        & $bash -lc ($cmd -join " ")
    }
}
