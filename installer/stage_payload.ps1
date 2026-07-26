#Requires -Version 5.1
<#
.SYNOPSIS
  Stage a self-contained Trimmi payload under dist\payload for the Inno installer.
  Bundles: Trimmi.exe, Qt runtime + plugins, MSVC CRT, ffmpeg/ffprobe tools.
#>
param(
    [string]$Configuration = "Release",
    [string]$BuildDir = "",
    [switch]$SkipFfmpegDownload
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
if (-not $BuildDir) {
    $BuildDir = Join-Path $root "build\$Configuration"
}
$payload = Join-Path $root "dist\payload"
$exeName = "Trimmi.exe"
$srcExe = Join-Path $BuildDir $exeName

if (-not (Test-Path $srcExe)) {
    Write-Error "Missing $srcExe — build $Configuration first."
}

Write-Host "==> Staging standalone payload"
Write-Host "    Source : $BuildDir"
Write-Host "    Payload: $payload"

if (Test-Path $payload) {
    Remove-Item -Recurse -Force $payload
}
New-Item -ItemType Directory -Force -Path $payload | Out-Null

# 1) Copy built app + anything already deployed next to it
Copy-Item -Force $srcExe $payload
Get-ChildItem $BuildDir -File -Include *.dll | ForEach-Object {
    Copy-Item -Force $_.FullName $payload
}
foreach ($dir in @("platforms", "styles", "imageformats", "multimedia", "tls", "generic", "iconengines", "networkinformation", "sqldrivers")) {
    $src = Join-Path $BuildDir $dir
    if (Test-Path $src) {
        Copy-Item -Recurse -Force $src (Join-Path $payload $dir)
    }
}

# 2) windeployqt — Qt plugins + MSVC compiler runtime (--compiler-runtime)
function Find-WinDeployQt {
    $candidates = @()
    if ($env:QTDIR) { $candidates += (Join-Path $env:QTDIR "bin\windeployqt.exe") }
    if ($env:QT_ROOT_DIR) { $candidates += (Join-Path $env:QT_ROOT_DIR "bin\windeployqt.exe") }
    $vcpkgInstalled = Join-Path $root "vcpkg_installed\x64-windows\tools\Qt6\bin\windeployqt.exe"
    $candidates += $vcpkgInstalled
    $candidates += (Join-Path $BuildDir "..\vcpkg_installed\x64-windows\tools\Qt6\bin\windeployqt.exe")
    if ($env:VCPKG_ROOT) {
        $candidates += (Join-Path $env:VCPKG_ROOT "installed\x64-windows\tools\Qt6\bin\windeployqt.exe")
    }
    $fromPath = Get-Command windeployqt.exe -ErrorAction SilentlyContinue
    if ($fromPath) { $candidates += $fromPath.Source }

    # Hunt near Qt6Config from build cache
    $cache = Join-Path $root "build\CMakeCache.txt"
    if (Test-Path $cache) {
        $line = Select-String -Path $cache -Pattern "Qt6_DIR:PATH=(.+)" | Select-Object -First 1
        if ($line) {
            $qtDir = $line.Matches.Groups[1].Value
            $candidates += (Join-Path $qtDir "..\..\..\tools\Qt6\bin\windeployqt.exe")
            $candidates += (Join-Path $qtDir "..\..\..\bin\windeployqt.exe")
            $candidates += (Join-Path $qtDir "..\..\..\..\bin\windeployqt.exe")
        }
    }

    foreach ($c in $candidates) {
        $full = [System.IO.Path]::GetFullPath($c)
        if (Test-Path $full) { return $full }
    }
    return $null
}

$windeployqt = Find-WinDeployQt
if (-not $windeployqt) {
    Write-Error "windeployqt.exe not found. Build with Qt/vcpkg so deployment tools are available."
}
Write-Host "    windeployqt: $windeployqt"
& $windeployqt `
    --release `
    --compiler-runtime `
    --no-translations `
    --multimedia `
    (Join-Path $payload $exeName)
if ($LASTEXITCODE -ne 0) {
    Write-Error "windeployqt failed with exit code $LASTEXITCODE"
}

# 3) Bundle ffmpeg.exe / ffprobe.exe (prefer static full builds so export has NVENC/SVT-AV1)
function Find-LocalTool([string]$name) {
    $candidates = @(
        (Join-Path $BuildDir "$name.exe"),
        (Join-Path $root "tools\ffmpeg\$name.exe"),
        (Join-Path $root "vcpkg_installed\x64-windows\tools\ffmpeg\$name.exe"),
        (Join-Path $root "build\vcpkg_installed\x64-windows\tools\ffmpeg\$name.exe")
    )
    if ($env:VCPKG_ROOT) {
        $candidates += (Join-Path $env:VCPKG_ROOT "installed\x64-windows\tools\ffmpeg\$name.exe")
    }
    $onPath = Get-Command "$name.exe" -ErrorAction SilentlyContinue
    if ($onPath) { $candidates += $onPath.Source }
    foreach ($c in $candidates) {
        if ($c -and (Test-Path $c)) { return (Resolve-Path $c).Path }
    }
    return $null
}

$ffmpegExe = Find-LocalTool "ffmpeg"
$ffprobeExe = Find-LocalTool "ffprobe"

if ((-not $ffmpegExe -or -not $ffprobeExe) -and -not $SkipFfmpegDownload) {
    Write-Host "    Downloading standalone FFmpeg (static)…"
    $toolsDir = Join-Path $root "tools\ffmpeg"
    New-Item -ItemType Directory -Force -Path $toolsDir | Out-Null
    $zip = Join-Path $toolsDir "ffmpeg-win64-gpl.zip"
    $urls = @(
        "https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-win64-gpl.zip",
        "https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-essentials.zip"
    )
    foreach ($url in $urls) {
        try {
            Write-Host "    Trying $url"
            Invoke-WebRequest -Uri $url -OutFile $zip -UseBasicParsing
            Expand-Archive -Path $zip -DestinationPath $toolsDir -Force
            $extractedBin = Get-ChildItem -Path $toolsDir -Recurse -Filter "ffmpeg.exe" |
                Where-Object { $_.DirectoryName -match '[\\/]bin$' } |
                Select-Object -First 1
            if ($extractedBin) {
                $binDir = $extractedBin.DirectoryName
                if (-not $ffmpegExe) { $ffmpegExe = Join-Path $binDir "ffmpeg.exe" }
                if (-not $ffprobeExe) { $ffprobeExe = Join-Path $binDir "ffprobe.exe" }
                break
            }
        } catch {
            Write-Warning "FFmpeg download failed: $($_.Exception.Message)"
        }
    }
}

if (-not $ffmpegExe -or -not (Test-Path $ffmpegExe)) {
    Write-Error "ffmpeg.exe not found. Place it on PATH, in build\$Configuration, or tools\ffmpeg, or allow download."
}
if (-not $ffprobeExe -or -not (Test-Path $ffprobeExe)) {
    Write-Error "ffprobe.exe not found."
}

Copy-Item -Force $ffmpegExe (Join-Path $payload "ffmpeg.exe")
Copy-Item -Force $ffprobeExe (Join-Path $payload "ffprobe.exe")
Write-Host "    Bundled ffmpeg : $ffmpegExe"
Write-Host "    Bundled ffprobe: $ffprobeExe"

# 4) Sanity checks
$required = @(
    $exeName,
    "ffmpeg.exe",
    "ffprobe.exe",
    "platforms\qwindows.dll"
)
$missing = @()
foreach ($rel in $required) {
    if (-not (Test-Path (Join-Path $payload $rel))) { $missing += $rel }
}
if ($missing.Count -gt 0) {
    Write-Error ("Payload incomplete. Missing:`n  - " + ($missing -join "`n  - "))
}

$fileCount = (Get-ChildItem $payload -Recurse -File).Count
$sizeMb = [math]::Round(((Get-ChildItem $payload -Recurse -File | Measure-Object -Property Length -Sum).Sum / 1MB), 1)
Write-Host "==> Payload ready: $fileCount files, ${sizeMb} MB"
Write-Host "    $payload"
