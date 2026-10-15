#pragma once

#include <cstdint>

namespace MphRead::Mods::Render
{
    // The time line animation ticks are counted on. The game advances its
    // integer animation frames at the DS's 30 Hz; this clock says when each
    // tick landed and how far a draw has run past the newest one, so the draw
    // can show the pose between two frames without the frames themselves
    // moving. The simulation's clock steps once per 60 Hz simulation step; a
    // menu preview owns one that steps once per 30 Hz tick.
    class AnimationClock final
    {
    public:
        // Ticks noted while one is alive are counted on this clock. Nests.
        class TickScope final
        {
        public:
            explicit TickScope(const AnimationClock& clock) noexcept;
            ~TickScope();
            TickScope(const TickScope&) = delete;
            TickScope& operator=(const TickScope&) = delete;

        private:
            const AnimationClock* _previous;
        };

        // A simulation step by default; a menu preview's clock passes its tick.
        AnimationClock() noexcept;
        explicit AnimationClock(double secondsPerStep) noexcept;

        void Step() noexcept;
        // How far into the next step the picture being drawn is, in [0, 1).
        void Phase(double value) noexcept;
        [[nodiscard]] double TickTime() const noexcept;
        [[nodiscard]] double DrawTime() const noexcept;
        [[nodiscard]] double SecondsPerStep() const noexcept;

        [[nodiscard]] static const AnimationClock* Ticking() noexcept;
        [[nodiscard]] static AnimationClock& Simulation() noexcept;

    private:
        double _secondsPerStep;
        std::uint64_t _steps = 0;
        double _phase = 0;
        static thread_local const AnimationClock* _ticking;
    };
}
