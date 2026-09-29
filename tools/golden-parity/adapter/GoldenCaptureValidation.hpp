#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace MphRead::Mods::Render::GoldenCaptureValidation
{
    inline constexpr std::int32_t ManifestPhase = 4;
    inline constexpr std::string_view FixtureContract
        = "phase4-final-stage-v2";
    inline constexpr std::string_view ParityAdapterContract
        = "phase3-phase4-shared-v1";

    struct PixelSummary final
    {
        std::uint64_t Fnv1a64 = 0;
        std::size_t PixelCount = 0;
        std::size_t LitPixelCount = 0;
    };

    struct FadeFixtureTiming final
    {
        float Start = 0.0F;
        float Length = 0.0F;
    };

    struct FadeObservation final
    {
        bool UpdateObserved = false;
        float UpdatePercent = 0.0F;
        bool DrawObserved = false;
        std::int32_t DrawType = 0;
        float DrawColor = 0.0F;
        float DrawPercent = 0.0F;
        bool UpdateHookObserved = false;
        bool UpdatePostconditionVerified = false;
        bool DrawHookObserved = false;
        bool DrawRgbVerified = false;
    };

    [[nodiscard]] inline bool FadeValueMatches(
        float actual,
        float expected) noexcept
    {
        return std::isfinite(actual)
            && std::fabs(actual - expected) <= 0.00001F;
    }

    [[nodiscard]] inline FadeFixtureTiming MakeDeterministicFadeTiming(
        float globalElapsedTime,
        float targetPercent) noexcept
    {
        const float percent = std::clamp(targetPercent, 0.0F, 1.0F);
        if (globalElapsedTime > 0.0F && percent > 0.0F)
        {
            return FadeFixtureTiming{
                0.0F,
                globalElapsedTime / percent
            };
        }
        return FadeFixtureTiming{
            globalElapsedTime - percent,
            1.0F
        };
    }

    [[nodiscard]] inline float FadePercentAfterUpdate(
        float globalElapsedTime,
        const FadeFixtureTiming& timing)
    {
        if (!(timing.Length > 0.0F)
            || !std::isfinite(timing.Length)
            || !std::isfinite(timing.Start)
            || !std::isfinite(globalElapsedTime))
        {
            throw std::invalid_argument(
                "golden fade fixture timing is not finite and positive");
        }
        return (globalElapsedTime - timing.Start) / timing.Length;
    }

    inline void RequireExpectedFadeSequence(
        const FadeObservation& observation,
        std::int32_t expectedType,
        float expectedColor,
        float expectedPercent)
    {
        if (!observation.UpdateObserved)
        {
            throw std::runtime_error(
                "golden fade fixture was not observed by production UpdateFade");
        }
        if (!FadeValueMatches(observation.UpdatePercent, expectedPercent))
        {
            throw std::runtime_error(
                "production UpdateFade did not compute the requested fade percent");
        }
        if (!observation.DrawObserved)
        {
            throw std::runtime_error(
                "golden fade fixture did not reach the production fade draw");
        }
        if (observation.DrawType != expectedType
            || !FadeValueMatches(observation.DrawColor, expectedColor)
            || !FadeValueMatches(observation.DrawPercent, expectedPercent))
        {
            throw std::runtime_error(
                "production fade draw did not consume the requested fixture state");
        }
    }

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

    [[nodiscard]] inline std::size_t RequireHalfWhiteFadeRgb(
        const std::vector<std::uint8_t>& control,
        const std::vector<std::uint8_t>& candidate,
        std::int32_t width,
        std::int32_t height)
    {
        (void)RequireMeaningfulRgb(control, width, height);
        (void)RequireMeaningfulRgb(candidate, width, height);

        std::size_t changedPixels = 0;
        for (std::size_t i = 0; i < control.size(); i += 3U)
        {
            bool pixelChanged = false;
            for (std::size_t channel = 0; channel < 3U; ++channel)
            {
                // RttFragmentShader emits vec4(1,1,1,0.5) for this fixture,
                // and both revisions retain SrcAlpha/OneMinusSrcAlpha through
                // the final stage. An 8-bit target may choose either adjacent
                // integer when the exact half lands between them.
                const std::uint16_t sum
                    = static_cast<std::uint16_t>(control[i + channel])
                    + UINT16_C(255);
                const std::uint8_t low
                    = static_cast<std::uint8_t>(sum / 2U);
                const std::uint8_t high
                    = static_cast<std::uint8_t>((sum + 1U) / 2U);
                const std::uint8_t actual = candidate[i + channel];
                if (actual != low && actual != high)
                {
                    throw std::runtime_error(
                        "golden fade output is not the expected 50% white "
                        "fullscreen blend of its same-update control");
                }
                pixelChanged = pixelChanged
                    || actual != control[i + channel];
            }
            if (pixelChanged)
            {
                ++changedPixels;
            }
        }
        if (changedPixels == 0)
        {
            throw std::runtime_error(
                "golden fade output did not change any control pixels");
        }
        return changedPixels;
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
