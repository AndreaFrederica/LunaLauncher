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

function Format-Bytes {
    param(
        [Parameter(Mandatory = $true)]
        [double]$Bytes
    )

    if ($Bytes -ge 1GB) { return ("{0:N2} GiB" -f ($Bytes / 1GB)) }
    if ($Bytes -ge 1MB) { return ("{0:N2} MiB" -f ($Bytes / 1MB)) }
    if ($Bytes -ge 1KB) { return ("{0:N2} KiB" -f ($Bytes / 1KB)) }
    return ("{0:N0} B" -f $Bytes)
}

function Format-Rate {
    param(
        [Parameter(Mandatory = $true)]
        [double]$BytesPerSecond
    )

    if ($BytesPerSecond -le 0) { return "0 B/s" }
    return ("{0}/s" -f (Format-Bytes -Bytes $BytesPerSecond))
}

function Try-GetContentLength {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Url,

        [int]$TimeoutMs = 5000
    )

    try {
        $req = [System.Net.HttpWebRequest]::Create($Url)
        $req.Method = "HEAD"
        $req.Timeout = $TimeoutMs
        $req.ReadWriteTimeout = $TimeoutMs
        $req.AllowAutoRedirect = $true
        $resp = $req.GetResponse()
        $len = [double]$resp.ContentLength
        $resp.Close()
        if ($len -gt 0) { return $len }
    }
    catch {}

    try {
        $req = [System.Net.HttpWebRequest]::Create($Url)
        $req.Method = "GET"
        $req.Timeout = $TimeoutMs
        $req.ReadWriteTimeout = $TimeoutMs
        $req.AllowAutoRedirect = $true
        $req.AddRange(0, 0)
        $resp = $req.GetResponse()
        $cr = $resp.Headers["Content-Range"]
        $len = [double]$resp.ContentLength
        $resp.Close()

        if ($cr -and $cr -match '/(\d+)$') { return [double]$Matches[1] }
        if ($len -gt 0) { return $len }
    }
    catch {}

    return 0.0
}

