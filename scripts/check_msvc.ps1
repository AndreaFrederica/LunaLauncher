$ErrorActionPreference = "Stop"

function Find-VisualStudio {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        throw "vswhere.exe not found"
    }

    $json = & $vswhere `
        -latest `
        -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -format json

    $data = $json | ConvertFrom-Json
    if (-not $data) {
        throw "No Visual Studio with MSVC found"
    }

    return $data[0].installationPath
}

function Import-MsvcEnv {
    param(
        [string]$VsInstallPath,
        [ValidateSet("x86", "x64", "arm64")]
        [string]$Arch = "x64",
        [string]$ExtraArgs = ""
    )

    $vsDevCmd = Join-Path $VsInstallPath "Common7\Tools\VsDevCmd.bat"
    if (-not (Test-Path $vsDevCmd)) {
        throw "VsDevCmd.bat not found"
    }

    $cmd = "`"$vsDevCmd`" -arch=$Arch $ExtraArgs && set"

    cmd /c $cmd |
        ForEach-Object {
            if ($_ -match "^(.*?)=(.*)$") {
                Set-Item "Env:$($matches[1])" $matches[2]
            }
        }
}

# -------------------- main --------------------

Write-Host "[MSVC] Detecting Visual Studio..."
$vsPath = Find-VisualStudio
Write-Host "[MSVC] VS found at: $vsPath"

Write-Host "[MSVC] Importing MSVC environment..."
Import-MsvcEnv -VsInstallPath $vsPath -Arch x64

Write-Host "[MSVC] Environment injected successfully"
