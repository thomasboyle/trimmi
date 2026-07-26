# Trimmi

Simple, fast video trimmer for Windows (portable C++ / Qt 6).

Trimmi provides a dark, modern UI with drag-and-drop loading, live preview, a filmstrip timeline with start/end handles, GPU AV1 encoding when available (NVENC / AMF / QSV), and automatic CPU fallback (SVT-AV1 / libaom).

## Requirements

- Windows 10/11 x64
- Visual Studio 2022 (MSVC) with C++ desktop workload
- [CMake](https://cmake.org/) 3.21+
- [vcpkg](https://github.com/microsoft/vcpkg) (recommended)
- FFmpeg tools (`ffmpeg.exe` / `ffprobe.exe`) on `PATH` or copied next to `Trimmi.exe` for export

## Build with vcpkg (recommended)

```powershell
# Clone vcpkg once (if you do not already have it)
git clone https://github.com/microsoft/vcpkg C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat

cd D:\C++\Trimmi
cmake -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake `
  -DVCPKG_TARGET_TRIPLET=x64-windows

cmake --build build --config Release
```

The first configure downloads and builds Qt 6 + FFmpeg via the `vcpkg.json` manifest. This can take a while.

Output binary:

```
build\Release\Trimmi.exe
```

### Manual FFmpeg

If you prefer a prebuilt FFmpeg instead of the vcpkg port:

1. Download a full build (with NVENC/SVT-AV1) from [gyan.dev](https://www.gyan.dev/ffmpeg/builds/) or BtbN.
2. Set `FFMPEG_DIR` to the FFmpeg prefix (must contain `include/` and `lib/`), **or** ensure `ffmpeg.exe` is on `PATH` for export even when libs come from vcpkg.
3. Copy `ffmpeg.exe` and `ffprobe.exe` next to `Trimmi.exe` for distribution.

## Architecture

| Component | Role |
|-----------|------|
| `MainWindow` | Frameless shell, sidebar, wiring |
| `VideoPlayer` | Qt Multimedia preview + libav metadata probe |
| `TimelineWidget` | Custom scrubber, filmstrip, trim handles, playhead |
| `ThumbnailGenerator` | Async FFmpeg frame extract (Qt Concurrent) |
| `EncoderCapabilities` | DXGI GPU name + `ffmpeg -encoders` detection |
| `Exporter` | Background `ffmpeg` process, progress + GPU→CPU fallback |

Preview uses **Qt Multimedia**. Metadata/thumbnails use **libav**. Export uses the **ffmpeg CLI** so NVENC/AV1 presets stay reliable.

## Usage

1. Drop a video onto the left panel (or **Select File**).
2. Drag **Start** / **End** handles (or edit the time fields).
3. Choose encoder / format.
4. Click **Trim & Export** and pick an output path.

Keyboard:

- `Space` — play / pause
- `Esc` — exit fullscreen preview

## Versioning

The release version lives in the root `VERSION` file (`X.Y.Z`). Keep it in sync with:

```powershell
.\scripts\version.ps1 -Action get
.\scripts\version.ps1 -Action set -Version 1.2.0
.\scripts\version.ps1 -Action bump -Part patch   # also: minor | major
```

## GitHub Actions release

Workflow: [`.github/workflows/release.yml`](.github/workflows/release.yml)

1. Open **Actions → Release → Run workflow**
2. Choose the post-release bump (`patch` by default, or `minor` / `major` / `none`)
3. The job builds Trimmi (vcpkg + Qt + FFmpeg), stages the standalone payload, compiles the Inno installer, and publishes a GitHub Release (`vX.Y.Z`) with `TrimmiSetup-X.Y.Z.exe`
4. On success it bumps `VERSION` on `main` for the next release

The first run can take a long time while vcpkg builds Qt/FFmpeg; later runs reuse the GitHub Actions binary cache.

## Standalone installer (local)

The Setup.exe is **self-contained**. End users do **not** need Qt, FFmpeg, vcpkg, or the Visual C++ redistributable installed separately.

On a build machine (after a Release build):

```powershell
# Requires Inno Setup 6: https://jrsoftware.org/isinfo.php
cd D:\C++\Trimmi
.\installer\build_installer.ps1
```

This will:

1. Stage `dist\payload\` with `Trimmi.exe`, Qt plugins, MSVC CRT (`windeployqt --compiler-runtime`), FFmpeg DLLs, and `ffmpeg.exe` / `ffprobe.exe`
2. Download a static FFmpeg essentials build if tools are missing (use `-SkipFfmpegDownload` to disable)
3. Compile `dist\TrimmiSetup-<VERSION>.exe`

Only ship that Setup.exe. Or stage without compiling:

```powershell
.\installer\stage_payload.ps1
```

## License

Source is provided as-is for the Trimmi project. Qt and FFmpeg are subject to their own licenses (LGPL/GPL depending on your build).
