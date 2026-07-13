#include "render/LayeredRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace wavebar {
namespace {

float Lerp(float left, float right, float amount) {
    return left + (right - left) * amount;
}

}  // namespace

LayeredRenderer::~LayeredRenderer() {
    DestroySurface();
}

bool LayeredRenderer::Initialize() {
    return true;
}

bool LayeredRenderer::EnsureSurface(int width, int height) {
    if (width == width_ && height == height_ && pixels_ != nullptr) {
        return true;
    }

    DestroySurface();

    HDC screenDc = GetDC(nullptr);
    if (screenDc == nullptr) {
        return false;
    }

    memoryDc_ = CreateCompatibleDC(screenDc);
    ReleaseDC(nullptr, screenDc);
    if (memoryDc_ == nullptr) {
        return false;
    }

    BITMAPINFO bitmapInfo{};
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = width;
    bitmapInfo.bmiHeader.biHeight = -height;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    void* rawPixels = nullptr;
    bitmap_ = CreateDIBSection(memoryDc_, &bitmapInfo, DIB_RGB_COLORS, &rawPixels, nullptr, 0);
    if (bitmap_ == nullptr || rawPixels == nullptr) {
        DestroySurface();
        return false;
    }

    previousBitmap_ = SelectObject(memoryDc_, bitmap_);
    pixels_ = static_cast<std::uint32_t*>(rawPixels);
    width_ = width;
    height_ = height;
    return true;
}

void LayeredRenderer::DestroySurface() {
    if (memoryDc_ != nullptr && previousBitmap_ != nullptr) {
        SelectObject(memoryDc_, previousBitmap_);
    }
    previousBitmap_ = nullptr;

    if (bitmap_ != nullptr) {
        DeleteObject(bitmap_);
        bitmap_ = nullptr;
    }

    if (memoryDc_ != nullptr) {
        DeleteDC(memoryDc_);
        memoryDc_ = nullptr;
    }

    pixels_ = nullptr;
    width_ = 0;
    height_ = 0;
}

void LayeredRenderer::Clear() {
    std::memset(pixels_, 0, static_cast<std::size_t>(width_) * height_ * sizeof(std::uint32_t));
}

LayeredRenderer::Color LayeredRenderer::GradientColor(float x) const {
    const float normalized = std::clamp(x / static_cast<float>(std::max(width_, 1)), 0.0f, 1.0f);

    constexpr Color magenta{214.0f, 44.0f, 255.0f};
    constexpr Color violet{149.0f, 109.0f, 255.0f};
    constexpr Color blue{99.0f, 135.0f, 255.0f};
    constexpr Color cyan{38.0f, 231.0f, 225.0f};

    const auto mix = [](const Color& from, const Color& to, float amount) {
        return Color{
            Lerp(from.red, to.red, amount),
            Lerp(from.green, to.green, amount),
            Lerp(from.blue, to.blue, amount),
        };
    };

    if (normalized < 0.34f) {
        return mix(magenta, violet, normalized / 0.34f);
    }
    if (normalized < 0.62f) {
        return mix(violet, blue, (normalized - 0.34f) / 0.28f);
    }
    return mix(blue, cyan, (normalized - 0.62f) / 0.38f);
}

void LayeredRenderer::BlendPixel(int x, int y, const Color& color, float opacity) {
    if (x < 0 || y < 0 || x >= width_ || y >= height_ || opacity <= 0.0f) {
        return;
    }

    const int sourceAlpha = std::clamp(static_cast<int>(opacity * 255.0f + 0.5f), 0, 255);
    if (sourceAlpha == 0) {
        return;
    }

    std::uint32_t& destination = pixels_[static_cast<std::size_t>(y) * width_ + x];

    const int destinationBlue = destination & 0xFF;
    const int destinationGreen = (destination >> 8) & 0xFF;
    const int destinationRed = (destination >> 16) & 0xFF;
    const int destinationAlpha = (destination >> 24) & 0xFF;
    const int inverseAlpha = 255 - sourceAlpha;

    const int sourceBlue = static_cast<int>(color.blue * sourceAlpha / 255.0f);
    const int sourceGreen = static_cast<int>(color.green * sourceAlpha / 255.0f);
    const int sourceRed = static_cast<int>(color.red * sourceAlpha / 255.0f);

    const int outputBlue = sourceBlue + destinationBlue * inverseAlpha / 255;
    const int outputGreen = sourceGreen + destinationGreen * inverseAlpha / 255;
    const int outputRed = sourceRed + destinationRed * inverseAlpha / 255;
    const int outputAlpha = sourceAlpha + destinationAlpha * inverseAlpha / 255;

    destination =
        static_cast<std::uint32_t>(outputBlue) |
        (static_cast<std::uint32_t>(outputGreen) << 8) |
        (static_cast<std::uint32_t>(outputRed) << 16) |
        (static_cast<std::uint32_t>(outputAlpha) << 24);
}

