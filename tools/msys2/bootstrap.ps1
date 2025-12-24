# tools/msys2/bootstrap.ps1
$ErrorActionPreference = "Stop"

# repo 根目录
$repo = Resolve-Path (Join-Path $PSScriptRoot "..\..")

$cfg  = Join-Path $repo "msys2.toml"
$lock = Join-Path $repo "msys2.lock"

$envRoot = Join-Path $repo ".msys2"
$msys    = Join-Path $envRoot "msys64"
$tmp     = Join-Path $envRoot "_tmp"
$bash    = Join-Path $msys "usr\bin\bash.exe"

if (-not (Test-Path $cfg)) {
    throw "msys2.toml not found at repo root"
}

function Get-TomlValue($key) {
    (Select-String -Path $cfg -Pattern "^\s*$key\s*=").Line.Split('"')[1]
}

function Get-PackageNames {
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

# 1) 下载并解压 MSYS2 base
if (-not (Test-Path $bash)) {
    Write-Host ">> Bootstrapping MSYS2 into .msys2/"

    $baseUrl = Get-TomlValue "base_url"
    if (-not $baseUrl) { throw "base_url missing in msys2.toml" }

    # 构建下载 URL
    # 支持两种格式:
    # 1. 完整文件 URL (旧格式): "https://repo.msys2.org/distrib/x86_64/msys2-base-x86_64-20251213.tar.xz"
    # 2. 目录 URL (新格式): "https://repo.msys2.org/distrib/x86_64/"
    if ($baseUrl -match '\.tar\.(zst|xz)$') {
        $downloadUrl = $baseUrl
    } else {
        $version = Get-TomlValue "version"
        if (-not $version) { throw "version missing in msys2.toml" }
        # 将 YYYY-MM-DD 转换为 YYYYMMDD
        $versionForUrl = $version -replace '-', ''
        # 使用 .tar.xz (PowerShell 的 tar 命令支持 xz 解压)
        $downloadUrl = "$baseUrl/msys2-base-x86_64-$versionForUrl.tar.xz"
    }

    New-Item -ItemType Directory -Force -Path $tmp | Out-Null
    $archiveName = Split-Path $downloadUrl -Leaf
    $archive = Join-Path $tmp $archiveName

    Write-Host ">> Downloading MSYS2 base archive from $downloadUrl"

    # 强制启用 TLS 1.2+（防止旧 PowerShell）
    [Net.ServicePointManager]::SecurityProtocol =
        [Net.ServicePointManager]::SecurityProtocol -bor
        [Net.SecurityProtocolType]::Tls12

    # 如果之前有残留，先删掉
    if (Test-Path $archive) {
        Remove-Item $archive -Force
    }

    $downloaded = $false

    # 首选：BITS（最稳定，支持断点续传）
    try {
        Start-BitsTransfer -Source $downloadUrl -Destination $archive -ErrorAction Stop
        $downloaded = $true
    }
    catch {
        Write-Warning "BITS download failed, falling back to Invoke-WebRequest"
    }

    # 兜底：Invoke-WebRequest（理论上这里不会再走到）
    if (-not $downloaded) {
        Invoke-WebRequest -Uri $downloadUrl -OutFile $archive -UseBasicParsing
    }

    tar -xf $archive -C $envRoot

    # 验证 MSYS2 root
    if (-not (Test-Path $msys)) {
        throw "MSYS2 base archive did not produce msys64/ as expected."
    }
}

# 2) pacman sync/update
& $bash -lc "pacman -Sy --noconfirm"

$autoUpdate = (Select-String -Path $cfg -Pattern "auto_update").Line
if ($autoUpdate -and $autoUpdate -match 'true') {
    & $bash -lc "pacman -Su --noconfirm"
}

# 3) install deps
if (Test-Path $lock) {
    Write-Host ">> Installing from msys2.lock"
    $pkgs = (Get-Content $lock) -join " "
    & $bash -lc "pacman -S --needed --noconfirm $pkgs"
} else {
    Write-Host ">> Installing from msys2.toml"
    $pkgs = (Get-PackageNames) -join " "
    & $bash -lc "pacman -S --needed --noconfirm $pkgs"

    Write-Host ">> Generating msys2.lock"
    $lockUnix = & $bash -lc "cygpath -u '$lock'"
    & $bash -lc "pacman -Q --explicit | sed 's/ /=/' > ""$lockUnix"""
}

Write-Host ">> MSYS2 ready at .msys2/"
