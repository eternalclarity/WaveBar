#pragma once

#include <array>
#include <cstdint>
#include <windows.h>

#include "dsp/SpectrumProcessor.h"

namespace wavebar {

enum class SkinType {
    NeonSpectrumLine,
    PulseBars,
};

class LayeredRenderer {
public:
    LayeredRenderer() = default;
    ~LayeredRenderer();

    LayeredRenderer(const LayeredRenderer&) = delete;
    LayeredRenderer& operator=(const LayeredRenderer&) = delete;

    bool Initialize();
    bool Render(HWND window, SkinType skin, const SpectrumFrame& spectrum);

private:
    struct Color {
        float red;
        float green;
        float blue;
    };

    bool EnsureSurface(int width, int height);
    void DestroySurface();
    void Clear();
    void DrawNeonSpectrumLine(int width, int height, const std::array<float, SpectrumFrame::kLineCount>& values);
    void DrawPulseBars(int width, int height, const std::array<float, SpectrumFrame::kBarCount>& values);
    void DrawSegment(float x1, float y1, float x2, float y2, float radius, float opacity);
    void BlendPixel(int x, int y, const Color& color, float opacity);
    Color GradientColor(float x) const;

    HDC memoryDc_ = nullptr;
    HBITMAP bitmap_ = nullptr;
    HGDIOBJ previousBitmap_ = nullptr;
    std::uint32_t* pixels_ = nullptr;
    int width_ = 0;
    int height_ = 0;
};

}  // namespace wavebar