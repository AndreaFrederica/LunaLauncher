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

# 读取 toml 内容
$lines = Get-Content $cfg

# 查找要删除的包
$toRemove = @()
$newLines = @()
$inPkgs = $false

foreach ($l in $lines) {
    if ($l -match '^\s*\[packages\]\s*$') {
        $inPkgs = $true
        $newLines += $l
        continue
    }
    if ($inPkgs) {
        if ($l -match '^\s*\[') {
            $inPkgs = $false
            $newLines += $l
            continue
        }
        # 检查是否是请求删除的包
        $match = $false
        foreach ($pkg in $Packages) {
            if ($l -match "^\s*([regex]::Escape($pkg))\s*=") {
                $toRemove += $pkg
                $match = $true
                break
            }
        }
        if (-not $match) {
            $newLines += $l
        }
    } else {
        $newLines += $l
    }
}

if ($toRemove.Count -eq 0) {
    Write-Host ">> No matching packages found in msys2.toml"
    exit 0
}

Write-Host ">> Removing packages from msys2.toml:"
$toRemove | ForEach-Object { Write-Host "   - $_" }

# 备份原文件
$backup = $cfg + ".bak"
Copy-Item $cfg $backup -Force

# 写入新内容
$newLines | Set-Content $cfg -Encoding UTF8

Write-Host ">> Uninstalling packages..."
& $bash -lc "pacman -Rns --noconfirm $($toRemove -join ' ')"

if ($LASTEXITCODE -eq 0) {
    Write-Host ">> Packages removed successfully"
    Write-Host ">> Backup saved to: $backup"
    Remove-Item $backup -Force
} else {
    Write-Host ">> Removal failed, restoring backup"
    Copy-Item $backup $cfg -Force
    exit 1
}
