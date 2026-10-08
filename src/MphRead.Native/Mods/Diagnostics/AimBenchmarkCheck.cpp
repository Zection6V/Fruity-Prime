#include "AimCheck.hpp"
#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../NativeRuntime/FrameTelemetry.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>

namespace MphRead::Mods::Diagnostics
{
    void AimCheck::Benchmark(Entities::PlayerEntity& player)
    {
        namespace Telemetry = NativeRuntime::FrameTelemetry;
        using Clock = std::chrono::steady_clock;
        using Vector3 = OpenTK::Mathematics::Vector3;
        using Keys = OpenTK::Windowing::GraphicsLibraryFramework::Keys;
        constexpr std::size_t count = 10000;
        std::array<double, count> samples{};
        // This asset-backed diagnostic measures input capture plus the biped
        // driver. It is not a GPU run or a physical input latency measurement.
        for (int mode = 0; mode < 3; ++mode)
        {
            player.Controls().ClearAll(); player.ModForgetInputDeltas();
            player.Controls().SetMouseAim(true); player.Controls().SetKeyboardAim(true);
            player._flags1 &= ~Entities::PlayerFlags1::NoAimInput;
            player._gunVec1 = player._facingVector = Vector3(0, 0, 1);
            player._gunVec2 = Vector3(1, 0, 0); player._upVector = Vector3(0, 1, 0);
            player._aimY = 0; player._equipInfo->Zoomed = false;
            OpenTK::Windowing::GraphicsLibraryFramework::KeyboardState keyboard;
            OpenTK::Windowing::GraphicsLibraryFramework::MouseState mouse;
            const auto step = [&](std::size_t sequence)
            {
                // Alternating direction keeps the fixture in the same range.
                if (mode == 1) mouse.X = (sequence & 1U) ? 0.5F : 0.0F;
                keyboard.SetKeyDown(Keys::Left, mode == 2 && (sequence % 120U < 60U));
                Entities::PlayerEntity::ProcessInput(keyboard, mouse, false);
                player.ProcessBiped();
            };
            for (std::size_t i = 0; i < 1000; ++i) step(i);
            Telemetry::Session session(128);
            session.Eligible(true);
            for (std::size_t i = 0; i < count; ++i)
            {
                Telemetry::Frame frame;
                const auto start = Clock::now();
                {
                    Telemetry::Scope scope(Telemetry::Phase::Simulation);
                    step(i);
                }
                samples[i] = std::chrono::duration<double, std::micro>(Clock::now() - start).count();
                session.Rendered(); // Commit this diagnostic's measured iteration.
            }
            session.Eligible(false);
            std::sort(samples.begin(), samples.end());
            const auto& allocations = session.Statistics().Phases[static_cast<std::size_t>(Telemetry::Phase::Simulation)];
            std::cout << "AIM BENCH mode=" << (mode == 0 ? "idle" : mode == 1 ? "mouse" : "dual")
                << " iterations=" << count << " warmup=1000 p50_us=" << samples[count * 50 / 100 - 1]
                << " p95_us=" << samples[count * 95 / 100 - 1] << " p99_us=" << samples[count * 99 / 100 - 1]
                << " application_new_measured=" << Telemetry::HasNewCounters()
                << " application_new_calls=" << allocations.NewCalls
                << " application_new_bytes=" << allocations.NewBytes << '\n';
        }
        player.Controls().ClearAll(); player.ModForgetInputDeltas();
    }
}
