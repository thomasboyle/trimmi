# Builds a classic Windows Setup.exe for Trimmi (Inno Setup).
param(
    [ValidateSet('x64')]
    [string]$Platform = 'x64',

    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',

    [string]$Version,

    [bool]$ReadyToRun,

    [ValidateSet('lzma2/max', 'lzma2', 'lzma2/fast', 'zip')]
    [string]$Compression,

    [bool]$SolidCompression
)

if (-not $PSBoundParameters.ContainsKey('ReadyToRun')) {
    $ReadyToRun = $true
}
if (-not $PSBoundParameters.ContainsKey('Compression')) {
    $Compression = 'lzma2/max'
}
if (-not $PSBoundParameters.ContainsKey('SolidCompression')) {
    $SolidCompression = $true
}

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false

$Root = Resolve-Path (Join-Path $PSScriptRoot '..')
$AppProject = Join-Path $Root 'Trimmi.App\Trimmi.App.csproj'
$IssFile = Join-Path $Root 'installer\Trimmi.iss'
$OutputDir = Join-Path $Root 'installer\output'
$PublishDir = Join-Path $Root "Trimmi.App\bin\$Configuration\net8.0-windows10.0.26100.0\win-$Platform\setup-publish"
$FfmpegDir = Join-Path $Root 'Trimmi.App\Assets\ffmpeg'

function Find-Iscc {
    $candidates = @(
        "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
        "$env:ProgramFiles\Inno Setup 6\ISCC.exe"
        "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe"
    )
    foreach ($path in $candidates) {
        if (Test-Path $path) {
            return $path
        }
    }
    return $null
}

if (-not (Test-Path $AppProject)) {
    throw "App project not found: $AppProject"
}

if (-not (Test-Path $IssFile)) {
    throw "Inno Setup script not found: $IssFile"
}

$ffmpeg = Join-Path $FfmpegDir 'ffmpeg.exe'
$ffprobe = Join-Path $FfmpegDir 'ffprobe.exe'
$ffmpegDlls = @(Get-ChildItem -LiteralPath $FfmpegDir -Filter '*.dll' -File -ErrorAction SilentlyContinue)
if (-not (Test-Path $ffmpeg) -or -not (Test-Path $ffprobe) -or $ffmpegDlls.Count -eq 0) {
    throw "FFmpeg shared build missing under $FfmpegDir (need ffmpeg.exe, ffprobe.exe, and codec DLLs). Run scripts/get-ffmpeg.ps1."
}

$iscc = Find-Iscc
if (-not $iscc) {
    throw @"
Inno Setup 6 was not found.
Install it with:
  winget install --id JRSoftware.InnoSetup -e
Then re-run this script.
"@
}

if ($Version) {
    if ($Version -notmatch '^\d+\.\d+\.\d+$') {
        throw "Version must look like major.minor.patch (got '$Version')."
    }
    Write-Host "Using version $Version"
}

Write-Host "Publishing Trimmi ($Configuration, $Platform, unpackaged)..."
if (Test-Path $PublishDir) {
    Remove-Item $PublishDir -Recurse -Force
}

$publishArgs = @(
    $AppProject
    '-c', $Configuration
    '-r', "win-$Platform"
    "-p:Platform=$Platform"
    "-p:PublishDir=$PublishDir"
    '-p:WindowsPackageType=None'
    '-p:WindowsAppSDKSelfContained=true'
    '-p:SelfContained=true'
    '-p:PublishSingleFile=false'
    "-p:PublishReadyToRun=$ReadyToRun"
    '-p:PublishTrimmed=false'
    '-p:GenerateAppxPackageOnBuild=false'
    '-p:RunAnalyzers=false'
    '-p:RunAnalyzersDuringBuild=false'
    '-p:EnableNuGetAudit=false'
)
if ($Version) {
    $publishArgs += "-p:Version=$Version"
    $publishArgs += "-p:AssemblyVersion=$Version.0"
    $publishArgs += "-p:FileVersion=$Version.0"
    $publishArgs += "-p:InformationalVersion=$Version"
}

dotnet publish @publishArgs
if ($LASTEXITCODE -ne 0) {
    throw "dotnet publish failed with exit code $LASTEXITCODE"
}

$appExe = Join-Path $PublishDir 'Trimmi.App.exe'
if (-not (Test-Path $appExe)) {
    throw "Published app not found: $appExe"
}

$unusedPublishPatterns = @(
    'onnxruntime.dll'
    'DirectML.dll'
    'Microsoft.ML.OnnxRuntime.dll'
    'Microsoft.Windows.AI*'
    'Microsoft.Windows.Internal.AI*'
    'Microsoft.Windows.Widgets*'
    'Microsoft.Web.WebView2*'
    'WebView2Loader.dll'
    'Microsoft.UI.Xaml.Phone.dll.mui'
    'Microsoft.DiaSymReader*'
    'mscordaccore*'
    'mscordbi.dll'
    'Microsoft.VisualBasic*'
    'Microsoft.Security.Authentication.OAuth*'
    'System.Net.Mail.dll'
    'Microsoft.Graphics.Canvas*'
)
$removedBytes = [long]0
foreach ($pattern in $unusedPublishPatterns) {
    Get-ChildItem -LiteralPath $PublishDir -Recurse -File -Filter $pattern -ErrorAction SilentlyContinue |
        ForEach-Object {
            $removedBytes += $_.Length
            Remove-Item -LiteralPath $_.FullName -Force
        }
}

if ($removedBytes -gt 0) {
    Write-Host ("Stripped unused WinUI/.NET publish files: {0:N1} MB" -f ($removedBytes / 1MB))
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$setupIcon = Join-Path $Root 'Trimmi.App\Assets\AppIcon.ico'
$solidValue = if ($SolidCompression) { 'yes' } else { 'no' }
$isccArgs = @(
    "/DPublishDir=$PublishDir"
    "/DOutputDir=$OutputDir"
    "/DCompression=$Compression"
    "/DSolidCompression=$solidValue"
)
if ($Version) {
    $isccArgs += "/DMyAppVersion=$Version"
}
if (Test-Path $setupIcon) {
    $isccArgs += "/DSetupIcon=$setupIcon"
}

Write-Host "Compiling installer with Inno Setup..."
& $iscc @isccArgs $IssFile
if ($LASTEXITCODE -ne 0) {
    throw "Inno Setup compilation failed with exit code $LASTEXITCODE"
}

$setupExe = Get-ChildItem $OutputDir -Filter 'TrimmiSetup-*.exe' |
    Where-Object { $_.Name -match '^TrimmiSetup-\d+\.\d+\.\d+\.exe$' } |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1

if (-not $setupExe) {
    throw "Setup.exe was not produced in $OutputDir"
}

$stableSetup = Join-Path $OutputDir 'TrimmiSetup.exe'
Copy-Item -LiteralPath $setupExe.FullName -Destination $stableSetup -Force

Write-Host ""
Write-Host "Installer ready:"
Write-Host "  $($setupExe.FullName)"
Write-Host "  $stableSetup"
Write-Host "  Size: $([math]::Round($setupExe.Length / 1MB, 1)) MB"
