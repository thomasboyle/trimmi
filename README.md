# Trimmi

Portable Windows video trimmer (WinUI 3 / .NET 8): drag-and-drop, live preview, filmstrip timeline, GPU AV1 when available (NVENC / AMF / QSV) with CPU fallback (SVT-AV1 / libaom).

## Requirements

- Windows 10/11 x64
- [.NET 8 SDK](https://dotnet.microsoft.com/download/dotnet/8.0)
- Visual Studio 2022+ with **Windows App SDK** / WinUI workload (or build from CLI)
- Bundled `ffmpeg.exe` / `ffprobe.exe` via `.\scripts\get-ffmpeg.ps1`

## Build

```powershell
.\scripts\get-ffmpeg.ps1
dotnet build Trimmi.sln -c Debug -p:Platform=x64 -p:WindowsPackageType=None
```

Run unpackaged:

```powershell
dotnet run --project Trimmi.App -c Debug -p:Platform=x64 -p:WindowsPackageType=None
```

## Usage

1. Drop a video (or **Select File**).
2. Set **Start** / **End** (handles or time fields).
3. Choose encoder / format, then **Trim & Export**.

## Versioning

Version is in `VERSION` (`X.Y.Z`):

```powershell
.\scripts\version.ps1 -Action get
.\scripts\version.ps1 -Action set -Version 1.2.0
.\scripts\version.ps1 -Action bump -Part patch   # minor | major
```

## Release

**GitHub Actions:** [`.github/workflows/release.yml`](.github/workflows/release.yml) — on push to `main` (and via **Actions → Release → Run workflow**). Publishes `vX.Y.Z` with `TrimmiSetup-X.Y.Z.exe`, then patch-bumps `VERSION`.

**Local installer** (Inno Setup 6):

```powershell
.\scripts\get-ffmpeg.ps1
.\scripts\build-installer.ps1 -Version (Get-Content VERSION).Trim()
```

Output: `installer\output\TrimmiSetup-<VERSION>.exe`.

## License

As-is for Trimmi. Pixelify Sans is OFL. FFmpeg has its own license (GPL for the bundled shared build).
