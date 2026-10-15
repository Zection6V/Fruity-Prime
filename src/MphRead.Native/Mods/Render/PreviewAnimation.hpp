#pragma once

#include "AnimationClock.hpp"

#include <chrono>
#include <optional>

namespace MphRead
{
    class ModelInstance;
}

namespace MphRead::Mods::Render
{
    // The 30 Hz animation clock of a model shown outside a match. Integer
    // animation state still advances through the original UpdateAnimFrames
    // state machine; AnimationBlend draws between its ticks.
    class PreviewAnimation final
    {
    public:
        void Update(ModelInstance& instance);
        void Advance(ModelInstance& instance, double seconds);
        void Reset() noexcept;
        [[nodiscard]] const AnimationClock& Clock() const noexcept;

    private:
        AnimationClock _clock{};
        double _phase = 0;
        std::optional<std::chrono::steady_clock::time_point> _last{};
    };
}
