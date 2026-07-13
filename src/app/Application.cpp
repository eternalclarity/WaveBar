#include "app/Application.h"
#include "resource.h"

#include <algorithm>

namespace wavebar {
namespace {

constexpr wchar_t kWindowClassName[] = L"WaveBarVisualizerWindow";
constexpr wchar_t kApplicationName[] = L"WaveBar";
constexpr UINT kTrayCallbackMessage = WM_APP + 1;
constexpr UINT kAudioActivityMessage = WM_APP + 2;
constexpr UINT_PTR kRenderTimerId = 1;
constexpr UINT kRenderIntervalMilliseconds = 33;
constexpr UINT kIdlePollIntervalMilliseconds = 250;
constexpr ULONGLONG kIdleDelayMilliseconds = 700;

constexpr UINT kCommandNeonLine = 1001;
constexpr UINT kCommandPulseBars = 1002;
constexpr UINT kCommandAdjustPosition = 1003;
constexpr UINT kCommandTopmost = 1004;
constexpr UINT kCommandExit = 1099;

}  // namespace

Application::~Application() {
    audio_.Stop();
    RemoveTrayIcon();
    if (window_ != nullptr) {
        DestroyWindow(window_);
        window_ = nullptr;
    }
}

bool Application::Initialize(HINSTANCE instance) {
    instance_ = instance;
    taskbarCreatedMessage_ = RegisterWindowMessageW(L"TaskbarCreated");

    if (!renderer_.Initialize() || !CreateVisualizerWindow()) {
        return false;
    }

    PositionInitially();

    if (!AddTrayIcon()) {
        return false;
    }

    ShowWindow(window_, SW_SHOWNOACTIVATE);
    SetWindowPos(
        window_,
        topmost_ ? HWND_TOPMOST : HWND_NOTOPMOST,
        0,
        0,
        0,
        0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);

    Render();

    if (!audio_.Start(window_, kAudioActivityMessage)) {
        return false;
    }

    lastActivityTick_ = GetTickCount64();
    StartAnimation();
    return true;
}

bool Application::CreateVisualizerWindow() {
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.hInstance = instance_;
    windowClass.lpfnWndProc = &Application::WindowProcedure;
    windowClass.lpszClassName = kWindowClassName;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_SIZEALL);
    windowClass.hIcon = LoadIconW(instance_, MAKEINTRESOURCEW(IDI_WAVEBAR));
    windowClass.hIconSm = windowClass.hIcon;

    if (!RegisterClassExW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    const DWORD extendedStyle =
        WS_EX_LAYERED |
        WS_EX_TOOLWINDOW |
        WS_EX_NOACTIVATE |
        WS_EX_TRANSPARENT;

    window_ = CreateWindowExW(
        extendedStyle,
        kWindowClassName,
        kApplicationName,
        WS_POPUP,
        0,
        0,
        1000,
        150,
        nullptr,
        nullptr,
        instance_,
        this);

    return window_ != nullptr;
}

void Application::PositionInitially() {
    RECT workArea{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);

    const int availableWidth = workArea.right - workArea.left;
    const int width = std::clamp(availableWidth * 55 / 100, 680, 1100);
    const int height = 150;
    const int x = workArea.left + (availableWidth - width) / 2;
    const int y = workArea.bottom - height -
        std::max(36, static_cast<int>((workArea.bottom - workArea.top) * 4 / 100));

    SetWindowPos(
        window_,
        nullptr,
        x,
        y,
        width,
        height,
        SWP_NOZORDER | SWP_NOACTIVATE);
}

bool Application::AddTrayIcon() {
    if (trayIcon_ != nullptr) {
        DestroyIcon(trayIcon_);
        trayIcon_ = nullptr;
    }

    trayData_ = {};
    trayData_.cbSize = sizeof(trayData_);
    trayData_.hWnd = window_;
    trayData_.uID = 1;
    trayData_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    trayData_.uCallbackMessage = kTrayCallbackMessage;
    trayIcon_ = static_cast<HICON>(LoadImageW(
        instance_,
        MAKEINTRESOURCEW(IDI_WAVEBAR),
        IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON),
        GetSystemMetrics(SM_CYSMICON),
        LR_DEFAULTCOLOR));
    trayData_.hIcon = trayIcon_ != nullptr
        ? trayIcon_
        : LoadIconW(nullptr, IDI_APPLICATION);
    wcscpy_s(trayData_.szTip, L"WaveBar");

    if (!Shell_NotifyIconW(NIM_ADD, &trayData_)) {
        return false;
    }

    trayData_.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &trayData_);
    return true;
}

void Application::RemoveTrayIcon() {
    if (trayData_.hWnd != nullptr) {
        Shell_NotifyIconW(NIM_DELETE, &trayData_);
        trayData_.hWnd = nullptr;
    }
    if (trayIcon_ != nullptr) {
        DestroyIcon(trayIcon_);
        trayIcon_ = nullptr;
    }
}

