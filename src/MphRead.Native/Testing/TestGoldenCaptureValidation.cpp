#include "Mods/Render/GoldenCaptureValidation.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{
    void Fail(const char* message, int& failures)
    {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }

    template <typename TAction>
    void ExpectThrows(
        const char* message,
        TAction&& action,
        int& failures)
    {
        try
        {
            action();
            Fail(message, failures);
        }
        catch (const std::exception&)
        {
        }
    }
}

int main()
{
    using namespace MphRead::Mods::Render::GoldenCaptureValidation;

    int failures = 0;

    static_assert(
        ManifestPhase == 4,
        "GoldenCapture manifests must identify Phase 4 acceptance captures");
#if defined(FRUITY_GOLDEN_PARITY_ADAPTER_BUILD)
    static_assert(
        FixtureContract == std::string_view("phase4-final-stage-v3"),
        "Golden parity adapter fixture contract must remain v3");
    static_assert(
        ParityAdapterContract == std::string_view("phase3-phase4-shared-v2"),
        "Golden parity adapter contract must remain shared-v2");
#else
    static_assert(
        FixtureContract == std::string_view("phase4-final-stage-v2"),
        "Canonical GoldenCapture fixture contract must remain v2");
#endif

    ExpectThrows(
        "empty RGB buffer must be rejected",
        []()
        {
            const std::vector<std::uint8_t> empty{};
            (void)RequireMeaningfulRgb(empty, 2, 2);
        },
        failures);

    ExpectThrows(
        "all-black RGB buffer must be rejected",
        []()
        {
            const std::vector<std::uint8_t> black(12, 0);
            (void)RequireMeaningfulRgb(black, 2, 2);
        },
        failures);

    const std::vector<std::uint8_t> hud{
        12, 24, 36,
        48, 60, 72,
        84, 96, 108,
        120, 132, 144
    };

    ExpectThrows(
        "RGB-identical HUD/fade buffers must be rejected",
        [&hud]()
        {
            (void)RequireDistinctRgb(hud, hud, 2, 2);
        },
        failures);

    std::vector<std::uint8_t> fade = hud;
    fade[0] = 13;
    const std::size_t changed
        = RequireDistinctRgb(hud, fade, 2, 2);
    if (changed != 1U)
    {
        Fail("one changed pixel must be counted exactly", failures);
    }

    const PixelSummary hudSummary = SummarizeRgb(hud, 2, 2);
    const PixelSummary fadeSummary = SummarizeRgb(fade, 2, 2);
    if (hudSummary.Fnv1a64 == fadeSummary.Fnv1a64)
    {
        Fail("different RGB fixtures must produce different fingerprints", failures);
    }
    if (hudSummary.PixelCount != 4U
        || hudSummary.LitPixelCount != 4U)
    {
        Fail("pixel summary must preserve dimensions and lit count", failures);
    }

    ExpectThrows(
        "identical capture fingerprints must be rejected",
        [&hudSummary]()
        {
            RequireDistinctFingerprints(hudSummary, hudSummary);
        },
        failures);

    try
    {
        RequireDistinctFingerprints(hudSummary, fadeSummary);
    }
    catch (const std::exception&)
    {
        Fail("different capture fingerprints must be accepted", failures);
    }

    const float globalElapsedTime = 0.25F;
    const float targetFadePercent = 0.5F;
    const FadeFixtureTiming fadeTiming
        = MakeDeterministicFadeTiming(
            globalElapsedTime,
            targetFadePercent);
    const float recomputedFadePercent
        = FadePercentAfterUpdate(globalElapsedTime, fadeTiming);
    if (!FadeValueMatches(recomputedFadePercent, targetFadePercent))
    {
        Fail(
            "injected fade timing must recompute to the requested percent",
            failures);
    }

    FadeObservation observation{};
    ExpectThrows(
        "fade sequence must require a production UpdateFade observation",
        [&observation]()
        {
            RequireExpectedFadeSequence(
                observation,
                3,
                1.0F,
                0.5F);
        },
        failures);

    observation.UpdateObserved = true;
    observation.UpdatePercent = recomputedFadePercent;
    ExpectThrows(
        "fade sequence must require the actual production draw",
        [&observation]()
        {
            RequireExpectedFadeSequence(
                observation,
                3,
                1.0F,
                0.5F);
        },
        failures);

    observation.DrawObserved = true;
    observation.DrawType = 3;
    observation.DrawColor = 1.0F;
    observation.DrawPercent = recomputedFadePercent;
    try
    {
        RequireExpectedFadeSequence(
            observation,
            3,
            1.0F,
            0.5F);
    }
    catch (const std::exception&)
    {
        Fail(
            "matching UpdateFade and draw observations must be accepted",
            failures);
    }

    FadeObservation staleDraw = observation;
    staleDraw.DrawPercent = 0.0F;
    ExpectThrows(
        "fade sequence must reject a draw-time percent overwritten after injection",
        [&staleDraw]()
        {
            RequireExpectedFadeSequence(
                staleDraw,
                3,
                1.0F,
                0.5F);
        },
        failures);

    ExpectThrows(
        "dimension/byte-count mismatch must be rejected",
        [&hud]()
        {
            (void)SummarizeRgb(hud, 3, 2);
        },
        failures);

    if (failures != 0)
    {
        std::cerr
            << "GoldenCapture validation tests failed: "
            << failures << '\n';
        return 1;
    }

    std::cout << "GoldenCapture validation tests passed\n";
    return 0;
}
