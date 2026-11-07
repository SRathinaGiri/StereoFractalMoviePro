## Stereo Fractal Movie Pro v1.1.1 — large VR videos fixed

A bug-fix release for **large 360° VR videos** (e.g. 5760×2880 per eye → a 5760×5760 top-bottom MP4).

### 🐞 Fixed
- **The app closed part-way through a 5760×5760 video** (for example after 13 seconds), leaving a short MP4. At that size FFmpeg's H.264 encoder used about 15 GB of memory with its default settings; when the PC had less to spare, FFmpeg ended the video early and the app then closed without a message.
- For frames larger than 4K the encoder now runs with 6 threads and a 10-frame look-ahead: **about 5 GB instead of 15 GB**, at the same speed and the same file size (measured on a 501-frame 5760×5760 video).

### 💬 Clearer errors
- If FFmpeg stops before the last frame, the app now stops and says after which frame and why, instead of carrying on.
- If memory runs out, the app shows "Not enough memory… close other programs" instead of closing.
- Error messages show FFmpeg's actual error lines rather than the end of its encoder statistics.

Everything else is as in v1.1.0 (360° VR stereo and mono, Parallel, Cross-Eye, Color and Grayscale Anaglyph).

---

### 📦 Downloads

| Download | File | Description |
| :--- | :--- | :--- |
| **Installer** | `StereoFractalMoviePro-Setup-v1.1.1.exe` | Windows installer with desktop & start menu shortcuts + uninstaller |
| **Portable** | `StereoFractalMoviePro-Portable-v1.1.1.zip` | Extract and run `StereoMoviePro.exe`, no installation needed |
| **Source Code** | `StereoFractalMoviePro-Source-v1.1.1.zip` | C++ / Qt source archive |

FFmpeg must be on your PATH (as in earlier versions).

---
*Built with Qt 6.8.3 MinGW 64-bit and CMake on GitHub Actions.*
