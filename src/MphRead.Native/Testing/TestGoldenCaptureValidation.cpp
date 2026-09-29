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
