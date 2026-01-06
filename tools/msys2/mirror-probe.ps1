param(
    [string]$DownloadUrl = "",
    [string]$Mirror = "",
    [int]$TimeoutMs = 2000,
    [switch]$ShowAll
)

$ErrorActionPreference = "Stop"

function Measure-UrlLatencyMs {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Url,

        [int]$TimeoutMs = 2000
    )

    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    try {
        $req = [System.Net.HttpWebRequest]::Create($Url)
        $req.Method = "HEAD"
        $req.Timeout = $TimeoutMs
        $req.ReadWriteTimeout = $TimeoutMs
        $req.AllowAutoRedirect = $true
        $resp = $req.GetResponse()
        $resp.Close()
        $sw.Stop()
        return [int]$sw.ElapsedMilliseconds
    }
    catch {
        try {
            $req = [System.Net.HttpWebRequest]::Create($Url)
            $req.Method = "GET"
            $req.Timeout = $TimeoutMs
            $req.ReadWriteTimeout = $TimeoutMs
            $req.AllowAutoRedirect = $true
            $req.AddRange(0, 0)
            $resp = $req.GetResponse()
            $resp.Close()
            $sw.Stop()
            return [int]$sw.ElapsedMilliseconds
        }
        catch {
            return $null
        }
    }
}

function Get-MirrorName {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Root
    )

    try {
        return ([Uri]$Root).Host
    }
    catch {
        return $Root
    }
}

function Format-MirrorArg {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Root
    )

    return ('-Mirror "{0}"' -f $Root)
}

function Pick-FastestCandidate {
    param(
        [Parameter(Mandatory = $true)]
        [object[]]$Candidates,

        [int]$TimeoutMs = 2000,

        [switch]$ShowAll
    )

    $bestMs = $null
    $best = $null

    foreach ($c in $Candidates) {
        if ($null -eq $c) { continue }
        $url = $c.url
        if (-not $url) { continue }

        $ms = Measure-UrlLatencyMs -Url $url -TimeoutMs $TimeoutMs
        if ($ShowAll) {
            $name = $c.name
            $arg = $c.arg
            if ($null -eq $ms) {
                Write-Host ("{0}  {1}  {2} ms  {3}" -f $name, $arg, "FAIL", $url)
            } else {
                Write-Host ("{0}  {1}  {2,4} ms  {3}" -f $name, $arg, $ms, $url)
            }
        }
        if ($null -eq $ms) { continue }
        if ($null -eq $bestMs -or $ms -lt $bestMs) {
            $bestMs = $ms
            $best = [pscustomobject]@{
                name = $c.name
                arg = $c.arg
                root = $c.root
                url = $url
                latency_ms = $ms
            }
        }
    }

    return $best
}

function Get-TomlValue {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,

        [Parameter(Mandatory = $true)]
        [string]$Key
    )

    $m = Select-String -Path $Path -Pattern ("^\s*{0}\s*=" -f [regex]::Escape($Key)) -ErrorAction SilentlyContinue
    if (-not $m) { return "" }
    $line = $m.Line
    $parts = $line.Split('"')
    if ($parts.Length -lt 2) { return "" }
    return $parts[1]
}

function Get-Msys2BaseDownloadUrlFromConfig {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot
    )

    $cfg = Join-Path $RepoRoot "msys2.toml"
    if (-not (Test-Path $cfg)) { throw "msys2.toml not found at repo root" }

    $baseUrl = Get-TomlValue -Path $cfg -Key "base_url"
    if (-not $baseUrl) { throw "base_url missing in msys2.toml" }

    if ($baseUrl -match '\.tar\.(zst|xz)$') {
        return $baseUrl
    }

    $version = Get-TomlValue -Path $cfg -Key "version"
    if (-not $version) { throw "version missing in msys2.toml" }
    $versionForUrl = $version -replace '-', ''
    return ("{0}/msys2-base-x86_64-{1}.tar.xz" -f $baseUrl.TrimEnd('/'), $versionForUrl)
}

function New-Msys2MirrorCandidates {
    param(
        [Parameter(Mandatory = $true)]
        [string]$DownloadUrl,

        [string]$Mirror = ""
    )

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
    if ($Mirror) {
        $m = $Mirror.Trim().TrimEnd('/')
        if ($m) { $candidateRoots.Add($m) }
    }
    $candidateRoots.Add("https://repo.msys2.org")
    foreach ($h in $mirrorHosts) {
        $candidateRoots.Add(("https://{0}" -f $h))
        $candidateRoots.Add(("https://{0}/msys2" -f $h))
    }

    $downloadPath = $DownloadUrl -replace '^https?://[^/]+', ''
    return $candidateRoots |
        ForEach-Object {
            $r = $_
            [pscustomobject]@{
                root = $r
                name = Get-MirrorName -Root $r
                arg = Format-MirrorArg -Root $r
                url = ("{0}{1}" -f $r, $downloadPath)
            }
        } |
        Sort-Object -Property url -Unique
}

if ($DownloadUrl -eq "") {
    $repo = Resolve-Path (Join-Path $PSScriptRoot "..\..")
    $DownloadUrl = Get-Msys2BaseDownloadUrlFromConfig -RepoRoot $repo
}

[Net.ServicePointManager]::SecurityProtocol =
    [Net.ServicePointManager]::SecurityProtocol -bor
    [Net.SecurityProtocolType]::Tls12

$candidates = New-Msys2MirrorCandidates -DownloadUrl $DownloadUrl -Mirror $Mirror
$picked = Pick-FastestCandidate -Candidates $candidates -TimeoutMs $TimeoutMs -ShowAll:$ShowAll
if (-not $picked) { exit 1 }

$shouldPrintSelection = $ShowAll -or (-not $env:GITHUB_ACTIONS)
if ($shouldPrintSelection) {
    Write-Host ("Selected: {0}  {1}" -f $picked.name, $picked.arg)
}

[pscustomobject]@{
    download_url = $picked.url
    mirror_name = $picked.name
    mirror_arg = $picked.arg
    mirror_root = $picked.root
    latency_ms = $picked.latency_ms
    timeout_ms = $TimeoutMs
}
