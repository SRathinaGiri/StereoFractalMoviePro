## Stereo Fractal Movie Pro v1.1.0 — 360° VR

Adds **360° VR video output**, so the app now covers flat stereo, anaglyph and VR headsets in one tool.

### 🆕 New output modes
- **360° VR Stereo (Top-Bottom)**: for Meta Quest, YouTube VR and other headset players. Takes 2:1 equirectangular panorama frames (e.g. Mandelbulb3D's spherical panorama camera in stereo) with the left eye on top and the right eye below. Output: `vr360_final_360_TB.mp4`.
- **360° VR Mono**: the left-eye panorama alone, for Facebook 360, YouTube 360 on phones and other mono 360 players. Output: `vr360_mono_final_360.mp4`.

### 🧭 360° metadata
- **Spherical Video V2** (`sv3d` equirectangular + `st3d` top-bottom) written into the MP4.
- **Spherical Video V1** (XMP `uuid` box) written as well, for older players and uploaders.
- File names carry the `_360` / `_TB` suffixes for players that go by the name.

### 🎯 360-aware processing
- **Stereo window** wraps the left eye around the sphere by the offset rather than cropping, so the seam behind the viewer stays intact.
- **3D text** sits in front of the viewer, above and below the horizon, rather than at the poles.
- **Audio** works as in the other modes (AAC, 3-second fade-out), as normal stereo sound (not head-tracked spatial audio).
- Tip: render panoramas at **4096×2048 per eye or larger** for a sharp image in a headset.

### 🔧 Other
- Automated Windows build and test workflow (GitHub Actions, Qt 6.8 MinGW); release packages are now built by CI.
- New unit tests for the spherical metadata and 360° frame processing.

All four v1.0.0 modes (Parallel, Cross-Eye, Color Anaglyph, Grayscale Anaglyph) are unchanged.

---

### 📦 Downloads

| Download | File | Description |
| :--- | :--- | :--- |
| **Installer** | `StereoFractalMoviePro-Setup-v1.1.0.exe` | Windows installer with desktop & start menu shortcuts + uninstaller |
| **Portable** | `StereoFractalMoviePro-Portable-v1.1.0.zip` | Extract and run `StereoMoviePro.exe`, no installation needed |
| **Source Code** | `StereoFractalMoviePro-Source-v1.1.0.zip` | C++ / Qt source archive |

FFmpeg must be on your PATH (as in v1.0.0).

---
*Built with Qt 6.8.3 MinGW 64-bit and CMake on GitHub Actions.*
