#include "FrameTimingCheck.hpp"
#include "LockjawTrailNoise.hpp"
#include "../../Utility/Rng.hpp"
#include "../../NativeRuntime/System/Random.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <memory>
#include <sstream>
#include <string>

#include "FrameTiming.hpp"
#include "NativeRuntime/System/Globalization.hpp"

namespace MphRead::Mods::Render
{

    class FrameTimingCheck::Case final
    {
    public:
        Case() = default;
        Case(const Case&) = delete;
        Case& operator=(const Case&) = delete;
        Case(Case&&) = delete;
        Case& operator=(Case&&) = delete;

        std::string Name = "";
        double Seconds = 0.0;
        std::function<double(std::int32_t)> FrameTime{};
        double ExpectedStepsPerSecond = FrameTiming::SimulationHz;
        double TolerancePercent = 0.5;
        std::int32_t MaxStepsInOneFrame = FrameTiming::MaxCatchUpSteps;
    };

    std::int32_t FrameTimingCheck::Run()
    {
        ::MphRead::NativeRuntime::Random rng(20260905);
        std::array<std::unique_ptr<Case>, 7> cases{};

        cases[0] = std::make_unique<Case>();
        cases[0]->Name = "60 Hz display";
        cases[0]->Seconds = 120.0;
        cases[0]->FrameTime = [](std::int32_t) { return 1.0 / 60.0; };
        cases[0]->MaxStepsInOneFrame = 1;

        cases[1] = std::make_unique<Case>();
        cases[1]->Name = "144 Hz display";
        cases[1]->Seconds = 120.0;
        cases[1]->FrameTime = [](std::int32_t) { return 1.0 / 144.0; };
        cases[1]->MaxStepsInOneFrame = 1;

        cases[2] = std::make_unique<Case>();
        cases[2]->Name = "240 Hz display";
        cases[2]->Seconds = 120.0;
        cases[2]->FrameTime = [](std::int32_t) { return 1.0 / 240.0; };
        cases[2]->MaxStepsInOneFrame = 1;

        cases[3] = std::make_unique<Case>();
        cases[3]->Name = "165 Hz display (not a multiple of 60)";
        cases[3]->Seconds = 300.0;
        cases[3]->FrameTime = [](std::int32_t) { return 1.0 / 165.0; };
        cases[3]->MaxStepsInOneFrame = 1;

        cases[4] = std::make_unique<Case>();
        cases[4]->Name = "40 Hz, a machine that cannot keep up";
        cases[4]->Seconds = 120.0;
        cases[4]->FrameTime = [](std::int32_t) { return 1.0 / 40.0; };
        cases[4]->MaxStepsInOneFrame = 2;

        cases[5] = std::make_unique<Case>();
        cases[5]->Name = "jittery 144 Hz";
        cases[5]->Seconds = 300.0;
        cases[5]->FrameTime = [&rng](std::int32_t)
        {
            return 1.0 / 144.0 * (0.4 + rng.NextDouble() * 1.2);
        };
        cases[5]->TolerancePercent = 1.0;
        cases[5]->MaxStepsInOneFrame = 3;

        cases[6] = std::make_unique<Case>();
        cases[6]->Name = "vsync flipping between 144 and 72";
        cases[6]->Seconds = 300.0;
        cases[6]->FrameTime = [](std::int32_t i)
        {
            return (i % 7 == 0 ? 2.0 : 1.0) / 144.0;
        };
        cases[6]->MaxStepsInOneFrame = 2;

        std::int32_t failures = 0;
        for (const std::unique_ptr<Case>& test : cases)
        {
            failures += RunCase(*test) ? 0 : 1;
        }
        failures += RunStallCase() ? 0 : 1;
        failures += RunLockjawNoiseCases();
        std::cout << (failures == 0
            ? "FRAMETIMING all cases pass"
            : "FRAMETIMING " + std::to_string(failures) + " case(s) FAILED")
            << '\n';
        return failures;
    }

    bool FrameTimingCheck::RunCase(const Case& test)
    {
        FrameTiming::Reset();
        FrameTiming::ResetDiagnostics();
        double elapsed = 0.0;
        std::int64_t steps = 0;
        std::int32_t worstFrame = 0;
        std::int32_t frame = 0;
        while (elapsed < test.Seconds)
        {
            const double dt = test.FrameTime(frame++);
            elapsed += dt;
            const std::int32_t taken = FrameTiming::Advance(dt);
            steps += taken;
            if (taken > worstFrame)
            {
                worstFrame = taken;
            }
        }
        const double rate = steps / elapsed;
        const double drift = std::abs(rate - test.ExpectedStepsPerSecond)
            / test.ExpectedStepsPerSecond * 100.0;
        const bool ok = drift <= test.TolerancePercent
            && worstFrame <= test.MaxStepsInOneFrame
            && FrameTiming::DroppedSteps() == 0;
        const double gameSeconds = steps * FrameTiming::StepSeconds;

        std::ostringstream line;
        line << "FRAMETIMING " << (ok ? "ok  " : "FAIL") << ' ' << test.Name
            << " | " << frame << " frames over " << ::MphRead::NativeRuntime::ToString(elapsed, "0.0")
            << " s"
            << " | " << steps << " steps = " << ::MphRead::NativeRuntime::ToString(rate, "0.000")
            << " Hz (drift " << ::MphRead::NativeRuntime::ToString(drift, "0.000") << "%)"
            << " | game ran " << ::MphRead::NativeRuntime::ToString(gameSeconds / elapsed, "0.0000")
            << "x real time"
            << " | worst frame " << worstFrame << " step(s)"
            << " | dropped " << FrameTiming::DroppedSteps();
        std::cout << line.str() << '\n';
        return ok;
    }

