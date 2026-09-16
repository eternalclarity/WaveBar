<div align="center">
<img src="./assets/icons/wavebar-icon.png" width="148" alt="WaveBar 图标">

# WaveBar

**轻量、原生、即开即用的 Windows 桌面音乐可视化组件**

[简体中文](./README.md) | [English](./README_EN.md)

[![Windows](https://img.shields.io/badge/Windows-10%20%7C%2011-0078D4?logo=windows&logoColor=white)](https://www.microsoft.com/windows)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white)](https://isocpp.org/)
[![MIT](https://img.shields.io/badge/License-MIT-7C3AED.svg)](./LICENSE)
![Portable](https://img.shields.io/badge/Portable-Single%20EXE-20C997)
![No Rainmeter](https://img.shields.io/badge/Rainmeter-Not%20Required-26E7E1)

[![观看实际运行演示](https://img.shields.io/badge/%E2%96%B6_%E8%A7%82%E7%9C%8B%E5%AE%9E%E9%99%85%E8%BF%90%E8%A1%8C%E6%BC%94%E7%A4%BA-D62CFF?style=for-the-badge)](./assets/wavebar-demo.mp4)

<sub>点击上方按钮打开仓库内的 MP4 演示</sub>
</div>

---

## 关于我开发WaveBar的背景

某一天，我一边愉快地听歌，一边愉快地敲代码，渐渐沉浸其中，无法自拔。

我突然想到：如果桌面上有一条跟随音乐律动的可视化水波，沉浸感应该会更好。

上网一搜，大多数方案都是 Rainmeter 雨滴插件加至美化整套皮肤。效果确实很炫，但安装繁琐、配置杂乱，还捆绑天气、时钟、系统监控、播放器面板等大量不需要的组件。为了得到一根音乐可视化条，却要带上一整套桌面美化系统，可谓“拔出萝卜带出泥”。

于是，我决定开发 WaveBar：

- 只专注音乐可视化；
- 不依赖 Rainmeter 或额外运行时；
- 没有标题栏、任务栏窗口和控制面板；
- 一个便携 EXE，打开即可使用；
- 尽可能降低体积、内存和空闲占用。

> 让音乐在桌面上呼吸，同时少一点不必要的负担。

## 最终效果

<p align="center">
  <a href="./assets/wavebar-demo.mp4">
    <img src="./assets/designs/neon-spectrum-line.svg" width="92%" alt="WaveBar 水波预览">
  </a>
  <br>
  <strong>点击水波观看实际运行视频</strong>
</p>

<table>
<tr>
<td width="50%" align="center">
<img src="./assets/designs/neon-spectrum-line.svg" width="100%" alt="Neon Water Wave"><br>
<strong>Neon Water Wave</strong><br>
频谱驱动的一维阻尼水波
</td>
<td width="50%" align="center">
<img src="./assets/designs/pulse-bars.svg" width="100%" alt="Pulse Bars"><br>
<strong>Pulse Bars</strong><br>
圆角渐变频谱柱
</td>
</tr>
</table>

## 功能亮点

- **WASAPI 系统音频捕获**：不限网易云、QQ 音乐、Spotify 或浏览器。
- **透明无边框组件**：没有标题栏、边框和普通任务栏入口。
- **两款内置皮肤**：无需再下载或配置皮肤包。
- **桌面层与置顶模式**：既能融入壁纸，也能保持可见。
- **拖动、锁定与鼠标穿透**：调整完成后不阻挡桌面操作。
- **托盘控制**：所有必要操作集中在系统托盘。
- **开机启动**：托盘菜单一键启用，基于当前用户配置，无需管理员权限。
- **音频自动恢复**：开机设备延迟就绪、休眠唤醒或默认输出变化后自动重连。
- **智能休眠与唤醒**：无有效音频时降低更新频率，音乐恢复后自动唤醒。
- **单实例与高 DPI 支持**：适配现代 Windows 桌面。
- **便携单文件**：无需安装程序和第三方运行时。

## 使用方法

正式构建将发布到 [Releases](https://github.com/eternalclarity/WaveBar/releases)，也可以从源码构建。

右键 WaveBar 托盘图标：

| 操作 | 作用 |
|---|---|
| Neon Spectrum Line | 霓虹水波皮肤 |
| Pulse Bars | 圆角频谱柱皮肤 |
| Adjust Position | 解锁并拖动组件 |
| Always on Top | 切换始终置顶 |
| Start with Windows | 切换登录 Windows 后自动启动 |
| Exit | 退出 |

双击托盘图标可快速切换皮肤。

> Windows 系统输出需要处于非静音状态。

## 性能

当前 Release 实测：

| 指标 | 结果 |
|---|---:|
| EXE | 约 274 KB |
| 工作集 | 约 17 MB |
| 私有内存 | 约 3.3 MB |
| 播放时总 CPU | 通常低于 1% |
| 渲染帧率 | 约 30 FPS |
| 外部运行时 | 0 |

不同硬件、驱动、屏幕和播放内容会产生不同结果。

## 从源码构建

要求：Windows 10/11 x64、LLVM-MinGW x64、PowerShell；无需 Visual Studio。

将 LLVM-MinGW 放到：

~~~text
.tools/llvm-mingw/
~~~

确认存在：

~~~text
.tools/llvm-mingw/bin/clang++.exe
.tools/llvm-mingw/bin/x86_64-w64-mingw32-windres.exe
~~~

构建 Release：

~~~powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Configuration Release
~~~

输出：build/release/WaveBar.exe

音频恢复回归测试（使用模拟设备，不修改系统音频或电源状态）：

~~~powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\test.ps1
~~~

实机验收：保持音乐播放，检查切换默认输出、睡眠唤醒和重新登录后频谱能否恢复。
没有音频包时会清除旧频谱，连续 5 秒无数据会重建采集会话；设备尚未就绪时会持续重试。

---

<div align="center">

**让音乐在桌面上呼吸。**

如果 WaveBar 对你有用，欢迎点一个 Star。

</div>