void LayeredRenderer::DrawSegment(
    float x1,
    float y1,
    float x2,
    float y2,
    float radius,
    float opacity) {
    const float minimumX = std::min(x1, x2) - radius - 1.0f;
    const float maximumX = std::max(x1, x2) + radius + 1.0f;
    const float minimumY = std::min(y1, y2) - radius - 1.0f;
    const float maximumY = std::max(y1, y2) + radius + 1.0f;
    const float deltaX = x2 - x1;
    const float deltaY = y2 - y1;
    const float lengthSquared = deltaX * deltaX + deltaY * deltaY;

    for (int y = static_cast<int>(std::floor(minimumY)); y <= static_cast<int>(std::ceil(maximumY)); ++y) {
        for (int x = static_cast<int>(std::floor(minimumX)); x <= static_cast<int>(std::ceil(maximumX)); ++x) {
            float position = 0.0f;
            if (lengthSquared > 0.0001f) {
                position = ((x + 0.5f - x1) * deltaX + (y + 0.5f - y1) * deltaY) / lengthSquared;
                position = std::clamp(position, 0.0f, 1.0f);
            }

            const float closestX = x1 + position * deltaX;
            const float closestY = y1 + position * deltaY;
            const float distanceX = x + 0.5f - closestX;
            const float distanceY = y + 0.5f - closestY;
            const float distance = std::sqrt(distanceX * distanceX + distanceY * distanceY);
            const float coverage = std::clamp(radius + 0.5f - distance, 0.0f, 1.0f);

            if (coverage > 0.0f) {
                BlendPixel(x, y, GradientColor(x + 0.5f), opacity * coverage);
            }
        }
    }
}

void LayeredRenderer::DrawNeonSpectrumLine(
    int width,
    int height,
    const std::array<float, SpectrumFrame::kLineCount>& values) {
    constexpr int kCurveSteps = 2;
    const float scaleX = static_cast<float>(width) / 1200.0f;
    const float scaleY = static_cast<float>(height) / 180.0f;
    const float startX = 80.0f * scaleX;
    const float endX = 1120.0f * scaleX;
    const float baseline = 105.0f * scaleY;
    const float amplitude = 72.0f * scaleY;

    const auto pointX = [&](std::size_t index) {
        return startX +
            (endX - startX) * static_cast<float>(index) /
                static_cast<float>(values.size() - 1);
    };
    const auto pointY = [&](float value) {
        return baseline + std::clamp(value, -1.0f, 1.0f) * amplitude;
    };

    const auto drawLayer = [&](float radius, float opacity) {
        float previousX = pointX(0);
        float previousY = pointY(values[0]);

        for (std::size_t index = 0; index + 1 < values.size(); ++index) {
            const float controlX = pointX(index);
            const float controlY = pointY(values[index]);
            const float endPointX = (pointX(index) + pointX(index + 1)) * 0.5f;
            const float endPointY = (pointY(values[index]) + pointY(values[index + 1])) * 0.5f;

            for (int step = 1; step <= kCurveSteps; ++step) {
                const float t = static_cast<float>(step) / kCurveSteps;
                const float inverse = 1.0f - t;
                const float x =
                    inverse * inverse * previousX +
                    2.0f * inverse * t * controlX +
                    t * t * endPointX;
                const float y =
                    inverse * inverse * previousY +
                    2.0f * inverse * t * controlY +
                    t * t * endPointY;
                DrawSegment(previousX, previousY, x, y, radius, opacity);
                previousX = x;
                previousY = y;
            }
        }

        DrawSegment(
            previousX,
            previousY,
            pointX(values.size() - 1),
            pointY(values.back()),
            radius,
            opacity);
    };

    drawLayer(4.2f, 0.040f);
    drawLayer(1.6f, 0.16f);
    drawLayer(0.64f, 0.98f);
    DrawSegment(startX, baseline, endX, baseline, 0.42f, 0.12f);
}

void LayeredRenderer::DrawPulseBars(
    int width,
    int height,
    const std::array<float, SpectrumFrame::kBarCount>& values) {
    const float scaleX = static_cast<float>(width) / 1200.0f;
    const float scaleY = static_cast<float>(height) / 180.0f;
    const float startX = 112.0f * scaleX;
    const float endX = 1048.0f * scaleX;
    const float barRadius = std::max(2.0f, 4.0f * scaleX);
    const float baseline = 154.0f * scaleY;
    const float maximumHeight = 108.0f * scaleY;
    const float minimumHeight = 2.0f * scaleY;

    const auto drawLayer = [&](float extraRadius, float opacity) {
        for (std::size_t index = 0; index < values.size(); ++index) {
            const float x =
                startX +
                (endX - startX) *
                    static_cast<float>(index) /
                    static_cast<float>(values.size() - 1);
            const float shaped = std::pow(std::clamp(values[index], 0.0f, 1.0f), 0.82f);
            const float barHeight = minimumHeight + shaped * maximumHeight;
            const float radius = barRadius + extraRadius;
            DrawSegment(
                x,
                baseline - barHeight + barRadius,
                x,
                baseline - barRadius,
                radius,
                opacity);
        }
    };

    drawLayer(3.5f, 0.07f);
    drawLayer(1.2f, 0.18f);
    drawLayer(0.0f, 0.92f);
    DrawSegment(108.0f * scaleX, baseline, 1056.0f * scaleX, baseline, 0.55f, 0.24f);
}

bool LayeredRenderer::Render(HWND window, SkinType skin, const SpectrumFrame& spectrum) {
    RECT client{};
    if (!GetClientRect(window, &client)) {
        return false;
    }

    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    if (width <= 0 || height <= 0 || !EnsureSurface(width, height)) {
        return false;
    }

    Clear();

    if (skin == SkinType::NeonSpectrumLine) {
        DrawNeonSpectrumLine(width, height, spectrum.line);
    } else {
        DrawPulseBars(width, height, spectrum.bars);
    }

    RECT windowRect{};
    GetWindowRect(window, &windowRect);

    POINT destination{windowRect.left, windowRect.top};
    POINT source{0, 0};
    SIZE size{width, height};
    BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};

    GdiFlush();
    return UpdateLayeredWindow(
               window,
               nullptr,
               &destination,
               &size,
               memoryDc_,
               &source,
               0,
               &blend,
               ULW_ALPHA) != FALSE;
}

}  // namespace wavebar