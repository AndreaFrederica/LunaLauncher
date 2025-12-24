param(
    [Parameter(Position = 0, Mandatory = $true)]
    [string[]]$Packages
)

$ErrorActionPreference = "Stop"

$repo = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$cfg = Join-Path $repo "msys2.toml"
$bash = Join-Path $repo ".msys2\msys64\usr\bin\bash.exe"

if (-not (Test-Path $bash)) {
    throw "MSYS2 not initialized. Run bootstrap.ps1 first."
}

# 先尝试安装包
Write-Host ">> Installing packages with pacman..."
$pkgList = $Packages -join ' '
& $bash -lc "pacman -S --needed --noconfirm $pkgList"

if ($LASTEXITCODE -ne 0) {
    Write-Host ">> Failed to install packages. Not updating msys2.toml"
    exit 1
}

# 安装成功，更新 toml
# 读取 toml 内容
$lines = Get-Content $cfg

# 找到 [packages] 部分的结束位置
$inPkgs = $false
$insertIdx = -1
for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($lines[$i] -match '^\s*\[packages\]\s*$') {
        $inPkgs = $true
        continue
    }
    if ($inPkgs) {
        if ($lines[$i] -match '^\s*\[') {
            $insertIdx = $i
            break
        }
    }
}

# 如果 [packages] 是最后一个部分
if ($insertIdx -eq -1) {
    $insertIdx = $lines.Count
}

# 检查包是否已存在
$existing = @()
$inPkgs = $false
foreach ($l in $lines) {
    if ($l -match '^\s*\[packages\]\s*$') { $inPkgs = $true; continue }
    if ($inPkgs) {
        if ($l -match '^\s*\[') { break }
        if ($l -match '^\s*(\S+)\s*=') {
            $existing += $matches[1]
        }
    }
}

# 过滤已存在的包
$toAdd = $Packages | Where-Object { $_ -notin $existing }

if ($toAdd.Count -eq 0) {
    Write-Host ">> Packages already exist in msys2.toml (skipping update)"
    exit 0
}

# 在 [packages] 部分末尾添加新包
$newLines = @()
$inPkgs = $false
$added = $false

for ($i = 0; $i -lt $lines.Count; $i++) {
    $newLines += $lines[$i]
    if ($inPkgs -and -not $added -and ($lines[$i] -match '^\s*\[' -or $i -eq $lines.Count - 1)) {
        foreach ($pkg in $toAdd) {
            $newLines += "$pkg = `"*`""
        }
        $added = $true
    }
    if ($lines[$i] -match '^\s*\[packages\]\s*$') {
        $inPkgs = $true
    }
}

# 如果 [packages] 是最后一节
if (-not $added) {
    foreach ($pkg in $toAdd) {
        $newLines += "$pkg = `"*`""
    }
}

Write-Host ">> Adding packages to msys2.toml:"
$toAdd | ForEach-Object { Write-Host "   + $_" }

# 写入新内容
$newLines | Set-Content $cfg -Encoding UTF8

Write-Host ">> Done"
