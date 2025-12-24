# tools/msys2/shell.ps1
# 启动 MSYS2 交互式 shell

$ErrorActionPreference = "Stop"

$repo = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$bash = Join-Path $repo ".msys2\msys64\usr\bin\bash.exe"

if (-not (Test-Path $bash)) {
    throw "MSYS2 environment not found. Run bootstrap.ps1 first."
}

# 设置 MSYS2 环境变量
$env:MSYSTEM = "UCRT64"
$env:CHERE_INVOKING = "1"

# 切换到仓库根目录
Set-Location $repo

# 启动交互式 bash
& $bash
