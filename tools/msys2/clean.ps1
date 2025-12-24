param(
    [switch]$Force
)

$ErrorActionPreference = "Stop"

$repo = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$envRoot = Join-Path $repo ".msys2"

if (-not (Test-Path $envRoot)) {
    Write-Host ">> .msys2 does not exist, nothing to clean"
    return
}

if (-not $Force) {
    Write-Host ""
    Write-Host "This will REMOVE the project MSYS2 environment:"
    Write-Host "  $envRoot"
    Write-Host ""
    $ans = Read-Host "Type 'yes' to confirm"
    if ($ans -ne "yes") {
        Write-Host ">> Aborted"
        return
    }
}

Write-Host ">> Removing .msys2/"
Remove-Item -Recurse -Force $envRoot

Write-Host ">> Clean complete"
