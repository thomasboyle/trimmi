#Requires -Version 5.1
<#
.SYNOPSIS
  Build a standalone Trimmi Setup.exe (no Qt/FFmpeg/MSVC install required on target PCs).
#>
param(
    [string]$Configuration = "Release",
    [string]$Version = "",
    [switch]$SkipFfmpegDownload,
    [switch]$SkipStage
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$payload = Join-Path $root "dist\payload"
$iss = Join-Path $PSScriptRoot "Trimmi.iss"

if (-not $Version) {
    $Version = (& (Join-Path $root "scripts\version.ps1") -Action get).Trim()
}

if (-not $SkipStage) {
    $stageArgs = @{
        Configuration = $Configuration
    }
    if ($SkipFfmpegDownload) { $stageArgs.SkipFfmpegDownload = $true }
    & (Join-Path $PSScriptRoot "stage_payload.ps1") @stageArgs
}

if (-not (Test-Path (Join-Path $payload "Trimmi.exe"))) {
    Write-Error "Payload missing Trimmi.exe. Run stage_payload.ps1 first."
}
if (-not (Test-Path (Join-Path $payload "ffmpeg.exe"))) {
    Write-Error "Payload missing ffmpeg.exe — installer would not be standalone."
}
if (-not (Test-Path (Join-Path $payload "platforms\qwindows.dll"))) {
    Write-Error "Payload missing Qt platform plugin — run windeployqt via stage_payload.ps1."
}

$iscc = @(
    "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
    "${env:ProgramFiles}\Inno Setup 6\ISCC.exe"
) | Where-Object { Test-Path $_ } | Select-Object -First 1

if (-not $iscc) {
    Write-Error "Inno Setup 6 not found. Install from https://jrsoftware.org/isinfo.php"
}

$dist = Join-Path $root "dist"
New-Item -ItemType Directory -Force -Path $dist | Out-Null

Write-Host "==> Compiling standalone installer (v$Version)"
& $iscc "/DMyAppVersion=$Version" $iss
if ($LASTEXITCODE -ne 0) {
    Write-Error "ISCC failed with exit code $LASTEXITCODE"
}

$setup = Get-ChildItem $dist -Filter "TrimmiSetup-*.exe" |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1

if (-not $setup) {
    Write-Error "Installer output not found in dist\"
}

Write-Host "==> Standalone installer ready:"
Write-Host "    $($setup.FullName)"
Write-Host "    $([math]::Round($setup.Length / 1MB, 1)) MB"
Write-Host ""
Write-Host "End users only need this Setup.exe — no Qt, FFmpeg, or Visual C++ install."
