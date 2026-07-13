#pragma once

#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>

#include "audio/WasapiLoopback.h"

namespace wavebar {

struct SpectrumFrame {
    static constexpr std::size_t kLineCount = 256;
    static constexpr std::size_t kBarCount = 48;

    std::array<float, kLineCount> line{};
    std::array<float, kBarCount> bars{};
    float rms = 0.0f;
    float maximum = 0.0f;
    bool active = false;
};

class SpectrumProcessor {
public:
    SpectrumProcessor();

    SpectrumFrame Process(
        const std::array<float, WasapiLoopback::kAnalysisSampleCount>& samples,
        std::uint32_t sampleRate,
        float rms);

private:
    static constexpr std::size_t kFftSize = WasapiLoopback::kAnalysisSampleCount;

    void Transform();
    float BucketPower(float lowFrequency, float highFrequency, std::uint32_t sampleRate) const;

    std::array<float, kFftSize> window_{};
    std::array<std::complex<float>, kFftSize> fft_{};
    std::array<float, SpectrumFrame::kLineCount> waveA_{};
    std::array<float, SpectrumFrame::kLineCount> waveB_{};
    bool useWaveAAsSource_ = true;
    std::array<float, SpectrumFrame::kBarCount> barSmoothed_{};
};

}  // namespace wavebar