#include "FrameTimingCheck.hpp"

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

namespace MphRead::Mods::Render
{
    namespace
    {
        class DotNetRandom final
        {
        public:
            explicit DotNetRandom(std::int32_t seed) noexcept
            {
                const std::int32_t subtraction = seed == std::numeric_limits<std::int32_t>::min()
                    ? std::numeric_limits<std::int32_t>::max()
                    : (seed < 0 ? -seed : seed);
                std::int32_t mj = 161803398 - subtraction;
                _seedArray[55] = mj;
                std::int32_t mk = 1;

                std::int32_t ii = 0;
                for (std::int32_t i = 1; i < 55; i++)
                {
                    if ((ii += 21) >= 55)
                    {
                        ii -= 55;
                    }

                    _seedArray[static_cast<std::size_t>(ii)] = mk;
                    mk = mj - mk;
                    if (mk < 0)
                    {
                        mk += std::numeric_limits<std::int32_t>::max();
                    }

                    mj = _seedArray[static_cast<std::size_t>(ii)];
                }

                for (std::int32_t k = 1; k < 5; k++)
                {
                    for (std::int32_t i = 1; i < 56; i++)
                    {
                        std::int32_t n = i + 30;
                        if (n >= 55)
                        {
                            n -= 55;
                        }

                        _seedArray[static_cast<std::size_t>(i)]
                            -= _seedArray[static_cast<std::size_t>(1 + n)];
                        if (_seedArray[static_cast<std::size_t>(i)] < 0)
                        {
                            _seedArray[static_cast<std::size_t>(i)]
                                += std::numeric_limits<std::int32_t>::max();
                        }
                    }
                }

                _inext = 0;
                _inextp = 21;
            }

            [[nodiscard]] double NextDouble() noexcept
            {
                return InternalSample()
                    * (1.0 / static_cast<double>(std::numeric_limits<std::int32_t>::max()));
            }

        private:
            [[nodiscard]] std::int32_t InternalSample() noexcept
            {
                std::int32_t locINext = _inext;
                if (++locINext >= 56)
                {
                    locINext = 1;
                }

                std::int32_t locINextp = _inextp;
                if (++locINextp >= 56)
                {
                    locINextp = 1;
                }

                std::int32_t retVal = _seedArray[static_cast<std::size_t>(locINext)]
                    - _seedArray[static_cast<std::size_t>(locINextp)];
                if (retVal == std::numeric_limits<std::int32_t>::max())
                {
                    retVal--;
                }
                if (retVal < 0)
                {
                    retVal += std::numeric_limits<std::int32_t>::max();
                }

                _seedArray[static_cast<std::size_t>(locINext)] = retVal;
                _inext = locINext;
                _inextp = locINextp;
                return retVal;
            }

            std::array<std::int32_t, 56> _seedArray{};
            std::int32_t _inext = 0;
            std::int32_t _inextp = 0;
        };

        [[nodiscard]] std::string FormatCurrentCultureFixed(double value, std::int32_t precision)
        {
            std::ostringstream formatted;
            try
            {
                // C# custom numeric interpolation uses CurrentCulture. The Native executable has no
                // CLR CultureInfo, so the narrow adapter is the process/user locale from the C++ runtime.
                formatted.imbue(std::locale(""));
            }
            catch (...)
            {
                // If the requested native locale is unavailable, preserve the stream's existing locale
                // rather than introducing a formatting-only failure that the C# interpolation cannot raise.
            }
            formatted << std::fixed << std::setprecision(precision) << value;
            return formatted.str();
        }
    }

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
        DotNetRandom rng(20260905);
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
            << " | " << frame << " frames over " << FormatCurrentCultureFixed(elapsed, 1)
            << " s"
            << " | " << steps << " steps = " << FormatCurrentCultureFixed(rate, 3)
            << " Hz (drift " << FormatCurrentCultureFixed(drift, 3) << "%)"
            << " | game ran " << FormatCurrentCultureFixed(gameSeconds / elapsed, 4)
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
}
