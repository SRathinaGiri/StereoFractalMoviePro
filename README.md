# Stereo Fractal Movie Pro

[![Platform](https://img.shields.io/badge/Platform-Windows%20x64-blue.svg)](https://github.com/SRathinaGiri/StereoFractalMoviePro/releases)
[![C++](https://img.shields.io/badge/Language-C%2B%2B20-green.svg)](https://en.cppreference.com/w/cpp/20)
[![Framework](https://img.shields.io/badge/Framework-Qt%206.11-brightgreen.svg)](https://www.qt.io/)
[![Build System](https://img.shields.io/badge/Build-CMake%20%7C%20Ninja-orange.svg)](https://cmake.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Release](https://img.shields.io/github/v/release/SRathinaGiri/StereoFractalMoviePro?include_prereleases&color=purple)](https://github.com/SRathinaGiri/StereoFractalMoviePro/releases)

A modern, standalone, high-performance C++ and Qt 6 desktop application designed to create stereoscopic 3D movies from left/right image sequences (such as Mandelbulb 3D, JWildfire, or stereoscopic camera renders).

Featuring real-time live preview, pixel-accurate stereo window adjustment, floating 3D parallax text overlays, project save/load (JSON), and ultra-fast direct-pipe FFmpeg video encoding without temporary intermediate AVI files.

---

## 🌟 Key Features

### 🎬 6 Output Modes
* **Parallel View (`||`)**: Side-by-side Left + Right for 3D displays, VR headsets, and parallel free-viewing.
* **Cross-Eye View (`X`)**: Side-by-side Right + Left for cross-eyed free-viewing without glasses.
* **Color Anaglyph (Red-Cyan)**: Classic red/cyan 3D glasses format (Red channel $\leftarrow$ Left, Green/Blue $\leftarrow$ Right).
* **Grayscale Anaglyph**: Dubois-weighted monochrome anaglyph minimizing retinal rivalry and color flicker.
* **360° VR Stereo (Top-Bottom)**: For VR headsets (Meta Quest) and YouTube VR. From 360° panorama frames (2:1, e.g. Mandelbulb3D's spherical panorama camera with stereo), the left eye on top and the right eye below. The MP4 carries the **Spherical Video V2** metadata (`st3d` top-bottom, `sv3d` equirectangular), so players show it as a stereo sphere; it is also named `vr360_final_360_TB.mp4` for players that go by the name.
* **360° VR Mono**: The left eye's panorama alone, with the 360° metadata (`vr360_mono_final_360.mp4`): for Facebook 360, YouTube 360 on phones and other mono 360 players.

#### 360° notes
* The **stereo window** of a 360° video does not crop (that would break the seam where the panorama meets itself behind the viewer): the left eye is turned around by the offset instead, wrapping round.
* The **3D text** is placed in front of the viewer, above and below the horizon in the middle of the panorama (at its top and bottom edges it would sit at the poles, squeezed into a point).
* **Audio** is the same as for the other modes (AAC, 3-second fade-out); it plays as ordinary stereo sound, not head-tracked spatial audio.
* For a sharp picture in a headset render the panoramas large: 4096×2048 per eye or more (a 360° panorama spreads its width over the whole circle around you).

### 📐 Pixel-Accurate Stereo Window Adjustment
* **Inward / Outward Cropping**: Eliminate stereo window violations by shifting the convergence plane.
* **Step Controls**: Fine-tune with `◀◀ -10`, `◀ -1`, `+1 ▶`, `+10 ▶▶`, and `Reset` buttons.
* **Automatic Even-Dimension Enforcement**: Automatically aligns cropped dimensions to even numbers for 100% compatibility with H.264 `yuv420p` chroma subsampling.

### 🔤 3D Parallax Text Overlay
* **Stereoscopic Depth Perception**: Title text at the top appears floating in the **foreground** (in front of the screen), while author/credit text at the bottom appears recessed in the **background**.
* **Legibility & Polish**: Anti-aliased typography with subtle dark drop shadows ensuring crystal-clear readability against bright fractal patterns.
* **Custom Timings**: Configurable intro duration (first $N$ seconds) and outro duration (last $M$ seconds).

### ⚡ Direct-Pipe Video Rendering Engine
* **Zero Disk Waste**: Streams raw RGB video directly into FFmpeg's stdin pipe via `QProcess`—no multi-gigabyte temporary AVI files written to disk.
* **Scanline Stride Correction**: Tightly packs pixel scanlines, stripping Qt 32-bit row alignment padding to guarantee zero diagonal shearing or tearing in media players like VLC.
* **Smooth Audio Fade-Out**: Automatically calculates sequence length and applies a 3-second audio fade-out filter (`afade=t=out:st=...:d=3`).
* **H.264 Tuning**: User-adjustable CRF (Constant Rate Factor, 0–51) and playback speed multiplier (0.1× to 10.0×).
* **Stereo Metadata**: Injects standard `stereo_mode=left_right` metadata into parallel and cross MP4 files for automated 3D TV and VR player recognition.

### 💾 Project Save & Import (JSON)
* Save all current project settings (frame paths, audio selection, stereo window offset, 3D text strings, output modes, speed, and CRF) into a standard `.json` file.
* Open existing project files anytime via `File -> Import Project (JSON)...` or `Ctrl+O` / `Ctrl+S`.

### 🖥️ Ergonomic Zero-Scroll GUI
* **Single-Row Setup**: Source folder and background audio selections are consolidated side-by-side in equal-width columns.
* **Tabbed Workflow**: Sections 3 (Stereo Window), 4 (3D Text), and 5 (Output Options) reside inside a compact tab bar (`QTabWidget`).
* **Universal English Digits**: Strict `C` / US English locale formatting preventing regional digit substitution.
* **Responsive Layout**: Designed to fit comfortably on 1080p and scaled laptop displays without vertical scrolling.

---

## 📥 Downloads

Grab the latest pre-compiled binaries from the [**Releases Page**](https://github.com/SRathinaGiri/StereoFractalMoviePro/releases):

| Asset | Description |
| :--- | :--- |
| **`StereoFractalMoviePro-Setup-v1.1.0.exe`** | Standard Windows installer with desktop and start menu shortcuts |
| **`StereoFractalMoviePro-Portable-v1.1.0.zip`** | Portable standalone application folder (run without installation) |
| **`StereoFractalMoviePro-Source-v1.1.0.zip`** | Complete C++ source code archive |

---

## 🛠️ Building from Source

### Prerequisites
1. **C++20 Compiler**: MinGW-w64 (`g++` 13+) or MSVC 2022.
2. **Qt 6**: Qt 6.6 or higher (Qt 6.11 recommended) with `Qt6::Widgets`.
3. **CMake**: 3.20 or newer.
4. **Ninja**: Recommended build tool.
5. **FFmpeg**: Placed in `PATH` or at `D:\utils\ffmpeg\ffmpeg.exe`.

### 1-Click Build (Windows MinGW)
Clone the repository and run:
```bat
build.bat
```
This script configures CMake with Ninja, compiles the executable, runs the test suite, and invokes `windeployqt` to populate the `dist/` directory with all necessary runtime DLLs and plugins.

### Manual CMake Build
```bash
git clone https://github.com/SRathinaGiri/StereoFractalMoviePro.git
cd StereoFractalMoviePro

cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="<path_to_qt6>"
ninja -C build

# Deploy Qt dependencies
windeployqt --compiler-runtime --release dist/StereoMoviePro.exe
```

---

## 📁 Expected Frame Naming Convention

The application automatically scans and pairs image frames matching:
* **Left Frames**: `*Left*.jpg` / `*Left*.png` (e.g. `Frame_0001_Left.jpg`)
* **Right Frames**: `*Right*.jpg` / `*Right*.png` (e.g. `Frame_0001_Right.jpg`)

Frames are numerically sorted by their trailing integer to ensure natural chronological ordering (`Frame_1` through `Frame_100` rather than alphabetical order).

---

## 📄 Project File Specification (`.json`)

Saved project files use a clean, transparent JSON format:

```json
{
    "format": "StereoFractalMoviePro",
    "version": "1.0",
    "folderPath": "C:/Render/FractalAnimation",
    "audioPath": "C:/Audio/soundtrack.mp3",
    "previewMode": "parallel",
    "showGuide": true,
    "guidePosition": 50,
    "windowOffset": -10,
    "topText": "Stereo Fractal Animation",
    "bottomText": "Created by S. Rathinagiri",
    "startDur": 5,
    "endDur": 5,
    "useText": true,
    "doParallel": true,
    "doCross": true,
    "doColorAna": true,
    "doGrayAna": false,
    "doVr360": true,
    "doVr360Mono": false,
    "doPingPong": true,
    "speedFactor": 1.0,
    "crf": 23
}
```

---

## 📜 License

Distributed under the **MIT License**. See [`LICENSE`](LICENSE) for more information.

Developed with ❤️ by **S. Rathinagiri** ([@SRathinaGiri](https://github.com/SRathinaGiri)).
