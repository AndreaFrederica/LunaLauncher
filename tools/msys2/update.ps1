# tools/msys2/update.ps1
$ErrorActionPreference = "Stop"

$repo = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$envRoot = Join-Path $repo ".msys2"
$msys    = Join-Path $envRoot "msys64"
$bash    = Join-Path $msys "usr\bin\bash.exe"
$lock    = Join-Path $repo "msys2.lock"

if (-not (Test-Path $bash)) {
    throw "MSYS2 not initialized"
}

Write-Host ">> Updating MSYS2 packages"
& $bash -lc "pacman -Syu --noconfirm"

Write-Host ">> Re-generating lockfile"
$lockUnix = & $bash -lc "cygpath -u '$lock'"
& $bash -lc "pacman -Q --explicit | sed 's/ /=/' > ""$lockUnix"""

Write-Host ">> Update complete. Review msys2.lock and commit."
