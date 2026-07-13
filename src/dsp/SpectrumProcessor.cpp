#include "dsp/SpectrumProcessor.h"

#include <algorithm>
#include <cmath>

namespace wavebar {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kMinimumFrequency = 15.0f;
constexpr float kMaximumFrequency = 10000.0f;

}  // namespace

SpectrumProcessor::SpectrumProcessor() {
    for (std::size_t index = 0; index < kFftSize; ++index) {
        window_[index] =
            0.5f -
            0.5f * std::cos(
                2.0f * kPi * static_cast<float>(index) /
                static_cast<float>(kFftSize - 1));
    }

    const std::size_t center = SpectrumFrame::kLineCount / 2;
    for (std::size_t index = center - 7; index <= center + 7; ++index) {
        waveA_[index] = -3.0f;
    }
}

SpectrumFrame SpectrumProcessor::Process(
    const std::array<float, WasapiLoopback::kAnalysisSampleCount>& samples,
    std::uint32_t sampleRate,
    float rms) {
    SpectrumFrame frame{};
    frame.rms = rms;
    frame.active = rms > 0.0008f;

    for (std::size_t index = 0; index < kFftSize; ++index) {
        fft_[index] = std::complex<float>(samples[index] * window_[index], 0.0f);
    }
    Transform();

    std::array<float, SpectrumFrame::kLineCount> bands{};
    constexpr float kSensitivity = 45.0f;
    constexpr std::size_t kRainmeterBandCount = SpectrumFrame::kLineCount + 1;
    const float frequencyRatio = kMaximumFrequency / kMinimumFrequency;

    for (std::size_t band = 0; band < bands.size(); ++band) {
        const float lowPosition = (static_cast<float>(band) + 0.5f) / kRainmeterBandCount;
        const float highPosition = (static_cast<float>(band) + 1.5f) / kRainmeterBandCount;
        const float lowFrequency = kMinimumFrequency * std::pow(frequencyRatio, lowPosition);
        const float highFrequency = kMinimumFrequency * std::pow(frequencyRatio, highPosition);
        const float power = BucketPower(lowFrequency, highFrequency, sampleRate);
        float value = 1.0f + (10.0f / kSensitivity) *
            std::log10(std::max(power, 0.000000000001f));
        value = std::clamp(value, 0.0f, 1.0f);
        bands[band] = rms < 0.00025f ? 0.0f : value;
    }

    constexpr float kDamping = 0.75f;
    constexpr float kWaveScale = 2.2f;

    for (int simulationStep = 0; simulationStep < 2; ++simulationStep) {
        auto& source = useWaveAAsSource_ ? waveA_ : waveB_;
        auto& destination = useWaveAAsSource_ ? waveB_ : waveA_;
        for (std::size_t index = 0; index < destination.size(); ++index) {
            const std::size_t left = index == 0 ? index : index - 1;
            const std::size_t right = index + 1 == source.size() ? index : index + 1;
            destination[index] += std::pow(bands[index], 1.8f);
            destination[index] =
                (source[left] + source[right]) - destination[index];
            destination[index] *= kDamping;

            frame.line[index] = std::clamp(
                destination[index] / kWaveScale,
                -1.25f,
                1.25f);
        }
        useWaveAAsSource_ = !useWaveAAsSource_;
    }
    for (float value : frame.line) {
        frame.maximum = std::max(frame.maximum, std::abs(value));
    }

    for (std::size_t bar = 0; bar < frame.bars.size(); ++bar) {
        const std::size_t begin = bar * bands.size() / frame.bars.size();
        const std::size_t end = (bar + 1) * bands.size() / frame.bars.size();
        float peak = 0.0f;
        float average = 0.0f;
        for (std::size_t index = begin; index < end; ++index) {
            peak = std::max(peak, bands[index]);
            average += bands[index];
        }
        average /= static_cast<float>(std::max<std::size_t>(end - begin, 1));
        const float target = peak * 0.55f + average * 0.45f;
        const float coefficient = target > barSmoothed_[bar] ? 0.56f : 0.13f;
        barSmoothed_[bar] += coefficient * (target - barSmoothed_[bar]);
        frame.bars[bar] = std::clamp(barSmoothed_[bar], 0.0f, 1.0f);
    }

    return frame;
}

void SpectrumProcessor::Transform() {
    for (std::size_t index = 1, reversed = 0; index < kFftSize; ++index) {
        std::size_t bit = kFftSize >> 1;
        for (; (reversed & bit) != 0; bit >>= 1) {
            reversed ^= bit;
        }
        reversed ^= bit;

        if (index < reversed) {
            std::swap(fft_[index], fft_[reversed]);
        }
    }

    for (std::size_t length = 2; length <= kFftSize; length <<= 1) {
        const float angle = -2.0f * kPi / static_cast<float>(length);
        const std::complex<float> step(std::cos(angle), std::sin(angle));

        for (std::size_t offset = 0; offset < kFftSize; offset += length) {
            std::complex<float> rotation(1.0f, 0.0f);
            for (std::size_t index = 0; index < length / 2; ++index) {
                const std::complex<float> even = fft_[offset + index];
                const std::complex<float> odd =
                    fft_[offset + index + length / 2] * rotation;
                fft_[offset + index] = even + odd;
                fft_[offset + index + length / 2] = even - odd;
                rotation *= step;
            }
        }
    }
}

float SpectrumProcessor::BucketPower(
    float lowFrequency,
    float highFrequency,
    std::uint32_t sampleRate) const {
    if (sampleRate == 0) {
        return 0.0f;
    }

    const float binWidth = static_cast<float>(sampleRate) / kFftSize;
    const std::size_t lowestBin = std::clamp(
        static_cast<std::size_t>(std::floor(lowFrequency / binWidth + 0.5f)),
        std::size_t{1},
        kFftSize / 2 - 1);
    const std::size_t highestBin = std::clamp(
        static_cast<std::size_t>(std::ceil(highFrequency / binWidth + 0.5f)),
        lowestBin,
        kFftSize / 2 - 1);

    float integratedPower = 0.0f;
    const float fftScalar = 1.0f / std::sqrt(static_cast<float>(kFftSize));
    const float bandScalar = 2.0f / static_cast<float>(sampleRate);
    for (std::size_t bin = lowestBin; bin <= highestBin; ++bin) {
        const float cellLow = (static_cast<float>(bin) - 0.5f) * binWidth;
        const float cellHigh = (static_cast<float>(bin) + 0.5f) * binWidth;
        const float overlap = std::max(
            0.0f,
            std::min(highFrequency, cellHigh) - std::max(lowFrequency, cellLow));
        integratedPower +=
            overlap * std::norm(fft_[bin]) * fftScalar * bandScalar;
    }
    return integratedPower;
}

}  // namespace wavebar