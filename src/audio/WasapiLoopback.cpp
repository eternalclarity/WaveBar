#include "audio/WasapiLoopback.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <audioclient.h>
#include <avrt.h>
#include <mmdeviceapi.h>
#include <objbase.h>

namespace wavebar {
namespace {

template <typename T>
void SafeRelease(T*& value) {
    if (value != nullptr) {
        value->Release();
        value = nullptr;
    }
}

bool IsFloatFormat(const WAVEFORMATEX& format) {
    if (format.wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        return true;
    }

    if (format.wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
        format.cbSize >= 22) {
        const auto& extensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE&>(format);
        return extensible.SubFormat.Data1 == WAVE_FORMAT_IEEE_FLOAT;
    }

    return false;
}

bool IsPcmFormat(const WAVEFORMATEX& format) {
    if (format.wFormatTag == WAVE_FORMAT_PCM) {
        return true;
    }

    if (format.wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
        format.cbSize >= 22) {
        const auto& extensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE&>(format);
        return extensible.SubFormat.Data1 == WAVE_FORMAT_PCM;
    }

    return false;
}

}  // namespace

WasapiLoopback::~WasapiLoopback() {
    Stop();
}

bool WasapiLoopback::Start(HWND notificationWindow, UINT activityMessage) {
    if (running_.exchange(true)) {
        return true;
    }

    notificationWindow_ = notificationWindow;
    activityMessage_ = activityMessage;
    stopEvent_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);

    if (stopEvent_ == nullptr) {
        Stop();
        return false;
    }

    thread_ = std::thread(&WasapiLoopback::CaptureThread, this);
    return true;
}

void WasapiLoopback::Stop() {
    if (!running_.exchange(false)) {
        return;
    }

    if (stopEvent_ != nullptr) {
        SetEvent(stopEvent_);
    }

    if (thread_.joinable()) {
        thread_.join();
    }

    if (stopEvent_ != nullptr) {
        CloseHandle(stopEvent_);
        stopEvent_ = nullptr;
    }
}

bool WasapiLoopback::CopyLatestSamples(
    std::array<float, kAnalysisSampleCount>& destination,
    std::uint32_t& sampleRate,
    float& rms) const {
    destination.fill(0.0f);

    {
        std::lock_guard lock(bufferMutex_);
        const std::size_t copyCount = std::min(availableSamples_, destination.size());
        const std::size_t destinationOffset = destination.size() - copyCount;
        const std::size_t start =
            (writePosition_ + kRingBufferSize - copyCount) % kRingBufferSize;

        for (std::size_t index = 0; index < copyCount; ++index) {
            destination[destinationOffset + index] =
                ringBuffer_[(start + index) % kRingBufferSize];
        }

        if (copyCount == 0) {
            sampleRate = sampleRate_.load(std::memory_order_relaxed);
            rms = rms_.load(std::memory_order_relaxed);
            return false;
        }
    }

    sampleRate = sampleRate_.load(std::memory_order_relaxed);
    rms = rms_.load(std::memory_order_relaxed);
    return true;
}

void WasapiLoopback::CaptureThread() {
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(comResult)) {
        running_.store(false);
        return;
    }

    DWORD taskIndex = 0;
    HANDLE multimediaTask = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);

    while (running_.load(std::memory_order_relaxed)) {
        if (!CaptureSession() && WaitForSingleObject(stopEvent_, 750) == WAIT_OBJECT_0) {
            break;
        }
    }

    if (multimediaTask != nullptr) {
        AvRevertMmThreadCharacteristics(multimediaTask);
    }

    CoUninitialize();
}

bool WasapiLoopback::CaptureSession() {
    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* device = nullptr;
    IAudioClient* audioClient = nullptr;
    IAudioCaptureClient* captureClient = nullptr;
    WAVEFORMATEX* format = nullptr;
    bool sessionStarted = false;

    HRESULT result = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        IID_PPV_ARGS(&enumerator));

    if (SUCCEEDED(result)) {
        result = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    }
    if (SUCCEEDED(result)) {
        result = device->Activate(
            __uuidof(IAudioClient),
            CLSCTX_ALL,
            nullptr,
            reinterpret_cast<void**>(&audioClient));
    }
    if (SUCCEEDED(result)) {
        result = audioClient->GetMixFormat(&format);
    }
    if (SUCCEEDED(result)) {
        result = audioClient->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_LOOPBACK,
            0,
            0,
            format,
            nullptr);
    }
    if (SUCCEEDED(result)) {
        result = audioClient->GetService(IID_PPV_ARGS(&captureClient));
    }
    if (SUCCEEDED(result)) {
        result = audioClient->Start();
        sessionStarted = SUCCEEDED(result);
    }

    if (SUCCEEDED(result) && format != nullptr) {
        sampleRate_.store(format->nSamplesPerSec, std::memory_order_relaxed);

        bool captureActive = true;

        while (captureActive && running_.load(std::memory_order_relaxed)) {
            if (WaitForSingleObject(stopEvent_, 10) == WAIT_OBJECT_0) {
                break;
            }

            UINT32 packetFrames = 0;
            result = captureClient->GetNextPacketSize(&packetFrames);

            while (SUCCEEDED(result) && packetFrames > 0) {
                BYTE* data = nullptr;
                UINT32 frames = 0;
                DWORD flags = 0;

                result = captureClient->GetBuffer(
                    &data,
                    &frames,
                    &flags,
                    nullptr,
                    nullptr);

                if (FAILED(result)) {
                    captureActive = false;
                    break;
                }

                WriteSamples(
                    data,
                    frames,
                    *format,
                    (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0);

                captureClient->ReleaseBuffer(frames);
                result = captureClient->GetNextPacketSize(&packetFrames);
            }

            if (FAILED(result)) {
                captureActive = false;
            }
        }
    }

    if (sessionStarted) {
        audioClient->Stop();
    }

    if (format != nullptr) {
        CoTaskMemFree(format);
    }
    SafeRelease(captureClient);
    SafeRelease(audioClient);
    SafeRelease(device);
    SafeRelease(enumerator);

    return SUCCEEDED(result);
}

