#pragma once

#include <cstdint>

namespace MphRead::Mods
{
    // The game's tick: how often the simulation steps. Read it from here and
    // from nowhere else. The picture is drawn at whatever rate the display
    // runs and shows the world between steps (Mods/Render/ScenePresentation),
    // so nothing about how smooth the game looks depends on this number.
    //
    // It is a definition, not a setting: the engine still counts much of its
    // time in steps of this length (timers, per-step rates, the wire and
    // demos), so changing it changes the game.
    struct SimulationRate final
    {
        SimulationRate() = delete;

        static constexpr std::int32_t Hz = 60;
        static constexpr float HzF = static_cast<float>(Hz);
        static constexpr double StepSeconds = 1.0 / Hz;
        static constexpr float StepSecondsF = 1.0F / HzF;
        // The DS's own frame: animations, effect spawn parity, touch samples
        // and homing steering run at it. The tick is a whole multiple of it.
        static constexpr std::int32_t DsHz = 30;
        static constexpr std::int32_t StepsPerDsFrame = Hz / DsHz;
        static_assert(Hz % DsHz == 0, "the tick must be a whole multiple of the DS's frame");
    };
}
