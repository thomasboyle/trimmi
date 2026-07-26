#Requires -Version 5.1
<#
.SYNOPSIS
  Read / write / bump the Trimmi semver in VERSION and related project files.
#>
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("get", "set", "bump")]
    [string]$Action,

    [string]$Version,
    [ValidateSet("patch", "minor", "major")]
    [string]$Part = "patch"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$versionFile = Join-Path $root "VERSION"

function Get-TrimmiVersion {
    if (-not (Test-Path $versionFile)) {
        Write-Error "VERSION file not found at $versionFile"
    }
    return (Get-Content -Raw $versionFile).Trim()
}

function Test-SemVer([string]$v) {
    return $v -match '^\d+\.\d+\.\d+$'
}

function Set-TrimmiVersion([string]$v) {
    if (-not (Test-SemVer $v)) {
        Write-Error "Version must be semver X.Y.Z (got '$v')"
    }

    [System.IO.File]::WriteAllText($versionFile, "$v`n")

    $vcpkg = Join-Path $root "vcpkg.json"
    if (Test-Path $vcpkg) {
        $content = [System.IO.File]::ReadAllText($vcpkg)
        $updated = [regex]::Replace(
            $content,
            '("version-string"\s*:\s*")\d+\.\d+\.\d+(")',
            "`${1}$v`${2}")
        [System.IO.File]::WriteAllText($vcpkg, $updated)
    }

    $iss = Join-Path $root "installer\Trimmi.iss"
    if (Test-Path $iss) {
        $content = [System.IO.File]::ReadAllText($iss)
        $updated = [regex]::Replace(
            $content,
            '(#define MyAppVersion ")\d+\.\d+\.\d+(")',
            "`${1}$v`${2}")
        [System.IO.File]::WriteAllText($iss, $updated)
    }

    Write-Host "Version set to $v"
}

function Bump-TrimmiVersion([string]$part) {
    $current = Get-TrimmiVersion
    if (-not (Test-SemVer $current)) {
        Write-Error "Current VERSION '$current' is not X.Y.Z"
    }
    $parts = $current.Split('.') | ForEach-Object { [int]$_ }
    switch ($part) {
        "major" { $parts[0]++; $parts[1] = 0; $parts[2] = 0 }
        "minor" { $parts[1]++; $parts[2] = 0 }
        "patch" { $parts[2]++ }
    }
    $next = "{0}.{1}.{2}" -f $parts[0], $parts[1], $parts[2]
    Set-TrimmiVersion $next
    return $next
}

switch ($Action) {
    "get" {
        Write-Output (Get-TrimmiVersion)
    }
    "set" {
        if (-not $Version) { Write-Error "-Version is required for set" }
        Set-TrimmiVersion $Version
    }
    "bump" {
        Write-Output (Bump-TrimmiVersion $Part)
    }
}
