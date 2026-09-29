#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace MphRead::Mods::Render::GoldenCaptureValidation
{
    struct PixelSummary final
    {
        std::uint64_t Fnv1a64 = 0;
        std::size_t PixelCount = 0;
        std::size_t LitPixelCount = 0;
    };

    [[nodiscard]] inline std::size_t ExpectedRgbBytes(
        std::int32_t width,
        std::int32_t height)
    {
        if (width <= 0 || height <= 0)
        {
            throw std::invalid_argument(
                "golden capture RGB dimensions must be positive");
        }
        const std::size_t w = static_cast<std::size_t>(width);
        const std::size_t h = static_cast<std::size_t>(height);
        if (w > std::numeric_limits<std::size_t>::max() / h)
        {
            throw std::overflow_error(
                "golden capture RGB pixel count overflow");
        }
        const std::size_t pixels = w * h;
        if (pixels > std::numeric_limits<std::size_t>::max() / 3U)
        {
            throw std::overflow_error(
                "golden capture RGB byte count overflow");
        }
        return pixels * 3U;
    }

    [[nodiscard]] inline PixelSummary SummarizeRgb(
        const std::vector<std::uint8_t>& rgb,
        std::int32_t width,
        std::int32_t height)
    {
        const std::size_t expected = ExpectedRgbBytes(width, height);
        if (rgb.size() != expected)
        {
            throw std::invalid_argument(
                "golden capture RGB byte count does not match dimensions");
        }

        PixelSummary summary{};
        summary.Fnv1a64 = UINT64_C(14695981039346656037);
        summary.PixelCount = expected / 3U;
        for (std::size_t i = 0; i < expected; i += 3U)
        {
            if (rgb[i] > 8U || rgb[i + 1U] > 8U || rgb[i + 2U] > 8U)
            {
                ++summary.LitPixelCount;
            }
            summary.Fnv1a64 ^= rgb[i];
            summary.Fnv1a64 *= UINT64_C(1099511628211);
            summary.Fnv1a64 ^= rgb[i + 1U];
            summary.Fnv1a64 *= UINT64_C(1099511628211);
            summary.Fnv1a64 ^= rgb[i + 2U];
            summary.Fnv1a64 *= UINT64_C(1099511628211);
        }
        return summary;
    }

    [[nodiscard]] inline PixelSummary RequireMeaningfulRgb(
        const std::vector<std::uint8_t>& rgb,
        std::int32_t width,
        std::int32_t height)
    {
        const PixelSummary summary = SummarizeRgb(rgb, width, height);
        if (summary.LitPixelCount == 0)
        {
            throw std::runtime_error(
                "golden capture RGB buffer is entirely black");
        }
        return summary;
    }

    [[nodiscard]] inline std::size_t CountChangedPixels(
        const std::vector<std::uint8_t>& left,
        const std::vector<std::uint8_t>& right,
        std::int32_t width,
        std::int32_t height)
    {
        (void)SummarizeRgb(left, width, height);
        (void)SummarizeRgb(right, width, height);

        std::size_t changed = 0;
        for (std::size_t i = 0; i < left.size(); i += 3U)
        {
            if (left[i] != right[i]
                || left[i + 1U] != right[i + 1U]
                || left[i + 2U] != right[i + 2U])
            {
                ++changed;
            }
        }
        return changed;
    }

    [[nodiscard]] inline std::size_t RequireDistinctRgb(
        const std::vector<std::uint8_t>& control,
        const std::vector<std::uint8_t>& candidate,
        std::int32_t width,
        std::int32_t height)
    {
        (void)RequireMeaningfulRgb(control, width, height);
        (void)RequireMeaningfulRgb(candidate, width, height);
        const std::size_t changed
            = CountChangedPixels(control, candidate, width, height);
        if (changed == 0)
        {
            throw std::runtime_error(
                "golden capture candidate is RGB-identical to its control");
        }
        return changed;
    }

    inline void RequireDistinctFingerprints(
        const PixelSummary& left,
        const PixelSummary& right)
    {
        if (left.PixelCount == 0
            || right.PixelCount == 0
            || left.PixelCount != right.PixelCount)
        {
            throw std::invalid_argument(
                "golden capture fingerprints are not comparable");
        }
        if (left.Fnv1a64 == right.Fnv1a64)
        {
            throw std::runtime_error(
                "HUD and fade captures share the same raw-RGB fingerprint");
        }
    }
}
