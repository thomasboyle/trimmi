# Trimmi

Portable Windows video trimmer (C++ / Qt 6): drag-and-drop, live preview, filmstrip timeline, GPU AV1 when available (NVENC / AMF / QSV) with CPU fallback (SVT-AV1 / libaom).

## Requirements

- Windows 10/11 x64
- Visual Studio 2022 (MSVC) + CMake 3.21+
- Qt 6 (Widgets, Multimedia, Svg) — via installer or [vcpkg](https://github.com/microsoft/vcpkg)
- `ffmpeg.exe` / `ffprobe.exe` on `PATH` or beside `Trimmi.exe` ([gyan.dev](https://www.gyan.dev/ffmpeg/builds/) essentials)

## Build

With system Qt (fast — same approach as CI):

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Or via vcpkg (`vcpkg.json` pulls Qt only; first configure is slow):

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake `
  -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build --config Release
```

Output: `build\Release\Trimmi.exe`.

## Usage

1. Drop a video (or **Select File**).
2. Set **Start** / **End** (handles or time fields).
3. Choose encoder / format, then **Trim & Export**.

`Space` play/pause · `Esc` exit fullscreen

## Versioning

Version is in `VERSION` (`X.Y.Z`):

```powershell
.\scripts\version.ps1 -Action get
.\scripts\version.ps1 -Action set -Version 1.2.0
.\scripts\version.ps1 -Action bump -Part patch   # minor | major
```

## Release

**GitHub Actions:** [`.github/workflows/release.yml`](.github/workflows/release.yml) — **Actions → Release → Run workflow**. Builds, packs the Inno installer, publishes `vX.Y.Z` with `TrimmiSetup-X.Y.Z.exe`, then bumps `VERSION` on `main`.

**Local installer** (Inno Setup 6, after a Release build):

```powershell
.\installer\build_installer.ps1
```

Stages a self-contained payload (Qt, CRT, FFmpeg) and produces `dist\TrimmiSetup-<VERSION>.exe`. Stage only: `.\installer\stage_payload.ps1`.

## License

As-is for Trimmi. Qt and FFmpeg have their own licenses (LGPL/GPL depending on build).
