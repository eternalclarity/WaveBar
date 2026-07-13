#include <windows.h>
#include <objbase.h>

#include "app/Application.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    HANDLE singleInstance = CreateMutexW(nullptr, TRUE, L"Local\\WaveBar.SingleInstance");
    if (singleInstance == nullptr) {
        return 1;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(singleInstance);
        return 0;
    }

    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(comResult)) {
        CloseHandle(singleInstance);
        return 1;
    }

    wavebar::Application application;
    const int result = application.Initialize(instance) ? application.Run() : 1;

    CoUninitialize();
    CloseHandle(singleInstance);
    return result;
}