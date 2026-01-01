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

# 任务存储
$script:tasks = @{}
$script:taskDependencies = @{}
$script:taskDescriptions = @{}

# 读取 toml 中的脚本定义
function Parse-TomlTasks {
    if (-not (Test-Path $cfg)) {
        return
    }

    $lines = Get-Content $cfg
    $inTasks = $false
    $currentTask = $null
    $inTable = $false

    foreach ($l in $lines) {
        # 跳过注释和空行
        if ($l -match '^\s*#' -or $l -match '^\s*$') {
            continue
        }

        # 检测 [tasks] 段落
        if ($l -match '^\s*\[tasks\]\s*$') {
            $inTasks = $true
            continue
        }

        # 检测新段落（退出 tasks）
        if ($inTasks -and $l -match '^\s*\[') {
            if ($l -notmatch '^\s*\[tasks\.') {
                break
            }
        }

        if ($inTasks) {
            # 检测任务子表 [tasks.task_name]
            if ($l -match '^\s*\[tasks\.([\w]+)\]\s*$') {
                $currentTask = $matches[1]
                $inTable = $true
                continue
            }

            # 简单定义：task_name = "command"
            if ($l -match '^\s*(\w+)\s*=\s*"(.+)"\s*$') {
                $taskName = $matches[1]
                $command = $matches[2]
                $script:tasks[$taskName] = $command
                $currentTask = $taskName
                $inTable = $false
                continue
            }

            # 子表内的属性
            if ($inTable -and $currentTask) {
                # command = "..."
                if ($l -match '^\s*command\s*=\s*"(.+)"\s*$') {
                    $script:tasks[$currentTask] = $matches[1]
                }
                # commands = ["cmd1", "cmd2"]
                elseif ($l -match '^\s*commands\s*=\s*\[(.+)\]\s*$') {
                    $arrayContent = $matches[1]
                    $cmds = [regex]::Matches($arrayContent, '"([^"]*)"') | ForEach-Object { $_.Groups[1].Value }
                    $script:tasks[$currentTask] = $cmds -join ';'
                }
                # depends_on = ["dep1", "dep2"]
                elseif ($l -match '^\s*depends_on\s*=\s*\[(.+)\]\s*$') {
                    $arrayContent = $matches[1]
                    $deps = [regex]::Matches($arrayContent, '"([^"]*)"') | ForEach-Object { $_.Groups[1].Value }
                    $script:taskDependencies[$currentTask] = $deps
                }
                # description = "..."
                elseif ($l -match '^\s*description\s*=\s*"(.+)"\s*$') {
                    $script:taskDescriptions[$currentTask] = $matches[1]
                }
            }
        }
    }
}

# 拓扑排序解析依赖
function Resolve-Dependencies {
    param([string]$taskName)

    $visited = @{}
    $visiting = @{}
    $result = @()

    function Visit {
        param([string]$name)

        # 循环依赖检测
        if ($visiting.ContainsKey($name)) {
            Write-Error "Circular dependency detected involving task '$name'"
            return $null
        }

        # 已访问
        if ($visited.ContainsKey($name)) {
            return $true
        }

        $visiting[$name] = $true

        # 先处理依赖
        if ($script:taskDependencies.ContainsKey($name)) {
            foreach ($dep in $script:taskDependencies[$name]) {
                if (-not (Visit $dep)) {
                    return $null
                }
            }
        }

        $visiting.Remove($name)
        $visited[$name] = $true
        $result += $name
        return $true
    }

    if (Visit $taskName) {
        return $result
    } else {
        return $null
    }
}

# 运行任务
function Invoke-Task {
    param([string]$taskName, [string[]]$extraArgs)

    $order = Resolve-Dependencies $taskName
    if (-not $order) {
        return 1
    }

    foreach ($t in $order) {
        if (-not $script:tasks.ContainsKey($t)) {
            Write-Error "Task '$t' not found or has no command"
            return 1
        }

        $taskCmd = $script:tasks[$t]

        # 如果是目标任务，添加额外参数
        if ($t -eq $taskName -and $extraArgs) {
            $taskCmd = "$taskCmd $($extraArgs -join ' ')"
        }

        Write-Host ">> Running task: $t"
        & $bash -lc $taskCmd
        $exitCode = $LASTEXITCODE
        if ($exitCode -ne 0) {
            Write-Error "Task '$t' failed with exit code $exitCode"
            return $exitCode
        }
    }

    return 0
}

# 解析 TOML
Parse-TomlTasks

$env:MSYSTEM = $msystem.ToUpper()
$env:CHERE_INVOKING = "1"

# 列出脚本
if ($list) {
    if ($script:tasks.Count -eq 0) {
        Write-Host ">> No tasks defined in msys2.toml"
    } else {
        Write-Host ">> Available tasks in msys2.toml:"
        foreach ($task in $script:tasks.GetEnumerator()) {
            Write-Host "   $($task.Key)"
            if ($script:taskDescriptions.ContainsKey($task.Key)) {
                Write-Host "      Description: $($script:taskDescriptions[$task.Key])"
            }
            Write-Host "      Command: $($task.Value)"
            if ($script:taskDependencies.ContainsKey($task.Key)) {
                Write-Host "      Depends on: $($script:taskDependencies[$task.Key] -join ', ')"
            }
        }
    }
    exit 0
}

if (-not $cmd -or $cmd.Count -eq 0) {
    # 交互 shell
    & $bash
} else {
    # 检查是否是脚本名称
    $taskName = $cmd[0]
    $extraArgs = if ($cmd.Count -gt 1) { $cmd[1..($cmd.Count-1)] } else { @() }

    if ($script:tasks.ContainsKey($taskName)) {
        # 运行定义的任务（包括依赖）
        $exitCode = Invoke-Task $taskName $extraArgs
        exit $exitCode
    } else {
        # 直接运行命令
        & $bash -lc ($cmd -join " ")
    }
}
