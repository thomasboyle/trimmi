#Requires -Version 5.1
param(
    [string]$VcpkgRoot = $env:VCPKG_ROOT,
    [string]$Triplet = "x64-windows"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot

if (-not $VcpkgRoot) {
    foreach ($candidate in @("C:\vcpkg", "D:\vcpkg", "$env:USERPROFILE\vcpkg")) {
        if (Test-Path (Join-Path $candidate "scripts\buildsystems\vcpkg.cmake")) {
            $VcpkgRoot = $candidate
            break
        }
    }
}

if (-not $VcpkgRoot -or -not (Test-Path (Join-Path $VcpkgRoot "scripts\buildsystems\vcpkg.cmake"))) {
    Write-Error "Set VCPKG_ROOT or install vcpkg. Example: git clone https://github.com/microsoft/vcpkg C:\vcpkg"
}

$toolchain = Join-Path $VcpkgRoot "scripts\buildsystems\vcpkg.cmake"
Write-Host "Using vcpkg: $VcpkgRoot"

cmake -B (Join-Path $root "build") -G "Visual Studio 17 2022" -A x64 `
    -DCMAKE_TOOLCHAIN_FILE="$toolchain" `
    -DVCPKG_TARGET_TRIPLET=$Triplet `
    -S $root