    bool FrameTimingCheck::RunStallCase()
    {
        FrameTiming::Reset();
        FrameTiming::ResetDiagnostics();
        std::int32_t worst = 0;
        for (std::int32_t i = 0; i < 600; i++)
        {
            worst = std::max(worst, FrameTiming::Advance(1.0 / 144.0));
        }
        const std::int32_t afterStall = FrameTiming::Advance(2.0);
        for (std::int32_t i = 0; i < 600; i++)
        {
            worst = std::max(worst, FrameTiming::Advance(1.0 / 144.0));
        }
        const bool ok = afterStall == 1 && worst <= 1 && FrameTiming::Stalls() == 1;

        std::cout << "FRAMETIMING " << (ok ? "ok  " : "FAIL") << " 2 s stall"
            << " | " << afterStall << " step(s) on the stalled frame"
            << " | " << FrameTiming::Stalls() << " stall(s) seen"
            << " | worst ordinary frame " << worst << " step(s)\n";
        return ok;
    }

    std::int32_t FrameTimingCheck::RunLockjawNoiseCases()
    {
        constexpr std::uint64_t tick = 100;
        constexpr std::int32_t segments = 10;
        constexpr std::int32_t axes = 3;
        std::array<float, segments * axes> firstRender{};
        const std::uint32_t rngBefore = ::MphRead::Rng::Rng1();
        bool repeatedRenderMatches = true;
        bool tickChanges = false;
        bool targetChanges = false;
        bool sourceChanges = false;
        bool ownerChanges = false;
        bool inBounds = true;
        for (std::int32_t render = 0; render < 4; render++)
        {
            for (std::int32_t segment = 0; segment < segments; segment++)
            {
                for (std::int32_t axis = 0; axis < axes; axis++)
                {
                    const float value = LockjawTrailNoise::Sample(tick, 0, 2, 0, segment, axis);
                    const auto index = static_cast<std::size_t>(segment * axes + axis);
                    if (render == 0)
                    {
                        firstRender[index] = value;
                    }
                    else if (value != firstRender[index])
                    {
                        repeatedRenderMatches = false;
                    }
                    tickChanges |= LockjawTrailNoise::Sample(tick + 1, 0, 2, 0, segment, axis) != value;
                    targetChanges |= LockjawTrailNoise::Sample(tick, 0, 2, 1, segment, axis) != value;
                    sourceChanges |= LockjawTrailNoise::Sample(tick, 0, 1, 0, segment, axis) != value;
                    ownerChanges |= LockjawTrailNoise::Sample(tick, 1, 2, 0, segment, axis) != value;
                }
            }
        }
        for (std::uint64_t sampleTick = 0; sampleTick < 128; sampleTick++)
        {
            for (std::int32_t owner = 0; owner < 2; owner++)
            {
                for (std::int32_t source = 1; source <= 2; source++)
                {
                    for (std::int32_t target = 0; target < source; target++)
                    {
                        for (std::int32_t segment = 0; segment < segments; segment++)
                        {
                            for (std::int32_t axis = 0; axis < axes; axis++)
                            {
                                const float value = LockjawTrailNoise::Sample(sampleTick, owner, source, target, segment, axis);
                                inBounds &= value >= -0.25F && value < 0.25F;
                            }
                        }
                    }
                }
            }
        }
        std::int32_t failures = 0;
        failures += ReportNoiseCase("repeated samples at one tick", repeatedRenderMatches);
        failures += ReportNoiseCase("next tick changes sequence", tickChanges);
        failures += ReportNoiseCase("different target changes sequence", targetChanges);
        failures += ReportNoiseCase("different source changes sequence", sourceChanges);
        failures += ReportNoiseCase("different owner changes sequence", ownerChanges);
        failures += ReportNoiseCase("offsets stay in [-0.25, 0.25)", inBounds);
        failures += ReportNoiseCase("global Rng1 unchanged", ::MphRead::Rng::Rng1() == rngBefore);
        return failures;
    }

    std::int32_t FrameTimingCheck::ReportNoiseCase(const std::string& name, bool passed)
    {
        std::cout << "FRAMETIMING " << (passed ? "ok  " : "FAIL") << " Lockjaw " << name << '\n';
        return passed ? 0 : 1;
    }
}