float WasapiLoopback::ReadChannelSample(
    const BYTE* frame,
    const WAVEFORMATEX& format,
    UINT32 channel) const {
    if (format.nChannels == 0) {
        return 0.0f;
    }

    const UINT32 bytesPerChannel = format.nBlockAlign / format.nChannels;
    const BYTE* sample = frame + channel * bytesPerChannel;

    if (IsFloatFormat(format)) {
        if (format.wBitsPerSample == 32) {
            float value = 0.0f;
            std::memcpy(&value, sample, sizeof(value));
            return std::clamp(value, -1.0f, 1.0f);
        }
        if (format.wBitsPerSample == 64) {
            double value = 0.0;
            std::memcpy(&value, sample, sizeof(value));
            return static_cast<float>(std::clamp(value, -1.0, 1.0));
        }
    }

    if (IsPcmFormat(format)) {
        if (format.wBitsPerSample == 16) {
            std::int16_t value = 0;
            std::memcpy(&value, sample, sizeof(value));
            return static_cast<float>(value) / 32768.0f;
        }
        if (format.wBitsPerSample == 24) {
            std::int32_t value =
                static_cast<std::int32_t>(sample[0]) |
                (static_cast<std::int32_t>(sample[1]) << 8) |
                (static_cast<std::int32_t>(sample[2]) << 16);
            if ((value & 0x00800000) != 0) {
                value |= static_cast<std::int32_t>(0xFF000000);
            }
            return static_cast<float>(value) / 8388608.0f;
        }
        if (format.wBitsPerSample == 32) {
            std::int32_t value = 0;
            std::memcpy(&value, sample, sizeof(value));
            return static_cast<float>(static_cast<double>(value) / 2147483648.0);
        }
    }

    return 0.0f;
}

void WasapiLoopback::WriteSamples(
    const BYTE* data,
    UINT32 frames,
    const WAVEFORMATEX& format,
    bool silent) {
    if (frames == 0 || format.nChannels == 0) {
        return;
    }

    double sumOfSquares = 0.0;

    std::lock_guard lock(bufferMutex_);
    for (UINT32 frameIndex = 0; frameIndex < frames; ++frameIndex) {
        float mono = 0.0f;

        if (!silent && data != nullptr) {
            const BYTE* frame = data + static_cast<std::size_t>(frameIndex) * format.nBlockAlign;
            for (UINT32 channel = 0; channel < format.nChannels; ++channel) {
                mono += ReadChannelSample(frame, format, channel);
            }
            mono /= static_cast<float>(format.nChannels);
        }

        mono = std::clamp(mono, -1.0f, 1.0f);
        sumOfSquares += static_cast<double>(mono) * mono;
        ringBuffer_[writePosition_] = mono;
        writePosition_ = (writePosition_ + 1) % kRingBufferSize;
        availableSamples_ = std::min(availableSamples_ + 1, kRingBufferSize);
    }

    const float blockRms = static_cast<float>(std::sqrt(sumOfSquares / frames));

    const float previousRms = rms_.load(std::memory_order_relaxed);
    rms_.store(previousRms + 0.25f * (blockRms - previousRms), std::memory_order_relaxed);
    UpdateActivity(blockRms, frames, format.nSamplesPerSec);
}

void WasapiLoopback::UpdateActivity(float blockRms, UINT32 frames, UINT32 sampleRate) {
    constexpr float kActivityThreshold = 0.0008f;

    if (blockRms > kActivityThreshold) {
        silentFrameCount_ = 0;
        if (!activitySignaled_) {
            activitySignaled_ = true;
            if (notificationWindow_ != nullptr && activityMessage_ != 0) {
                PostMessageW(notificationWindow_, activityMessage_, 0, 0);
            }
        }
        return;
    }

    silentFrameCount_ += frames;
    if (silentFrameCount_ > static_cast<std::uint64_t>(sampleRate) * 3 / 10) {
        activitySignaled_ = false;
    }
}

}  // namespace wavebar