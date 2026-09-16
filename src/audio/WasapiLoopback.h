#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <thread>

#include <windows.h>
#include <mmreg.h>

namespace wavebar {

class WasapiLoopback {
public:
    static constexpr std::size_t kAnalysisSampleCount = 8192;

    WasapiLoopback() = default;
    ~WasapiLoopback();

    WasapiLoopback(const WasapiLoopback&) = delete;
    WasapiLoopback& operator=(const WasapiLoopback&) = delete;

    bool Start(HWND notificationWindow, UINT activityMessage);
    void Stop();
    // Called on the window thread; rebuilding WASAPI stays on the capture thread.
    void RequestRestart();

    bool CopyLatestSamples(
        std::array<float, kAnalysisSampleCount>& destination,
        std::uint32_t& sampleRate,
        float& rms) const;

private:
    static constexpr std::size_t kRingBufferSize = 32768;

    void CaptureThread();
    bool CaptureSession();
    void ResetSamples();
    void WriteSamples(const BYTE* data, UINT32 frames, const WAVEFORMATEX& format, bool silent);
    float ReadChannelSample(const BYTE* frame, const WAVEFORMATEX& format, UINT32 channel) const;
    void UpdateActivity(float blockRms, UINT32 frames, UINT32 sampleRate);

    mutable std::mutex bufferMutex_;
    std::array<float, kRingBufferSize> ringBuffer_{};
    std::size_t writePosition_ = 0;
    std::size_t availableSamples_ = 0;
    std::atomic<std::uint32_t> sampleRate_{48000};
    std::atomic<float> rms_{0.0f};
    std::atomic<bool> running_{false};
    std::thread thread_;
    HANDLE stopEvent_ = nullptr;
    HANDLE restartEvent_ = nullptr;
    HWND notificationWindow_ = nullptr;
    UINT activityMessage_ = 0;
    bool activitySignaled_ = false;
    std::uint64_t silentFrameCount_ = 0;
};

}  // namespace wavebar
