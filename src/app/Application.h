#pragma once

#include <array>
#include <windows.h>
#include <shellapi.h>

#include "audio/WasapiLoopback.h"
#include "dsp/SpectrumProcessor.h"
#include "render/LayeredRenderer.h"

namespace wavebar {

class Application {
public:
    Application() = default;
    ~Application();

    bool Initialize(HINSTANCE instance);
    int Run();

private:
    static LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    bool CreateVisualizerWindow();
    bool AddTrayIcon();
    void RemoveTrayIcon();
    void ShowTrayMenu();
    void HandleCommand(UINT command);
    bool IsAutoStartEnabled() const;
    bool SetAutoStartEnabled(bool enabled) const;
    void SetAdjustingPosition(bool adjusting);
    void SetTopmost(bool topmost);
    void SetSkin(SkinType skin);
    void PositionInitially();
    void StartAnimation();
    void StopAnimation();
    void UpdateSpectrum();
    void Render();

    HINSTANCE instance_ = nullptr;
    HWND window_ = nullptr;
    HICON trayIcon_ = nullptr;
    NOTIFYICONDATAW trayData_{};
    WasapiLoopback audio_;
    SpectrumProcessor spectrumProcessor_;
    LayeredRenderer renderer_;
    SpectrumFrame spectrumFrame_{};
    std::array<float, WasapiLoopback::kAnalysisSampleCount> audioSamples_{};
    SkinType skin_ = SkinType::NeonSpectrumLine;
    bool adjustingPosition_ = false;
    bool topmost_ = false;
    bool animationRunning_ = false;
    ULONGLONG lastActivityTick_ = 0;
    UINT taskbarCreatedMessage_ = 0;
    HPOWERNOTIFY suspendResumeNotification_ = nullptr;
};

}  // namespace wavebar
