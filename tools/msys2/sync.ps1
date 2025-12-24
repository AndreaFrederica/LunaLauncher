param(
    [switch]$Prune   # 是否删除 toml 中未声明的包
)

$ErrorActionPreference = "Stop"

$repo = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$cfg  = Join-Path $repo "msys2.toml"
$lock = Join-Path $repo "msys2.lock"
$msys = Join-Path $repo ".msys2\msys64"
$bash = Join-Path $msys "usr\bin\bash.exe"

if (-not (Test-Path $bash)) {
    throw "MSYS2 not initialized. Run bootstrap.ps1 first."
}

function Get-TomlPackages {
    $lines = Get-Content $cfg
    $inPkgs = $false
    foreach ($l in $lines) {
        if ($l -match '^\s*\[packages\]\s*$') { $inPkgs = $true; continue }
        if ($inPkgs) {
            if ($l -match '^\s*\[') { break }
            if ($l -match '^\s*#' -or $l -match '^\s*$') { continue }
            if ($l -match '=') {
                ($l.Split('=')[0]).Trim()
            }
        }
    }
}

Write-Host ">> Reading declared packages from msys2.toml"
$declared = Get-TomlPackages

Write-Host ">> Querying installed explicit packages"
$installed = & $bash -lc "pacman -Qq --explicit"

# 1) 安装缺失
$missing = $declared | Where-Object { $_ -notin $installed }
if ($missing.Count -gt 0) {
    Write-Host ">> Installing missing packages:"
    $missing | ForEach-Object { Write-Host "   + $_" }
    & $bash -lc "pacman -S --needed --noconfirm $($missing -join ' ')"
} else {
    Write-Host ">> No missing packages"
}

# 2) 可选：删除多余
if ($Prune) {
    $extra = $installed | Where-Object { $_ -notin $declared }
    if ($extra.Count -gt 0) {
        Write-Host ">> Removing extra packages:"
        $extra | ForEach-Object { Write-Host "   - $_" }
        & $bash -lc "pacman -Rns --noconfirm $($extra -join ' ')"
    } else {
        Write-Host ">> No extra packages"
    }
} else {
    Write-Host ">> Prune disabled (use -Prune to remove extra packages)"
}

Write-Host ">> Generating msys2.lock"
$lockUnix = & $bash -lc "cygpath -u '$lock'"
& $bash -lc "pacman -Q --explicit | sed 's/ /=/' > ""$lockUnix"""
