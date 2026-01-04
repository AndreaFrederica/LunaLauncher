#!/usr/bin/env pwsh
<#
.SYNOPSIS
    安装 LunaLauncher JavaScript API

.DESCRIPTION
    将 JavaScript API 文件安装到用户数据目录，支持从 URL 或本地文件安装

.PARAMETER Source
    API 文件来源（URL 或本地文件路径）

.PARAMETER Force
    强制覆盖已存在的文件

.EXAMPLE
    .\install-jsapi.ps1 -Source "https://example.com/custom_api.js"

.EXAMPLE
    .\install-jsapi.ps1 -Source ".\my_custom_api.js" -Force
#>

param(
    [Parameter(Mandatory=$true)]
    [string]$Source,

    [Parameter(Mandatory=$false)]
    [switch]$Force
)

# 确定 API 安装目录
$apiDir = "$env:APPDATA\LunaLauncher\jsapi"

Write-Host "LunaLauncher JavaScript API 安装器" -ForegroundColor Cyan
Write-Host "=================================" -ForegroundColor Cyan
Write-Host ""

# 创建目录
if (-not (Test-Path $apiDir)) {
    Write-Host "创建 API 目录: $apiDir" -ForegroundColor Yellow
    New-Item -ItemType Directory -Force -Path $apiDir | Out-Null
}

# 确定文件名
if ($Source -match "^https?://") {
    # 从 URL 下载
    $fileName = [System.IO.Path]::GetFileName($Source)
    if ([string]::IsNullOrEmpty($fileName)) {
        $fileName = "api_$(Get-Date -Format 'yyyyMMddHHmmss').js"
    }

    $destPath = Join-Path $apiDir $fileName

    # 检查文件是否已存在
    if ((Test-Path $destPath) -and -not $Force) {
        Write-Host "文件已存在: $destPath" -ForegroundColor Red
        Write-Host "使用 -Force 参数强制覆盖" -ForegroundColor Yellow
        exit 1
    }

    Write-Host "正在下载: $Source" -ForegroundColor Green
    Write-Host "保存到: $destPath" -ForegroundColor Green

    try {
        Invoke-WebRequest -Uri $Source -OutFile $destPath -ErrorAction Stop
        Write-Host "✓ 下载成功!" -ForegroundColor Green
    }
    catch {
        Write-Host "✗ 下载失败: $_" -ForegroundColor Red
        exit 1
    }
}
else {
    # 从本地文件复制
    if (-not (Test-Path $Source)) {
        Write-Host "✗ 文件不存在: $Source" -ForegroundColor Red
        exit 1
    }

    $fileName = [System.IO.Path]::GetFileName($Source)
    $destPath = Join-Path $apiDir $fileName

    # 检查文件是否已存在
    if ((Test-Path $destPath) -and -not $Force) {
        Write-Host "文件已存在: $destPath" -ForegroundColor Red
        Write-Host "使用 -Force 参数强制覆盖" -ForegroundColor Yellow
        exit 1
    }

    Write-Host "正在复制: $Source" -ForegroundColor Green
    Write-Host "保存到: $destPath" -ForegroundColor Green

    try {
        Copy-Item -Path $Source -Destination $destPath -Force:$Force -ErrorAction Stop
        Write-Host "✓ 复制成功!" -ForegroundColor Green
    }
    catch {
        Write-Host "✗ 复制失败: $_" -ForegroundColor Red
        exit 1
    }
}

# 验证 API 文件
Write-Host ""
Write-Host "正在验证 API 文件..." -ForegroundColor Yellow

$content = Get-Content $destPath -Raw -ErrorAction SilentlyContinue

if ($content -match 'const\s+api\s*=\s*\{') {
    Write-Host "✓ API 对象声明: 找到" -ForegroundColor Green
}
else {
    Write-Host "⚠ API 对象声明: 未找到 'const api = {'" -ForegroundColor Yellow
}

if ($content -match 'metadata\s*:') {
    Write-Host "✓ 元数据声明: 找到" -ForegroundColor Green
}
else {
    Write-Host "⚠ 元数据声明: 未找到 'metadata:'" -ForegroundColor Yellow
}

# 显示安装信息
Write-Host ""
Write-Host "=================================" -ForegroundColor Cyan
Write-Host "安装完成!" -ForegroundColor Green
Write-Host ""
Write-Host "API 文件: $fileName" -ForegroundColor Cyan
Write-Host "安装位置: $destPath" -ForegroundColor Cyan
Write-Host ""
Write-Host "下一步:" -ForegroundColor Yellow
Write-Host "  1. 重启 LunaLauncher" -ForegroundColor White
Write-Host "  2. API 将自动加载" -ForegroundColor White
Write-Host ""
Write-Host "管理 API:" -ForegroundColor Yellow
Write-Host "  查看已安装: explorer '$apiDir'" -ForegroundColor White
Write-Host "  卸载: Remove-Item '$destPath'" -ForegroundColor White
Write-Host ""