void Application::ShowTrayMenu() {
    HMENU menu = CreatePopupMenu();
    HMENU skinMenu = CreatePopupMenu();
    if (menu == nullptr || skinMenu == nullptr) {
        if (skinMenu != nullptr) {
            DestroyMenu(skinMenu);
        }
        if (menu != nullptr) {
            DestroyMenu(menu);
        }
        return;
    }

    AppendMenuW(
        skinMenu,
        MF_STRING | (skin_ == SkinType::NeonSpectrumLine ? MF_CHECKED : 0),
        kCommandNeonLine,
        L"Neon Spectrum Line");
    AppendMenuW(
        skinMenu,
        MF_STRING | (skin_ == SkinType::PulseBars ? MF_CHECKED : 0),
        kCommandPulseBars,
        L"Pulse Bars");

    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(skinMenu), L"Skin");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(
        menu,
        MF_STRING | (adjustingPosition_ ? MF_CHECKED : 0),
        kCommandAdjustPosition,
        L"Adjust Position");
    AppendMenuW(
        menu,
        MF_STRING | (topmost_ ? MF_CHECKED : 0),
        kCommandTopmost,
        L"Always on Top");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kCommandExit, L"Exit");

    POINT cursor{};
    GetCursorPos(&cursor);
    SetForegroundWindow(window_);

    const UINT command = TrackPopupMenu(
        menu,
        TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
        cursor.x,
        cursor.y,
        0,
        window_,
        nullptr);

    if (command != 0) {
        HandleCommand(command);
    }

    DestroyMenu(menu);
}

void Application::HandleCommand(UINT command) {
    switch (command) {
        case kCommandNeonLine:
            SetSkin(SkinType::NeonSpectrumLine);
            break;
        case kCommandPulseBars:
            SetSkin(SkinType::PulseBars);
            break;
        case kCommandAdjustPosition:
            SetAdjustingPosition(!adjustingPosition_);
            break;
        case kCommandTopmost:
            SetTopmost(!topmost_);
            break;
        case kCommandExit:
            DestroyWindow(window_);
            break;
        default:
            break;
    }
}

void Application::SetAdjustingPosition(bool adjusting) {
    adjustingPosition_ = adjusting;

    LONG_PTR style = GetWindowLongPtrW(window_, GWL_EXSTYLE);
    if (adjusting) {
        style &= ~static_cast<LONG_PTR>(WS_EX_TRANSPARENT);
    } else {
        style |= WS_EX_TRANSPARENT;
    }

    SetWindowLongPtrW(window_, GWL_EXSTYLE, style);
    SetWindowPos(
        window_,
        nullptr,
        0,
        0,
        0,
        0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

void Application::SetTopmost(bool topmost) {
    topmost_ = topmost;
    SetWindowPos(
        window_,
        topmost ? HWND_TOPMOST : HWND_NOTOPMOST,
        0,
        0,
        0,
        0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void Application::SetSkin(SkinType skin) {
    skin_ = skin;
    Render();
}

void Application::StartAnimation() {
    if (!animationRunning_ && window_ != nullptr) {
        SetTimer(window_, kRenderTimerId, kRenderIntervalMilliseconds, nullptr);
        animationRunning_ = true;
    }
}

void Application::StopAnimation() {
    if (animationRunning_ && window_ != nullptr) {
        SetTimer(window_, kRenderTimerId, kIdlePollIntervalMilliseconds, nullptr);
        animationRunning_ = false;
    }
}

void Application::UpdateSpectrum() {
    std::uint32_t sampleRate = 48000;
    float rms = 0.0f;
    audio_.CopyLatestSamples(audioSamples_, sampleRate, rms);
    spectrumFrame_ = spectrumProcessor_.Process(audioSamples_, sampleRate, rms);

    if (spectrumFrame_.active) {
        lastActivityTick_ = GetTickCount64();
        StartAnimation();
    }

    Render();

    if (!spectrumFrame_.active &&
        spectrumFrame_.maximum < 0.004f &&
        GetTickCount64() - lastActivityTick_ >= kIdleDelayMilliseconds) {
        StopAnimation();
    }
}

void Application::Render() {
    renderer_.Render(window_, skin_, spectrumFrame_);
}

int Application::Run() {
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

LRESULT CALLBACK Application::WindowProcedure(
    HWND window,
    UINT message,
    WPARAM wParam,
    LPARAM lParam) {
    Application* application = nullptr;

    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        application = static_cast<Application*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(application));
        application->window_ = window;
    } else {
        application = reinterpret_cast<Application*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    }

    if (application != nullptr) {
        return application->HandleMessage(message, wParam, lParam);
    }

    return DefWindowProcW(window, message, wParam, lParam);
}

LRESULT Application::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == taskbarCreatedMessage_) {
        AddTrayIcon();
        return 0;
    }

    switch (message) {
        case kTrayCallbackMessage:
            if (LOWORD(lParam) == WM_RBUTTONUP || LOWORD(lParam) == WM_CONTEXTMENU) {
                ShowTrayMenu();
            } else if (LOWORD(lParam) == WM_LBUTTONDBLCLK) {
                SetSkin(
                    skin_ == SkinType::NeonSpectrumLine
                        ? SkinType::PulseBars
                        : SkinType::NeonSpectrumLine);
            }
            return 0;

        case kAudioActivityMessage:
            lastActivityTick_ = GetTickCount64();
            StartAnimation();
            return 0;

        case WM_TIMER:
            if (wParam == kRenderTimerId) {
                UpdateSpectrum();
            }
            return 0;

        case WM_NCHITTEST:
            if (adjustingPosition_) {
                return HTCAPTION;
            }
            return HTTRANSPARENT;

        case WM_EXITSIZEMOVE:
        case WM_DISPLAYCHANGE:
            Render();
            return 0;

        case WM_DESTROY:
            KillTimer(window_, kRenderTimerId);
            animationRunning_ = false;
            audio_.Stop();
            RemoveTrayIcon();
            window_ = nullptr;
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProcW(window_, message, wParam, lParam);
    }
}

}  // namespace wavebar