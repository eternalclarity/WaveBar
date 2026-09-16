// Exercise the real capture worker with fault-injected COM devices. No change
// to system devices, audio services, volume, or power state is required.
#include <windows.h>
#include <audioclient.h>
#include <avrt.h>
#include <mmdeviceapi.h>
#include <objbase.h>
#include <shellapi.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
std::atomic<int> endpoint{1}; // Zero means the audio service/device is not ready.
std::atomic<int> activations{0};
std::atomic<int> queries{0};
std::atomic<int> liveObjects{0};
std::atomic<bool> packets{true};
std::atomic<bool> failRelease{false};
std::atomic<bool> failCom{false};

template<class Interface>
class FakeCom : public Interface {
public:
    FakeCom() { ++liveObjects; }
    virtual ~FakeCom() { --liveObjects; }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void** value) override {
        *value = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++references_; }
    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG left = --references_;
        if (left == 0) delete this;
        return left;
    }
private:
    std::atomic<ULONG> references_{1};
};

class Capture : public FakeCom<IAudioCaptureClient> {
public:
    explicit Capture(int id) : sample_(id == 1 ? 0.25f : 0.5f) {}
    HRESULT STDMETHODCALLTYPE GetBuffer(BYTE** data, UINT32* frames, DWORD* flags,
                                        UINT64*, UINT64*) override {
        *data = reinterpret_cast<BYTE*>(&sample_);
        *frames = 1;
        *flags = 0;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE ReleaseBuffer(UINT32) override {
        return failRelease.exchange(false) ? AUDCLNT_E_DEVICE_INVALIDATED : S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetNextPacketSize(UINT32* frames) override {
        // One packet per worker iteration, followed by an empty queue.
        ready_ = !ready_;
        *frames = packets && ready_ ? 1 : 0;
        return S_OK;
    }
private:
    float sample_;
    bool ready_ = false;
};

class Client : public FakeCom<IAudioClient> {
public:
    explicit Client(int id) : id_(id) {}
    HRESULT STDMETHODCALLTYPE Initialize(AUDCLNT_SHAREMODE, DWORD, REFERENCE_TIME,
                                         REFERENCE_TIME, const WAVEFORMATEX*, LPCGUID) override {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetMixFormat(WAVEFORMATEX** value) override {
        *value = static_cast<WAVEFORMATEX*>(CoTaskMemAlloc(sizeof(WAVEFORMATEX)));
        if (*value == nullptr) return E_OUTOFMEMORY;
        **value = {WAVE_FORMAT_IEEE_FLOAT, 1, 48000, 192000, 4, 32, 0};
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Start() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE Stop() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetService(REFIID, void** value) override {
        *value = static_cast<IAudioCaptureClient*>(new Capture(id_));
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetBufferSize(UINT32*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetStreamLatency(REFERENCE_TIME*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetCurrentPadding(UINT32*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE IsFormatSupported(AUDCLNT_SHAREMODE, const WAVEFORMATEX*,
                                                WAVEFORMATEX**) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetDevicePeriod(REFERENCE_TIME*, REFERENCE_TIME*) override {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE Reset() override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetEventHandle(HANDLE) override { return E_NOTIMPL; }
private:
    int id_;
};

class Device : public FakeCom<IMMDevice> {
public:
    explicit Device(int id) : id_(id) {}
    HRESULT STDMETHODCALLTYPE Activate(REFIID, DWORD, PROPVARIANT*, void** value) override {
        ++activations;
        *value = static_cast<IAudioClient*>(new Client(id_));
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetId(LPWSTR* value) override {
        *value = static_cast<LPWSTR>(CoTaskMemAlloc(2 * sizeof(wchar_t)));
        if (*value == nullptr) return E_OUTOFMEMORY;
        (*value)[0] = static_cast<wchar_t>(L'0' + id_);
        (*value)[1] = L'\0';
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetState(DWORD* state) override {
        *state = DEVICE_STATE_ACTIVE;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OpenPropertyStore(DWORD, IPropertyStore**) override {
        return E_NOTIMPL;
    }
private:
    int id_;
};

class Enumerator : public FakeCom<IMMDeviceEnumerator> {
public:
    HRESULT STDMETHODCALLTYPE GetDefaultAudioEndpoint(EDataFlow, ERole, IMMDevice** value) override {
        ++queries;
        const int id = endpoint.load();
        *value = id == 0 ? nullptr : new Device(id);
        return id == 0 ? E_NOTFOUND : S_OK;
    }
    HRESULT STDMETHODCALLTYPE EnumAudioEndpoints(EDataFlow, DWORD, IMMDeviceCollection**) override {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE GetDevice(LPCWSTR, IMMDevice**) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE RegisterEndpointNotificationCallback(IMMNotificationClient*) override {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE UnregisterEndpointNotificationCallback(IMMNotificationClient*) override {
        return E_NOTIMPL;
    }
};

HRESULT FakeCreateInstance(REFCLSID, IUnknown*, DWORD, REFIID, void** value) {
    *value = static_cast<IMMDeviceEnumerator*>(new Enumerator);
    return S_OK;
}
HRESULT FakeInitialize(LPVOID reserved, DWORD flags) {
    return failCom ? E_FAIL : CoInitializeEx(reserved, flags);
}
// Keep real event waits/threading but accelerate watchdog and endpoint polling.
ULONGLONG FastTick() { return GetTickCount64() * 10; }
}

#define CoCreateInstance FakeCreateInstance
#define CoInitializeEx FakeInitialize
#define GetTickCount64 FastTick
#include "audio/WasapiLoopback.cpp"
#undef GetTickCount64
#undef CoInitializeEx
#undef CoCreateInstance

namespace {
int trayAttempts = 0;
BOOL FakeNotifyIcon(DWORD message, PNOTIFYICONDATAW) {
    // Explorer's tray is unavailable for the first two startup attempts.
    if (message == NIM_ADD) return ++trayAttempts >= 3;
    return TRUE;
}
}
#define Shell_NotifyIconW FakeNotifyIcon
#include "app/Application.cpp"
#undef Shell_NotifyIconW

namespace {
void Check(bool ok, const char* label) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", label);
        std::exit(1);
    }
    std::printf("PASS: %s\n", label);
}
template<class Predicate>
bool Eventually(Predicate predicate, DWORD timeout = 2500) {
    const ULONGLONG start = GetTickCount64();
    while (GetTickCount64() - start < timeout) {
        if (predicate()) return true;
        Sleep(5);
    }
    return predicate();
}
bool HasSample(wavebar::WasapiLoopback& audio, float expected) {
    std::array<float, wavebar::WasapiLoopback::kAnalysisSampleCount> samples{};
    std::uint32_t rate = 0;
    float rms = 0;
    return audio.CopyLatestSamples(samples, rate, rms) &&
        samples.back() == expected && rate == 48000 && rms > 0;
}
bool Empty(wavebar::WasapiLoopback& audio) {
    std::array<float, wavebar::WasapiLoopback::kAnalysisSampleCount> samples{};
    std::uint32_t rate = 0;
    float rms = 0;
    const bool available = audio.CopyLatestSamples(samples, rate, rms);
    return !available && rms == 0 && samples.back() == 0;
}
void PumpMessages() {
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}
BOOL CALLBACK FindTestWindow(HWND window, LPARAM result) {
    wchar_t name[128]{};
    GetClassNameW(window, name, 128);
    if (std::wcscmp(name, L"WaveBarVisualizerWindow") == 0) {
        *reinterpret_cast<HWND*>(result) = window;
        return FALSE;
    }
    return TRUE;
}
}

int main() {
    wavebar::WasapiLoopback audio;
    endpoint = 0;
    Check(audio.Start(nullptr, 0), "start while device is unavailable");
    Check(Eventually([] { return queries >= 2; }), "startup failure retries automatically");
    endpoint = 1;
    Check(Eventually([&] { return HasSample(audio, 0.25f); }), "late device recovers without restart");

    endpoint = 2;
    Check(Eventually([&] { return HasSample(audio, 0.5f); }), "default device change follows new output");
    int before = activations;
    audio.RequestRestart();
    Check(Eventually([&] { return activations > before && HasSample(audio, 0.5f); }),
          "resume request rebuilds a working stream");

    packets = false;
    Check(Eventually([&] { return Empty(audio); }, 300), "no packets clears stale spectrum");
    before = activations;
    Check(Eventually([&] { return activations > before; }), "S_OK with no packets triggers recovery");
    packets = true;
    Check(Eventually([&] { return HasSample(audio, 0.5f); }), "audio returns after stalled session");

    before = activations;
    failRelease = true;
    Check(Eventually([&] { return activations > before && HasSample(audio, 0.5f); }),
          "ReleaseBuffer failure recreates session");
    endpoint = 0;
    Check(Eventually([&] { return Empty(audio); }), "device disappearance clears samples");
    endpoint = 1;
    Check(Eventually([&] { return HasSample(audio, 0.25f); }), "device reappearance recovers");

    audio.Stop();
    Check(liveObjects == 0 && Empty(audio), "stop releases COM objects and samples");
    Check(audio.Start(nullptr, 0), "same instance can start again");
    Check(Eventually([&] { return HasSample(audio, 0.25f); }), "restarted instance captures");
    audio.Stop();

    endpoint = 0;
    const int queryBefore = queries;
    audio.Start(nullptr, 0);
    Check(Eventually([&] { return queries > queryBefore; }), "worker enters retry wait");
    const ULONGLONG stopStart = GetTickCount64();
    audio.Stop();
    Check(GetTickCount64() - stopStart < 500, "stop interrupts retry wait");

    failCom = true;
    audio.Start(nullptr, 0);
    Sleep(100);
    audio.Stop();
    Check(liveObjects == 0, "COM initialization failure can be stopped safely");
    failCom = false;
    endpoint = 1;
    audio.Start(nullptr, 0);
    Check(Eventually([&] { return HasSample(audio, 0.25f); }), "worker recovers after failed start");
    audio.Stop();
    Check(liveObjects == 0, "all simulated COM resources released");

    {
        wavebar::Application app;
        Check(app.Initialize(GetModuleHandleW(nullptr)), "missing startup tray does not abort application");
        HWND window = nullptr;
        EnumThreadWindows(GetCurrentThreadId(), FindTestWindow, reinterpret_cast<LPARAM>(&window));
        Check(window != nullptr, "application window created");
        ShowWindow(window, SW_HIDE);
        Check(Eventually([] { PumpMessages(); return trayAttempts >= 3; }, 3500),
              "tray installation retries until Explorer is ready");
        const ULONGLONG trayStart = GetTickCount64();
        Check(Eventually([&] { PumpMessages(); return GetTickCount64() - trayStart >= 1200; }),
              "process messages after tray recovery");
        Check(trayAttempts == 3, "tray retries stop after success");
        for (const WPARAM resume : {PBT_APMRESUMEAUTOMATIC, PBT_APMRESUMESUSPEND,
                                    PBT_APMRESUMECRITICAL}) {
            before = activations;
            Check(SendMessageW(window, WM_POWERBROADCAST, resume, 0) == TRUE,
                  "application accepts resume notification");
            Check(Eventually([&] { return activations > before; }),
                  "resume window message reaches capture recovery");
        }
    }
    Check(liveObjects == 0, "application shutdown releases audio resources");
}
