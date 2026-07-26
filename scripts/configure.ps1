#Requires -Version 5.1
param(
    [string]$VcpkgRoot = $env:VCPKG_ROOT,
    [string]$Triplet = "x64-windows",
    [string]$Generator = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot

function Get-PreferredVsGenerator {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $display = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property catalog_productLineVersion
        if ($display -eq "18") { return "Visual Studio 18 2026" }
        if ($display -eq "2022") { return "Visual Studio 17 2022" }
    }
    return "Visual Studio 18 2026"
}

if (-not $Generator) {
    $Generator = Get-PreferredVsGenerator
}

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
Write-Host "Generator: $Generator"

cmake -B (Join-Path $root "build") -G $Generator -A x64 `
    -DCMAKE_TOOLCHAIN_FILE="$toolchain" `
    -DVCPKG_TARGET_TRIPLET=$Triplet `
    -S $root
