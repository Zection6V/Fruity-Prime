#pragma once

#include <cstdint>

namespace MphRead::Mods::Input
{
    // When 02029778 runs. The DS game ticks its players at 30 Hz and Fruity
    // simulates at 60, so a native touch tick is every other simulation step.
    // Advancing the producer on every step would halve the time its four
    // samples span (66.7 ms instead of 133.3 ms) and with it the distance a
    // swipe covers inside the >90 window -- a swipe would need twice the
    // speed. Between ticks the state is held, so both 60 Hz substeps read
    // the same native sample. NativeTouchSample distributes half its roll
    // impulse to each substep and the boost consumer latches the branch;
    // holding the producer state alone does not adapt its consumption.
    class NativeTouchClock final
    {
    public:
        static constexpr std::int32_t SimulationStepsPerTick = 2;

        // One simulation step. True when the producer ticks on this one; the
        // first step after a reset is a tick, so a contact is never late.
        [[nodiscard]] bool Advance() noexcept
        {
            const bool tick = _phase == 0;
            _phase = (_phase + 1) % SimulationStepsPerTick;
            return tick;
        }

        void Reset() noexcept { _phase = 0; }

    private:
        std::int32_t _phase = 0;
    };
}