function Download-FileWithProgress {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Url,

        [Parameter(Mandatory = $true)]
        [string]$DestinationPath,

        [int]$TimeoutMs = 0
    )

    Write-Host ">> Starting download: $Url"

    $logStepPercent = 10
    if ($env:GITHUB_ACTIONS -eq "true") {
        $logStepPercent = 1
    }

    if (Test-Path $DestinationPath) {
        Remove-Item $DestinationPath -Force
    }

    $startTime = [DateTimeOffset]::UtcNow
    $lastLogThreshold = 0
    $lastLogTime = $startTime
    $lastLogBytes = 0.0

    $bitsCmd = Get-Command Start-BitsTransfer -ErrorAction SilentlyContinue
    $useBits = ($bitsCmd -and ($env:GITHUB_ACTIONS -ne "true"))
    if ($useBits) {
        $job = $null
        $knownTotal = Try-GetContentLength -Url $Url -TimeoutMs 5000
        try {
            $job = Start-BitsTransfer -Source $Url -Destination $DestinationPath -Asynchronous -ErrorAction Stop

            while ($true) {
                $job = Get-BitsTransfer -JobId $job.JobId -ErrorAction Stop

                if ($job.JobState -eq "Transferred") {
                    Complete-BitsTransfer -BitsJob $job
                    break
                }
                if ($job.JobState -eq "Error" -or $job.JobState -eq "Cancelled") {
                    throw ("BITS download failed: {0}" -f $job.ErrorDescription)
                }
                if ($job.JobState -eq "TransientError") {
                    Resume-BitsTransfer -BitsJob $job -ErrorAction SilentlyContinue | Out-Null
                }

                $total = 0.0
                if ($knownTotal -gt 0) {
                    $total = $knownTotal
                } else {
                    $rawTotal = $job.BytesTotal
                    if ($rawTotal -ne [UInt64]::MaxValue) {
                        $total = [double]$rawTotal
                    }
                }
                $done = [double]$job.BytesTransferred

                $now = [DateTimeOffset]::UtcNow
                $dt = ($now - $lastLogTime).TotalSeconds
                if ($dt -le 0) { $dt = 0.001 }

                if ($total -gt 0) {
                    $pct = [int][Math]::Floor(($done / $total) * 100)
                    $threshold = [int]([Math]::Floor($pct / $logStepPercent) * $logStepPercent)
                    if ($threshold -ge $logStepPercent -and $threshold -ge ($lastLogThreshold + $logStepPercent) -and $threshold -le 100) {
                        $speed = ($done - $lastLogBytes) / $dt
                        Write-Host (">> Download {0}% ({1}/{2}) {3}" -f $threshold, (Format-Bytes -Bytes $done), (Format-Bytes -Bytes $total), (Format-Rate -BytesPerSecond $speed))
                        $lastLogThreshold = $threshold
                        $lastLogTime = $now
                        $lastLogBytes = $done
                    }
                } else {
                    if (($now - $lastLogTime).TotalSeconds -ge 15) {
                        $speed = ($done - $lastLogBytes) / $dt
                        Write-Host (">> Downloaded {0} {1}" -f (Format-Bytes -Bytes $done), (Format-Rate -BytesPerSecond $speed))
                        $lastLogTime = $now
                        $lastLogBytes = $done
                    }
                }

                Start-Sleep -Seconds 2
            }

            return
        }
        catch {
            try {
                if ($job) { Remove-BitsTransfer -BitsJob $job -Confirm:$false -ErrorAction SilentlyContinue | Out-Null }
            } catch {}
            if (Test-Path $DestinationPath) { Remove-Item $DestinationPath -Force -ErrorAction SilentlyContinue }
        }
    }

    $handler = New-Object System.Net.Http.HttpClientHandler
    $handler.AllowAutoRedirect = $true
    $client = [System.Net.Http.HttpClient]::new($handler)
    try {
        if ($TimeoutMs -gt 0) {
            $client.Timeout = [TimeSpan]::FromMilliseconds($TimeoutMs)
        } else {
            $client.Timeout = [TimeSpan]::FromMinutes(30)
        }

        $resp = $client.GetAsync($Url, [System.Net.Http.HttpCompletionOption]::ResponseHeadersRead).GetAwaiter().GetResult()
        $resp.EnsureSuccessStatusCode() | Out-Null

        $total = $resp.Content.Headers.ContentLength
        if ($null -eq $total) { $total = 0 }
        $total = [double]$total

        $stream = $resp.Content.ReadAsStreamAsync().GetAwaiter().GetResult()
        try {
            $fs = [System.IO.File]::Open($DestinationPath, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
            try {
                $buffer = New-Object byte[] (1024 * 1024)
                $done = 0.0
                while ($true) {
                    $read = $stream.Read($buffer, 0, $buffer.Length)
                    if ($read -le 0) { break }
                    $fs.Write($buffer, 0, $read)
                    $done += $read

                    $now = [DateTimeOffset]::UtcNow
                    $dt = ($now - $lastLogTime).TotalSeconds
                    if ($dt -le 0) { $dt = 0.001 }

                    if ($total -gt 0) {
                        $pct = [int][Math]::Floor(($done / $total) * 100)
                        $threshold = [int]([Math]::Floor($pct / 10) * 10)
                        if ($threshold -ge ($lastLogThreshold + 10) -and $threshold -le 100) {
                            $speed = ($done - $lastLogBytes) / $dt
                            Write-Host (">> Download {0}% ({1}/{2}) {3}" -f $threshold, (Format-Bytes -Bytes $done), (Format-Bytes -Bytes $total), (Format-Rate -BytesPerSecond $speed))
                            $lastLogThreshold = $threshold
                            $lastLogTime = $now
                            $lastLogBytes = $done
                        }
                    } else {
                        if (($now - $lastLogTime).TotalSeconds -ge 15) {
                            $speed = ($done - $lastLogBytes) / $dt
                            Write-Host (">> Downloaded {0} {1}" -f (Format-Bytes -Bytes $done), (Format-Rate -BytesPerSecond $speed))
                            $lastLogTime = $now
                            $lastLogBytes = $done
                        }
                    }
                }
            }
            finally {
                $fs.Close()
            }
        }
        finally {
            $stream.Dispose()
        }
    }
    finally {
        $client.Dispose()
        $handler.Dispose()
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

    $mirrorSetting = Get-TomlValue "mirror"
    if ($downloadUrl -match '^https?://repo\.msys2\.org/distrib/x86_64/' -or ($baseUrl -match '^https?://repo\.msys2\.org/distrib/x86_64/?$')) {
        $mirrorHosts = @(
            "mirror.umd.edu",
            "mirror.accum.se",
            "ftp.nluug.nl",
            "ftp2.osuosl.org",
            "mirror.internet.asn.au",
            "mirror.selfnet.de",
            "mirror.yandex.ru",
            "mirrors.dotsrc.org",
            "mirrors.tuna.tsinghua.edu.cn",
            "mirrors.ustc.edu.cn",
            "mirror.nju.edu.cn",
            "mirrors.bfsu.edu.cn",
            "mirror.clarkson.edu",
            "distrohub.kyiv.ua",
            "mirror.archlinux.tw",
            "us.mirrors.cicku.me",
            "ca.mirrors.cicku.me"
        )

        $candidateRoots = New-Object System.Collections.Generic.List[string]
        if ($mirrorSetting) {
            $m = $mirrorSetting.Trim()
            if ($m) {
                $candidateRoots.Add($m.TrimEnd('/'))
            }
        }
        $candidateRoots.Add("https://repo.msys2.org")
        foreach ($h in $mirrorHosts) {
            $candidateRoots.Add(("https://{0}" -f $h))
            $candidateRoots.Add(("https://{0}/msys2" -f $h))
        }

        $downloadPath = $downloadUrl -replace '^https?://[^/]+', ''
        $candidateUrls = $candidateRoots |
            ForEach-Object { ("{0}{1}" -f $_, $downloadPath) } |
            Select-Object -Unique

        Write-Host ">> Probing mirrors for fastest MSYS2 base download..."
        $pickedResult = & (Join-Path $PSScriptRoot "mirror-probe.ps1") -DownloadUrl $downloadUrl -Mirror $mirrorSetting -TimeoutMs 2000
        $picked = $pickedResult.download_url
        if ($picked) {
            $downloadUrl = $picked
            Write-Host ">> Selected fastest mirror: $downloadUrl"
        } else {
            Write-Host ">> Mirror probing failed; using default: $downloadUrl"
        }
    }

    # 如果之前有残留，先删掉
    if (Test-Path $archive) {
        Remove-Item $archive -Force
    }

    Download-FileWithProgress -Url $downloadUrl -DestinationPath $archive -TimeoutMs 0

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
