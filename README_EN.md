<div align="center">
<img src="./assets/icons/wavebar-icon.png" width="148" alt="WaveBar icon">

# WaveBar

**A lightweight, native, plug-and-play desktop music visualizer for Windows**

[简体中文](./README.md) | [English](./README_EN.md)

[![Windows](https://img.shields.io/badge/Windows-10%20%7C%2011-0078D4?logo=windows&logoColor=white)](https://www.microsoft.com/windows)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white)](https://isocpp.org/)
[![MIT](https://img.shields.io/badge/License-MIT-7C3AED.svg)](./LICENSE)
![Portable](https://img.shields.io/badge/Portable-Single%20EXE-20C997)
![No Rainmeter](https://img.shields.io/badge/Rainmeter-Not%20Required-26E7E1)

[![Watch the live demo](https://img.shields.io/badge/%E2%96%B6_Watch_the_live_demo-D62CFF?style=for-the-badge)](./assets/wavebar-demo.mp4)

<sub>Click the button above to open the MP4 demo stored in this repository.</sub>
</div>

---

## Why I Built WaveBar

One day, I was happily listening to music while happily writing code. Gradually, I became completely immersed in the moment.

Then a thought occurred to me: if a visual wave on the desktop could move with the music, the experience would feel even more immersive.

I searched online, but most solutions were Rainmeter paired with a complete ZhutiX skin suite. They certainly looked impressive, but the installation was cumbersome, the configuration was cluttered, and they came bundled with weather widgets, clocks, system monitors, media panels, and many other components I did not need. All I wanted was a single music visualizer, yet I had to bring along an entire desktop customization system—getting one small feature meant dragging a whole ecosystem with it.

So I decided to build WaveBar:

- Focused exclusively on music visualization;
- No dependency on Rainmeter or additional runtimes;
- No title bar, taskbar window, or control panel;
- One portable EXE that works as soon as it is launched;
- Designed to minimize file size, memory usage, and idle resource consumption.

> Let music breathe on the desktop, with less unnecessary weight.

## Final Result

<p align="center">
  <a href="./assets/wavebar-demo.mp4">
    <img src="./assets/designs/neon-spectrum-line.svg" width="92%" alt="WaveBar water-wave preview">
  </a>
  <br>
  <strong>Click the wave to watch the live demo</strong>
</p>

<table>
<tr>
<td width="50%" align="center">
<img src="./assets/designs/neon-spectrum-line.svg" width="100%" alt="Neon Water Wave"><br>
<strong>Neon Water Wave</strong><br>
A spectrum-driven one-dimensional damped water wave
</td>
<td width="50%" align="center">
<img src="./assets/designs/pulse-bars.svg" width="100%" alt="Pulse Bars"><br>
<strong>Pulse Bars</strong><br>
Rounded gradient spectrum bars
</td>
</tr>
</table>

## Highlights

- **WASAPI system audio capture**: Works with NetEase Cloud Music, QQ Music, Spotify, browsers, and more.
- **Transparent frameless component**: No title bar, window border, or standard taskbar entry.
- **Two built-in skins**: No additional skin packages to download or configure.
- **Desktop and always-on-top modes**: Blend into the wallpaper or remain visible above other windows.
- **Drag, lock, and click-through**: Position the component, then keep it out of the way of desktop interaction.
- **System tray controls**: All essential actions are available from the tray icon.
- **Smart sleep and wake**: Reduces update frequency when no valid audio is detected and wakes automatically when music resumes.
- **Single-instance and high-DPI support**: Designed for modern Windows desktops.
- **Portable single file**: No installer or third-party runtime required.

## Usage

Official builds will be published on [Releases](https://github.com/eternalclarity/WaveBar/releases). You can also build WaveBar from source.

Right-click the WaveBar tray icon:

| Action | Result |
|---|---|
| Neon Spectrum Line | Neon water-wave skin |
| Pulse Bars | Rounded spectrum-bar skin |
| Adjust Position | Unlock and drag the component |
| Always on Top | Toggle always-on-top mode |
| Exit | Close WaveBar |

Double-click the tray icon to switch skins quickly.

> Windows system output must not be muted.

## Performance

Current Release measurements:

| Metric | Result |
|---|---:|
| EXE | About 274 KB |
| Working set | About 17 MB |
| Private memory | About 3.3 MB |
| Total CPU during playback | Typically below 1% |
| Rendering frame rate | About 30 FPS |
| External runtimes | 0 |

Results vary depending on hardware, drivers, displays, and audio content.

## Build from Source

Requirements: Windows 10/11 x64, LLVM-MinGW x64, and PowerShell. Visual Studio is not required.

Place LLVM-MinGW at:

~~~text
.tools/llvm-mingw/
~~~

Confirm that the following files exist:

~~~text
.tools/llvm-mingw/bin/clang++.exe
.tools/llvm-mingw/bin/x86_64-w64-mingw32-windres.exe
~~~

Build the Release version:

~~~powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Configuration Release
~~~

Output: build/release/WaveBar.exe

---

<div align="center">

**Let music breathe on your desktop.**

If WaveBar is useful to you, consider giving it a Star.

</div>